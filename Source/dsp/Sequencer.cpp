#include "Sequencer.h"

#include <algorithm>
#include <cmath>

namespace sb64::seq
{
    void Sequencer::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        running = holding = false;
        position = -1;
        swingTick = -1;
        gateCountdown = -1.0;
    }

    void Sequencer::stop (dsp::Voice& voice) noexcept
    {
        if (holding || gateCountdown >= 0.0)
            voice.noteOff();

        running = holding = slideNext = false;
        gateCountdown = -1.0;
        swingTick = -1;
        progress = 0.0f;
        expectedGrid = -1.0;
    }

    void Sequencer::process (dsp::Voice& voice, PatternSource& source, const Clock& clock) noexcept
    {
        const auto waiting = ! clock.run || (clock.host && ! clock.hostPlaying);

        if (waiting)
        {
            if (running)
                stop (voice);
            return;
        }

        const auto bpm = std::clamp (clock.bpm, 1.0, 1000.0);
        swing = std::clamp (clock.swing, 0.0f, 0.5f);

        if (running && clock.host != hostClock)
            running = false;   // switching the clock source starts over

        hostClock = clock.host;
        stepSamples = sampleRate * 60.0 / (bpm * 4.0);
        const auto stepsPerSample = 1.0 / stepSamples;

        if (clock.host)
        {
            // 16ths on the host's grid: the step follows the bar position (loops, locates)
            const auto grid = clock.ppq * 4.0;

            if (! running || std::abs (grid - expectedGrid) > 0.5)
            {
                lastHostTick = (int64_t) std::ceil (grid - 1.0e-6) - 1;
                running = true;
            }

            expectedGrid = grid + stepsPerSample;
            const auto n = (int64_t) std::floor (grid + 1.0e-6);
            progress = (float) std::clamp (grid - (double) n, 0.0, 1.0);

            if (n > lastHostTick)
            {
                lastHostTick = n;
                tick (voice, source, n, (n & 1) != 0);
            }
        }
        else
        {
            // counted in steps, so a tempo change acts on the step already under way
            if (! running)
            {
                running = true;
                stepsToNext = 0.0;
                internalTicks = 0;
            }

            if (stepsToNext < stepsPerSample)
            {
                const auto n = internalTicks++;
                stepsToNext += 1.0;
                tick (voice, source, n, (n & 1) != 0);
            }

            stepsToNext -= stepsPerSample;
            progress = (float) std::clamp (1.0 - stepsToNext, 0.0, 1.0);
        }

        if (swingTick >= 0 && --swingCountdown <= 0.0)
            playStep (voice, source, std::exchange (swingTick, -1));

        if (gateCountdown >= 0.0 && --gateCountdown < 0.0)
        {
            voice.noteOff();
            holding = false;
        }
    }

    void Sequencer::tick (dsp::Voice& voice, PatternSource& source, int64_t gridIndex, bool offbeat) noexcept
    {
        // a waiting offbeat always plays before the next step
        if (swingTick >= 0)
            playStep (voice, source, std::exchange (swingTick, -1));

        const auto delay = (double) swing * stepSamples;

        if (offbeat && delay >= 1.0)
        {
            swingTick = gridIndex;
            swingCountdown = delay;
        }
        else
        {
            playStep (voice, source, gridIndex);
        }
    }

    void Sequencer::playStep (dsp::Voice& voice, PatternSource& source, int64_t gridIndex) noexcept
    {
        auto* pattern = &source.playing();

        // host clock: the step follows the grid; internal clock: simply the next one
        auto nextStep = [&]
        {
            const auto first = pattern->first();
            const auto length = (int64_t) pattern->length();
            return hostClock ? first + (int) (((gridIndex % length) + length) % length)
                             : (position < first || position >= pattern->last() ? first : position + 1);
        };

        auto next = nextStep();

        if (next == pattern->first())
        {
            pattern = &source.startingOver();   // a queued pattern starts here
            next = nextStep();
        }

        position = next;
        const auto step = pattern->get (next);

        if (! step.on)
        {
            if (holding)
                voice.noteOff();

            holding = slideNext = false;
            gateCountdown = -1.0;
            return;
        }

        // a note held from the step before slides (or, at gate 100 %, joins without a new attack)
        const auto transition = holding ? (slideNext ? dsp::Transition::slide : dsp::Transition::legato)
                                        : dsp::Transition::retrigger;
        voice.noteOn ((float) step.note, step.accent, transition, (float) (stepSamples / sampleRate));

        holding = true;
        slideNext = step.slide;
        gateCountdown = step.slide || step.gate >= maxGate ? -1.0 : (double) step.gate * 0.01 * stepSamples;
    }
}
