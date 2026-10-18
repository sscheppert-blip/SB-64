#pragma once

#include "Pattern.h"
#include "Voice.h"

#include <cstdint>

// The step sequencer: a 16th-note clock (its own tempo or locked to the host's beat position) that
// plays the pattern's steps on the voice – gate length, accent, slide into the next step and gate
// 100 % as legato – with swing on every second 16th. Runs one host sample at a time. JUCE-free.
namespace sb64::seq
{
    struct Clock
    {
        bool run = false;          // the sequencer's own PLAY/STOP
        bool host = false;         // follow the host's beat position
        bool hostPlaying = false;
        double bpm = 120.0;
        double ppq = 0.0;          // host position at this sample (host clock only)
        float swing = 0.0f;        // 0 .. 0.5 of a step for the offbeats
    };

    class Sequencer
    {
    public:
        void prepare (double sampleRate) noexcept;

        // one host sample: clocks, plays steps and ends gates
        void process (dsp::Voice&, PatternSource&, const Clock&) noexcept;

        // lets go of the voice and waits for the next start
        void stop (dsp::Voice&) noexcept;

        // the next step played is the pattern's first
        void reset() noexcept { position = -1; }

        bool isRunning() const noexcept { return running; }
        int getPosition() const noexcept { return position; }   // step playing, -1 = none yet
        float getProgress() const noexcept { return progress; } // 0..1 through the current step

    private:
        void tick (dsp::Voice&, PatternSource&, int64_t gridIndex, bool offbeat) noexcept;
        void playStep (dsp::Voice&, PatternSource&, int64_t gridIndex) noexcept;

        double sampleRate = 48000.0, stepSamples = 6000.0;
        bool running = false, hostClock = false;
        float swing = 0.0f;

        // internal clock
        double stepsToNext = 0.0;
        int64_t internalTicks = 0;

        // host clock
        int64_t lastHostTick = -1;
        double expectedGrid = -1.0;

        int position = -1;
        float progress = 0.0f;

        // an offbeat waiting for its swing delay
        int64_t swingTick = -1;
        double swingCountdown = 0.0;

        // the note still sounding: its gate ends after gateCountdown samples (< 0 = held into the next step)
        bool holding = false, slideNext = false;
        double gateCountdown = -1.0;
    };
}
