#pragma once

#include "Pattern.h"
#include "Scales.h"

#include <cstdint>

// The random pattern generator (RND PATT): fills the steps between the pattern's first and last step
// with notes of the scale and random gates, accents, slides and cutoff offsets. JUCE-free.
namespace sb64::seq
{
    struct RandomSettings
    {
        int density = 70;     // percent of the steps that play
        int accent = 25;      // percent of the playing steps with an accent
        int slide = 20;       // ... with a slide
        int octaves = 1;      // note range above the base note, 1..3
        int mod = 0;          // cutoff offsets up to ± this (0..64)
        int pitchEnv = 0;     // percent of the playing steps with a random pitch envelope
        int chance = 0;       // ... with a random probability or pass condition
        int gateLow = 30, gateHigh = 70;   // gate range, percent
        int scale = 0, root = 0;
        const UserScales* users = nullptr;
        int baseNote = 36;    // C2
    };

    void randomise (Pattern&, const RandomSettings&, uint32_t seed) noexcept;
}
