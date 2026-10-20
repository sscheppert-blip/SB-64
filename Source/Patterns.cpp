#include "Patterns.h"

namespace sb64
{
    namespace
    {
        constexpr int digitsPerStep = (seq::Step::packedBits + 3) / 4, legacyDigits = 5;
    }

    Patterns::Patterns()
    {
        fromTree ({});
    }

    void Patterns::select (int index, bool whenPlaying) noexcept
    {
        index = juce::jlimit (0, numPatterns - 1, index);
        chainSize = 0;
        chainPosition = 0;

        if (whenPlaying && index != current.load())
        {
            queued = index;
        }
        else
        {
            queued = -1;
            current = index;
        }
    }

    void Patterns::addToChain (int index) noexcept
    {
        index = juce::jlimit (0, numPatterns - 1, index);
        auto size = chainSize.load();

        if (size >= maxChain)
            return;

        if (size == 0)
        {
            // the chain starts with the selected pattern
            chain[0] = selectedIndex();
            chainPosition = 0;
            size = 1;
        }

        chain[(size_t) size] = index;
        chainSize = size + 1;
    }

    seq::Pattern& Patterns::startingOver (bool afterPass) noexcept
    {
        if (const auto next = queued.exchange (-1); next >= 0)
        {
            current = next;
        }
        else if (const auto size = chainSize.load(); size > 1 && afterPass)
        {
            const auto position = (chainPosition.load() + 1) % size;
            chainPosition = position;
            current = chain[(size_t) position].load();
        }

        return playing();
    }

    juce::String Patterns::name (int index)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + index / patternsPerBank)) + juce::String (index % patternsPerBank + 1);
    }

    juce::String Patterns::stepsToString (const seq::Pattern& p)
    {
        // 64 steps, eleven hex digits each
        juce::String text;
        text.preallocateBytes (seq::maxSteps * digitsPerStep);

        for (int i = 0; i < seq::maxSteps; ++i)
            text << juce::String::toHexString ((juce::int64) p.get (i).pack()).paddedLeft ('0', digitsPerStep);

        return text;
    }

    void Patterns::stepsFromString (seq::Pattern& p, const juce::String& text)
    {
        // patterns saved before the step tracks had five digits per step (note, gate, on, accent, slide)
        const auto digits = text.length() >= seq::maxSteps * digitsPerStep ? digitsPerStep : legacyDigits;

        for (int i = 0; i < seq::maxSteps; ++i)
        {
            const auto field = text.substring (i * digits, i * digits + digits);

            if (field.length() != digits)
            {
                p.set (i, seq::Step {});
                continue;
            }

            auto step = seq::Step::unpack ((uint64_t) field.getHexValue64());

            if (digits == legacyDigits)
                step.pitchDecay = seq::Step {}.pitchDecay;

            p.set (i, step);
        }
    }

    juce::ValueTree Patterns::toTree() const
    {
        juce::ValueTree tree ("PATTERNS");
        tree.setProperty ("current", current.load(), nullptr);

        for (int i = 0; i < numPatterns; ++i)
        {
            const auto& p = patterns[(size_t) i];

            if (p.isEmpty())
                continue;

            juce::ValueTree child ("PATTERN");
            child.setProperty ("index", i, nullptr);
            child.setProperty ("first", p.first(), nullptr);
            child.setProperty ("last", p.last(), nullptr);
            child.setProperty ("steps", stepsToString (p), nullptr);
            tree.appendChild (child, nullptr);
        }

        return tree;
    }

    void Patterns::fromTree (const juce::ValueTree& tree)
    {
        for (auto& p : patterns)
            p.clear();

        queued = -1;
        chainSize = 0;

        if (! tree.isValid())
        {
            current = 0;
            writeDemo (patterns[0]);
            return;
        }

        for (const auto& child : tree)
        {
            if (! child.hasType ("PATTERN"))
                continue;

            const auto index = (int) child.getProperty ("index", -1);

            if (index < 0 || index >= numPatterns)
                continue;

            auto& p = patterns[(size_t) index];
            stepsFromString (p, child.getProperty ("steps").toString());
            p.setLast ((int) child.getProperty ("last", seq::stepsPerBar - 1));
            p.setFirst ((int) child.getProperty ("first", 0));
        }

        current = juce::jlimit (0, numPatterns - 1, (int) tree.getProperty ("current", 0));
    }

    void Patterns::writeDemo (seq::Pattern& p)
    {
        // note (0 = rest), accent, slide
        struct Note { int note; bool accent, slide; };
        static constexpr Note line[] {
            { 36, true, false }, { 36, false, false }, { 48, false, true }, { 36, false, false },
            { 0, false, false }, { 39, true, false },  { 36, false, false }, { 46, false, true },
            { 48, false, false }, { 36, false, false }, { 0, false, false }, { 43, true, true },
            { 41, false, false }, { 36, true, false },  { 39, false, true }, { 36, false, false },
        };

        p.clear();

        for (int i = 0; i < seq::stepsPerBar; ++i)
        {
            seq::Step s;
            s.on = line[i].note > 0;
            s.note = s.on ? line[i].note : 48;
            s.accent = line[i].accent;
            s.slide = line[i].slide;
            p.set (i, s);
        }
    }
}
