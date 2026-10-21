#pragma once

#include "NoteListener.h"
#include "Scales.h"
#include "Voice.h"

#include <array>
#include <cstdint>
#include <vector>

// The arpeggiator: plays the held notes (or the scale from the lowest held note) one after the other
// in 16ths at the sequencer's tempo, over 1..3 octaves, with a rhythm pattern and its own gate length.
// Starts with the first key. JUCE-free.
namespace sb64::seq
{
    enum class ArpDirection { up, down, upDown, random };
    inline constexpr int numArpRhythms = 6;
    inline constexpr const char* arpDirectionNames[] { "UP", "DN", "UD", "RND" };

    struct ArpSettings
    {
        ArpDirection direction = ArpDirection::up;
        int octaves = 1;          // 1..3
        int gate = 50;            // percent of a 16th
        int rhythm = 0;           // 0 .. numArpRhythms - 1
        enum class Source { keys, scale, pattern };
        Source source = Source::keys;   // the held notes, the scale from the lowest held note, or the
                                        // pattern's notes in step order (moved by the lowest key − C3)
        int scale = 0;
        const int* patternNotes = nullptr;
        int numPatternNotes = 0;
        const UserScales* users = nullptr;
    };

    class Arp
    {
    public:
        void prepare (double sampleRate) noexcept;

        // held notes, in the order they were pressed
        void setHeld (const std::vector<int>& keys) noexcept;

        // one host sample; transpose is added to every note. Returns the note it started, else -1.
        int process (dsp::Voice&, const ArpSettings&, double bpm, int transpose) noexcept;

        // lets go of the voice
        void stop (dsp::Voice&) noexcept;

        bool isPlaying() const noexcept { return active; }
        void setListener (NoteListener* l) noexcept { listener = l; }

        // for the tests: the notes of one pass
        static std::vector<int> sequence (const std::vector<int>& held, const ArpSettings&);

    private:
        // up to 128 notes over three octaves, up and down
        using Notes = std::array<int, 128 * 3 * 2>;
        static int build (const int* held, int count, const ArpSettings&, Notes& out) noexcept;

        Notes notes {};
        NoteListener* listener = nullptr;
        double sampleRate = 48000.0, samplesToNext = 0.0, gateCountdown = -1.0;
        bool active = false, sounding = false;
        int64_t tick = 0, played = 0;
        uint32_t randomState = 0x9e3779b9u;
        std::array<int, 128> held {};
        int numHeld = 0;
    };
}
