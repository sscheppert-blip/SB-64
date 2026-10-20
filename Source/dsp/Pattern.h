#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

// A pattern: up to 64 steps (four bars of 16) with a first and a last step. The panel edits it on the
// message thread while the sequencer reads it on the audio thread, so every step is one atomic word.
// JUCE-free.
namespace sb64::seq
{
    inline constexpr int maxSteps = 64, stepsPerBar = 16, numBars = maxSteps / stepsPerBar;
    inline constexpr int lowestNote = 12, highestNote = 84;   // C0 .. C6
    inline constexpr int minGate = 10, maxGate = 100;           // percent of the step

    inline constexpr int minMod = -64, maxMod = 63;             // cutoff offset per step
    inline constexpr int maxPitchEnv = 9;                       // pitch envelope depth and decay 0..9
    inline constexpr int maxCent = 50;                          // fine tune per step, ± cents

    // when a step plays: always, with a probability, or on some passes of every eight
    inline constexpr int numChances = 13;
    inline constexpr int chancePercent[6] { 100, 10, 25, 50, 75, 90 };   // chance 0..5
    inline constexpr int firstRatio = 6;                                 // chance 6..12 = 1/8 .. 7/8

    struct Step
    {
        int note = 48;   // C3
        int gate = 50;   // percent
        bool on = false, accent = false, slide = false;
        int mod = 0;            // minMod .. maxMod
        int pitchDepth = 0;     // 0 = the menu's pitch envelope, 1..9 = this step's
        int pitchDecay = 4;     // 0..9
        int cent = 0;           // ±maxCent
        int chance = 0;         // 0 .. numChances - 1

        bool operator== (const Step&) const = default;

        // bits: note 0-6, gate 7-13, on 14, accent 15, slide 16, mod 17-23, pitch depth 24-27,
        // pitch decay 28-31, cent 32-38, chance 39-42 (mod and cent as 7-bit two's complement)
        static constexpr int packedBits = 43;

        uint64_t pack() const noexcept
        {
            auto signed7 = [] (int v, int lo, int hi) { return (uint64_t) (std::clamp (v, lo, hi) & 127); };

            return (uint64_t) std::clamp (note, 0, 127)
                 | ((uint64_t) std::clamp (gate, 0, 127) << 7)
                 | ((uint64_t) on << 14) | ((uint64_t) accent << 15) | ((uint64_t) slide << 16)
                 | (signed7 (mod, minMod, maxMod) << 17)
                 | ((uint64_t) std::clamp (pitchDepth, 0, maxPitchEnv) << 24)
                 | ((uint64_t) std::clamp (pitchDecay, 0, maxPitchEnv) << 28)
                 | (signed7 (cent, -maxCent, maxCent) << 32)
                 | ((uint64_t) std::clamp (chance, 0, numChances - 1) << 39);
        }

        static Step unpack (uint64_t bits) noexcept
        {
            auto signed7 = [bits] (int shift) { const auto v = (int) ((bits >> shift) & 127); return v >= 64 ? v - 128 : v; };

            Step s;
            s.note = (int) (bits & 127);
            s.gate = std::clamp ((int) ((bits >> 7) & 127), minGate, maxGate);
            s.on = (bits >> 14) & 1;
            s.accent = (bits >> 15) & 1;
            s.slide = (bits >> 16) & 1;
            s.mod = signed7 (17);
            s.pitchDepth = std::min ((int) ((bits >> 24) & 15), maxPitchEnv);
            s.pitchDecay = std::min ((int) ((bits >> 28) & 15), maxPitchEnv);
            s.cent = std::clamp (signed7 (32), -maxCent, maxCent);
            s.chance = std::min ((int) ((bits >> 39) & 15), numChances - 1);
            return s;
        }
    };

    class Pattern
    {
    public:
        Pattern() { clear(); }

        Step get (int index) const noexcept
        {
            return Step::unpack (steps[(size_t) std::clamp (index, 0, maxSteps - 1)].load (std::memory_order_relaxed));
        }

        void set (int index, const Step& step) noexcept
        {
            if (index >= 0 && index < maxSteps)
                steps[(size_t) index].store (step.pack(), std::memory_order_relaxed);
        }

        int first() const noexcept { return firstStep.load (std::memory_order_relaxed); }
        int last() const noexcept  { return lastStep.load (std::memory_order_relaxed); }
        int length() const noexcept { return last() - first() + 1; }

        // keeps first <= last
        void setFirst (int index) noexcept
        {
            firstStep = std::clamp (index, 0, maxSteps - 1);
            if (last() < first()) lastStep = first();
        }

        void setLast (int index) noexcept
        {
            lastStep = std::clamp (index, 0, maxSteps - 1);
            if (first() > last()) firstStep = last();
        }

        void clear() noexcept
        {
            for (auto& s : steps)
                s.store (Step {}.pack(), std::memory_order_relaxed);
            firstStep = 0;
            lastStep = stepsPerBar - 1;
        }

        bool isEmpty() const noexcept
        {
            for (int i = 0; i < maxSteps; ++i)
                if (get (i) != Step {})
                    return false;
            return first() == 0 && last() == stepsPerBar - 1;
        }

        void copyFrom (const Pattern& other) noexcept
        {
            for (int i = 0; i < maxSteps; ++i)
                set (i, other.get (i));
            firstStep = other.first();
            lastStep = other.last();
        }

    private:
        std::array<std::atomic<uint64_t>, maxSteps> steps;
        std::atomic<int> firstStep { 0 }, lastStep { stepsPerBar - 1 };
    };

    // Where the sequencer gets its pattern from; a pattern change waits for the end of the pattern.
    class PatternSource
    {
    public:
        virtual ~PatternSource() = default;
        virtual Pattern& playing() noexcept = 0;
        // called when the sequence starts over: switch to a queued pattern now (or, after a full
        // pass, to the next pattern of a chain)
        virtual Pattern& startingOver (bool afterPass) noexcept = 0;
    };
}
