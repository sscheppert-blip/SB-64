#include "Generator.h"
#include "Scales.h"

#include <algorithm>

namespace sb64::seq
{
    namespace
    {
        struct Random
        {
            uint32_t state;
            uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
            int below (int n) noexcept { return n <= 0 ? 0 : (int) (next() % (uint32_t) n); }
            bool percent (int p) noexcept { return below (100) < p; }
        };
    }

    void randomise (Pattern& pattern, const RandomSettings& s, uint32_t seed) noexcept
    {
        Random random { seed == 0 ? 0x2545f491u : seed };

        // the notes to choose from; the root (and its octaves) comes up more often
        int notes[48] {};
        int count = 0;
        const auto base = s.baseNote + ((s.root % 12) + 12) % 12;

        for (int n = base; n <= base + 12 * std::clamp (s.octaves, 1, 3) && count < 48; ++n)
            if (inScale (n, s.scale, s.root, s.users) && n >= lowestNote && n <= highestNote)
                notes[count++] = n;

        for (int i = pattern.first(); i <= pattern.last(); ++i)
        {
            Step step;
            step.on = random.percent (std::clamp (s.density, 0, 100));

            if (step.on)
            {
                step.note = count > 0 && ! random.percent (30) ? notes[random.below (count)] : base;
                step.gate = 30 + 10 * random.below (5);
                step.accent = random.percent (s.accent);
                step.slide = random.percent (s.slide);
                const auto range = std::clamp (s.mod, 0, 64);
                step.mod = range > 0 ? std::clamp (random.below (2 * range + 1) - range, minMod, maxMod) : 0;
            }

            pattern.set (i, step);
        }
    }
}
