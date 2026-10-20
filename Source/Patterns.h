#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "dsp/Pattern.h"

// Pattern memory: 8 banks (A..H) × 16 patterns, saved with the plugin state. The pattern that plays is
// always the selected one; while the sequencer runs, a newly selected pattern waits for the end of the
// playing one.
namespace sb64
{
    class Patterns final : public seq::PatternSource
    {
    public:
        static constexpr int numBanks = 8, patternsPerBank = 16, numPatterns = numBanks * patternsPerBank;

        Patterns();

        seq::Pattern& pattern (int index) noexcept { return patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)]; }

        // the pattern being edited and played (or about to play, when one is queued)
        int selectedIndex() const noexcept { const auto q = queued.load(); return q >= 0 ? q : current.load(); }
        seq::Pattern& selected() noexcept { return pattern (selectedIndex()); }

        int playingIndex() const noexcept { return current.load(); }
        bool isQueued() const noexcept { return queued.load() >= 0; }

        // whenPlaying: wait for the end of the playing pattern; ends a chain
        void select (int index, bool whenPlaying) noexcept;

        // chain: after the selected pattern the next ones of the chain follow, pass by pass (not saved)
        static constexpr int maxChain = 16;
        void addToChain (int index) noexcept;
        int chainLength() const noexcept { return chainSize.load(); }
        int chainEntry (int i) const noexcept { return chain[(size_t) juce::jlimit (0, maxChain - 1, i)].load(); }

        // PatternSource (audio thread)
        seq::Pattern& playing() noexcept override { return pattern (current.load()); }
        seq::Pattern& startingOver (bool afterPass) noexcept override;

        // "A1" .. "H16"
        static juce::String name (int index);

        juce::ValueTree toTree() const;
        void fromTree (const juce::ValueTree&);   // invalid tree = factory state

        // the first pattern of a new instance: a short acid line
        static void writeDemo (seq::Pattern&);

        static juce::String stepsToString (const seq::Pattern&);
        static void stepsFromString (seq::Pattern&, const juce::String&);

    private:
        std::array<seq::Pattern, numPatterns> patterns;
        std::atomic<int> current { 0 }, queued { -1 };
        std::array<std::atomic<int>, maxChain> chain {};
        std::atomic<int> chainSize { 0 }, chainPosition { 0 };
    };
}
