#pragma once

#include <cstdint>

namespace sb64::dsp
{
    // In panel order. The last three wave shapes are unipolar (0..1), the others bipolar (-1..1).
    enum class LfoShape
    {
        sine, triangle, saw, ramp, square,
        tripleSaw,      // three falling ramps at 100, 60 and 30 %
        doubleSawTri,   // two falling ramps, then a triangle
        tripleRamp,     // three rising ramps at 30, 60 and 100 %
        sampleHold,     // a new random value every cycle
        noise           // random values eight times per cycle, smoothly joined
    };

    inline constexpr int numLfoShapes = 10;

    // The shape at phase 0..1 (wave shapes only; S&H and noise are produced by Lfo).
    float lfoWave (LfoShape, float phase) noexcept;

    class Lfo
    {
    public:
        void prepare (double sampleRate) noexcept;
        void setShape (LfoShape s) noexcept { shape = s; }
        void setFrequency (float hz) noexcept;

        // jumps to the given phase (retrigger, host sync)
        void setPhase (float phase) noexcept;
        float getPhase() const noexcept { return (float) phase; }

        // advances one sample and returns the new value
        float next() noexcept;

    private:
        float random() noexcept;

        LfoShape shape = LfoShape::sine;
        double sampleRate = 48000.0, phase = 0.0, increment = 0.0;
        double noisePhase = 0.0;
        float held = 0.0f, from = 0.0f, to = 0.0f;
        uint32_t seed = 0x2545f491u;
    };
}
