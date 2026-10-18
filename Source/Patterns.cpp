#include "Patterns.h"

namespace sb64
{
    Patterns::Patterns()
    {
        fromTree ({});
    }

    void Patterns::select (int index, bool whenPlaying) noexcept
    {
        index = juce::jlimit (0, numPatterns - 1, index);

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

    seq::Pattern& Patterns::startingOver() noexcept
    {
        if (const auto next = queued.exchange (-1); next >= 0)
            current = next;

        return playing();
    }

    juce::String Patterns::name (int index)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + index / patternsPerBank)) + juce::String (index % patternsPerBank + 1);
    }

    juce::String Patterns::stepsToString (const seq::Pattern& p)
    {
        // 64 steps, five hex digits each
        juce::String text;
        text.preallocateBytes (seq::maxSteps * 5);

        for (int i = 0; i < seq::maxSteps; ++i)
            text << juce::String::toHexString ((juce::int64) p.get (i).pack()).paddedLeft ('0', 5);

        return text;
    }

    void Patterns::stepsFromString (seq::Pattern& p, const juce::String& text)
    {
        for (int i = 0; i < seq::maxSteps; ++i)
        {
            const auto digits = text.substring (i * 5, i * 5 + 5);
            p.set (i, digits.length() == 5 ? seq::Step::unpack ((uint64_t) digits.getHexValue64()) : seq::Step {});
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
