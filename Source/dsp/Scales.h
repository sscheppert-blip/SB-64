#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

// Scales for the sequencer, the arpeggiator and the random generator: notes are moved down to the
// nearest note of the scale. Eight factory scales plus 16 user scales, whose notes can also be
// detuned by up to ±50 cents each (microtonal). JUCE-free.
namespace sb64::seq
{
    inline constexpr int numFactoryScales = 8, numUserScales = 16, numScales = numFactoryScales + numUserScales;
    inline constexpr int maxScaleCent = 50;

    // one bit per semitone above the root
    inline constexpr std::array<unsigned, numFactoryScales> scaleMasks {
        0xfffu,                                   // chromatic
        0b101010110101u,                          // major
        0b010110101101u,                          // natural minor
        0b001010010101u,                          // major pentatonic
        0b010010101001u,                          // minor pentatonic
        0b010101010101u,                          // whole tone
        0b001010011101u,                          // major blues
        0b010011101001u,                          // minor blues
    };

    inline constexpr const char* scaleNames[numFactoryScales] { "CHR", "MAJ", "MIN", "PMA", "PMI", "WHL", "BMA", "BMI" };

    inline bool isUserScale (int scale) noexcept { return scale >= numFactoryScales && scale < numScales; }

    // The user scales: which semitones above the root belong to the scale, and each one's detune.
    // Edited on the message thread, read on the audio thread.
    class UserScales
    {
    public:
        UserScales() { reset(); }

        unsigned mask (int user) const noexcept { return masks[index (user)].load (std::memory_order_relaxed); }
        int cents (int user, int degree) const noexcept { return detune[index (user)][(size_t) (degree % 12)].load (std::memory_order_relaxed); }

        // the root always stays in the scale
        void setMask (int user, unsigned m) noexcept { masks[index (user)].store ((m & 0xfffu) | 1u, std::memory_order_relaxed); }
        void setCents (int user, int degree, int c) noexcept
        {
            detune[index (user)][(size_t) (degree % 12)].store ((int8_t) std::clamp (c, -maxScaleCent, maxScaleCent), std::memory_order_relaxed);
        }

        bool isDefault (int user) const noexcept
        {
            for (int d = 0; d < 12; ++d)
                if (cents (user, d) != 0)
                    return false;
            return mask (user) == 0xfffu;
        }

        // every user scale chromatic and in tune
        void reset() noexcept
        {
            for (int u = 0; u < numUserScales; ++u)
            {
                setMask (u, 0xfffu);
                for (int d = 0; d < 12; ++d)
                    setCents (u, d, 0);
            }
        }

    private:
        static size_t index (int user) noexcept { return (size_t) std::clamp (user, 0, numUserScales - 1); }

        std::array<std::atomic<unsigned>, numUserScales> masks;
        std::array<std::array<std::atomic<int8_t>, 12>, numUserScales> detune;
    };

    inline int degreeOf (int note, int root) noexcept { return (((note - root) % 12) + 12) % 12; }

    inline unsigned scaleMask (int scale, const UserScales* users) noexcept
    {
        if (isUserScale (scale))
            return users != nullptr ? users->mask (scale - numFactoryScales) : 0xfffu;

        return scaleMasks[(size_t) (scale < 0 || scale >= numFactoryScales ? 0 : scale)];
    }

    inline bool inScale (int note, int scale, int root, const UserScales* users = nullptr) noexcept
    {
        return ((scaleMask (scale, users) >> degreeOf (note, root)) & 1u) != 0;
    }

    inline int quantise (int note, int scale, int root, const UserScales* users = nullptr) noexcept
    {
        for (int down = 0; down < 12; ++down)
            if (inScale (note - down, scale, root, users))
                return note - down;

        return note;
    }

    // the detune of a note of the scale, in semitones (user scales only)
    inline float tuning (int note, int scale, int root, const UserScales* users) noexcept
    {
        if (! isUserScale (scale) || users == nullptr)
            return 0.0f;

        return (float) users->cents (scale - numFactoryScales, degreeOf (note, root)) * 0.01f;
    }
}
