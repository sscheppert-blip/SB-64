#include "Lfo.h"

#include <cmath>

namespace sb64::dsp
{
    namespace
    {
        constexpr float pi = 3.14159265358979f;

        // a falling ramp from height to 0 over the part [start, start + width)
        float fallingRamp (float phase, float start, float width, float height) noexcept
        {
            return height * (1.0f - (phase - start) / width);
        }
    }

    float lfoWave (LfoShape shape, float p) noexcept
    {
        switch (shape)
        {
            case LfoShape::sine:      return std::sin (2.0f * pi * p);
            case LfoShape::triangle:  return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case LfoShape::saw:       return 1.0f - 2.0f * p;
            case LfoShape::ramp:      return 2.0f * p - 1.0f;
            case LfoShape::square:    return p < 0.5f ? 1.0f : -1.0f;

            case LfoShape::tripleSaw:
            {
                constexpr float third = 1.0f / 3.0f;
                if (p < third)        return fallingRamp (p, 0.0f, third, 1.0f);
                if (p < 2.0f * third) return fallingRamp (p, third, third, 0.6f);
                return fallingRamp (p, 2.0f * third, third, 0.3f);
            }

            case LfoShape::doubleSawTri:
            {
                if (p < 0.25f) return fallingRamp (p, 0.0f, 0.25f, 1.0f);
                if (p < 0.5f)  return fallingRamp (p, 0.25f, 0.25f, 1.0f);
                const auto t = (p - 0.5f) * 2.0f;   // triangle over the second half
                return t < 0.5f ? 2.0f * t : 2.0f - 2.0f * t;
            }

            case LfoShape::tripleRamp:
            {
                constexpr float third = 1.0f / 3.0f;
                if (p < third)        return 0.3f * p / third;
                if (p < 2.0f * third) return 0.6f * (p - third) / third;
                return (p - 2.0f * third) / third;
            }

            case LfoShape::sampleHold:
            case LfoShape::noise:
                break;
        }

        return 0.0f;
    }

    void Lfo::prepare (double rate) noexcept
    {
        sampleRate = rate;
    }

    void Lfo::setFrequency (float hz) noexcept
    {
        increment = (double) hz / sampleRate;
    }

    void Lfo::setPhase (float p) noexcept
    {
        phase = p - std::floor (p);
        noisePhase = 0.0;
        held = random();
        from = to;
        to = random();
    }

    float Lfo::random() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    float Lfo::next() noexcept
    {
        phase += increment;

        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = random();
        }

        switch (shape)
        {
            case LfoShape::sampleHold:
                return held;

            case LfoShape::noise:
            {
                // eight random points per cycle, joined with a smooth step
                noisePhase += increment * 8.0;

                if (noisePhase >= 1.0)
                {
                    noisePhase -= std::floor (noisePhase);
                    from = to;
                    to = random();
                }

                const auto t = (float) noisePhase;
                return from + (to - from) * t * t * (3.0f - 2.0f * t);
            }

            default:
                return lfoWave (shape, (float) phase);
        }
    }
}
