#include "Sequencer.h"
#include "Scales.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sb64::seq
{
    int passLength (PlayMode mode, int length) noexcept
    {
        length = std::max (1, length);

        switch (mode)
        {
            case PlayMode::pingPong:  return length > 1 ? 2 * length - 2 : 1;
            case PlayMode::pingPong2: return 2 * length;
            case PlayMode::forward:
            case PlayMode::backward:
            case PlayMode::random:    break;
        }

        return length;
    }

    int stepInPass (PlayMode mode, int first, int last, int k) noexcept
    {
        const auto length = last - first + 1;

        switch (mode)
        {
            case PlayMode::backward:  return last - k;
            case PlayMode::pingPong:  return k < length ? first + k : last - (k - length + 1);
            case PlayMode::pingPong2: return k < length ? first + k : last - (k - length);
            case PlayMode::forward:
            case PlayMode::random:    break;
        }

        return first + k;
    }

    bool chancePlays (int chance, int64_t pass, int random) noexcept
    {
        if (chance <= 0 || chance >= numChances)
            return true;

        if (chance < firstRatio)
            return random < chancePercent[chance];

        // n/8: the first n of every eight passes
        return (pass % 8) < (chance - firstRatio + 1);
    }

    void Sequencer::ended (dsp::Voice& voice) noexcept
    {
        voice.noteOff();

        if (listener != nullptr)
            listener->noteEnded();
    }

    int Sequencer::random (int n) noexcept
    {
        randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
        return n <= 0 ? 0 : (int) (randomState % (uint32_t) n);
    }
    void Sequencer::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        running = holding = false;
        reset();
        swingTick = -1;
        gateCountdown = -1.0;
    }

    void Sequencer::stop (dsp::Voice& voice) noexcept
    {
        if (holding || gateCountdown >= 0.0)
            ended (voice);

        voice.setCutoffOffset (0.0f);

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
            ended (voice);
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

        // where in the pass: host clock from the grid position, internal clock counted along
        int k;
        int64_t thisPass;

        auto locate = [&]
        {
            const auto cycle = (int64_t) passLength ((PlayMode) pattern->playMode(), pattern->length());

            if (hostClock)
            {
                k = (int) (((gridIndex % cycle) + cycle) % cycle);
                thisPass = (gridIndex - k) / cycle;
            }
            else
            {
                if (passStep < 0)
                {
                    passStep = 0;
                    pass = 0;
                }
                else if (++passStep >= cycle)
                {
                    passStep = 0;
                    ++pass;
                }

                k = passStep;
                thisPass = pass;
            }
        };

        locate();

        if (k == 0)
        {
            // a queued (or chained) pattern starts here
            auto* next = &source.startingOver (position >= 0);

            if (next != pattern)
            {
                pattern = next;

                if (! hostClock)
                    passStep = -1;

                locate();
            }
        }

        const auto mode = (PlayMode) pattern->playMode();
        position = mode == PlayMode::random ? pattern->first() + random (pattern->length())
                                             : std::clamp (stepInPass (mode, pattern->first(), pattern->last(), k),
                                                                   pattern->first(), pattern->last());
        const auto step = pattern->get (position);

        if (! step.on || ! chancePlays (step.chance, thisPass, random (100)))
        {
            if (holding)
                ended (voice);

            holding = slideNext = false;
            gateCountdown = -1.0;
            return;
        }

        const auto note = std::clamp (quantise (step.note + options.transpose, options.scale, options.root, options.users), 0, 127);
        const auto detune = tuning (note, options.scale, options.root, options.users);
        voice.setCutoffOffset ((float) step.mod / 64.0f * modOctaves);
        voice.setStepPitchEnvelope (step.pitchDepth > 0 ? step.pitchDepth : -1, (float) step.pitchDecay / (float) maxPitchEnv);

        // a note held from the step before slides (or, at gate 100 %, joins without a new attack)
        const auto transition = holding ? (slideNext ? dsp::Transition::slide : dsp::Transition::legato)
                                        : dsp::Transition::retrigger;
        voice.noteOn ((float) note + detune + (float) step.cent * 0.01f, step.accent, transition, (float) (stepSamples / sampleRate));

        if (listener != nullptr)
            listener->noteStarted (note, step.accent);

        holding = true;
        slideNext = step.slide;
        gateCountdown = step.slide || step.gate >= maxGate ? -1.0 : (double) step.gate * 0.01 * stepSamples;
    }
}
