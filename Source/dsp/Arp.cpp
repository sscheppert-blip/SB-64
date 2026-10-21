#include "Arp.h"
#include "Pattern.h"
#include "Scales.h"

#include <algorithm>

namespace sb64::seq
{
    namespace
    {
        // eight 16ths each, a set bit plays
        constexpr unsigned rhythms[numArpRhythms] {
            0b11111111u,   // every 16th
            0b01010101u,   // 8ths
            0b11011011u,   // two on, one off
            0b01001001u,   // 3 + 3 + 2
            0b01110111u,   // three on, one off
            0b11011101u,   // one off, then three on
        };
    }

    void Arp::prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        active = sounding = false;
        gateCountdown = -1.0;
    }

    void Arp::setHeld (const std::vector<int>& keys) noexcept
    {
        numHeld = (int) std::min (keys.size(), held.size());
        std::copy (keys.begin(), keys.begin() + numHeld, held.begin());
    }

    int Arp::build (const int* heldNotes, int count, const ArpSettings& s, Notes& out) noexcept
    {
        // sorted, without repeats (no allocation: this runs on the audio thread)
        std::array<int, 128> base {};
        int* const b = base.data();
        int numBase = 0;

        for (int i = 0; i < std::min (count, (int) base.size()); ++i)
        {
            const auto n = heldNotes[i];
            int* pos = std::lower_bound (b, b + numBase, n);
            if (pos != b + numBase && *pos == n)
                continue;
            std::copy_backward (pos, b + numBase, b + numBase + 1);
            *pos = n;
            ++numBase;
        }

        if (numBase == 0)
            return 0;

        if (s.source == ArpSettings::Source::pattern && s.patternNotes != nullptr && s.numPatternNotes > 0)
        {
            // the pattern's notes in step order, moved by the lowest held key relative to C3
            const auto shift = base[0] - 48;
            numBase = 0;
            for (int i = 0; i < std::min (s.numPatternNotes, (int) base.size()); ++i)
                base[(size_t) numBase++] = std::clamp (s.patternNotes[i] + shift, 0, 127);
        }
        else if (s.source == ArpSettings::Source::scale)
        {
            // the scale from the lowest held note, one octave
            const auto root = base[0];
            numBase = 0;
            for (int n = root; n < root + 12; ++n)
                if (inScale (n, s.scale, root, s.users))
                    base[(size_t) numBase++] = n;
        }

        int size = 0;
        for (int o = 0; o < std::clamp (s.octaves, 1, 3); ++o)
            for (int i = 0; i < numBase; ++i)
                out[(size_t) size++] = base[(size_t) i] + 12 * o;

        if (s.direction == ArpDirection::down)
            std::reverse (out.begin(), out.begin() + size);

        if (s.direction == ArpDirection::upDown && size > 2)
            for (auto i = size - 2; i > 0; --i)
                out[(size_t) size++] = out[(size_t) i];

        return size;
    }

    std::vector<int> Arp::sequence (const std::vector<int>& heldNotes, const ArpSettings& s)
    {
        Notes notes {};
        const auto size = build (heldNotes.data(), (int) std::min (heldNotes.size(), (size_t) 128), s, notes);
        return { notes.begin(), notes.begin() + size };
    }

    void Arp::stop (dsp::Voice& voice) noexcept
    {
        if (sounding)
        {
            voice.noteOff();
            if (listener != nullptr)
                listener->noteEnded();
        }

        active = sounding = false;
        gateCountdown = -1.0;
    }

    int Arp::process (dsp::Voice& voice, const ArpSettings& s, double bpm, int transpose) noexcept
    {
        if (numHeld == 0)
        {
            if (active)
                stop (voice);
            return -1;
        }

        auto started = -1;

        const auto stepSamples = sampleRate * 60.0 / (std::clamp (bpm, 1.0, 1000.0) * 4.0);

        if (! active)
        {
            active = true;
            samplesToNext = 0.0;
            tick = played = 0;
        }

        if (samplesToNext <= 0.0)
        {
            samplesToNext += stepSamples;

            const auto size = build (held.data(), numHeld, s, notes);

            if (size > 0 && ((rhythms[std::clamp (s.rhythm, 0, numArpRhythms - 1)] >> (tick % 8)) & 1u))
            {
                auto index = (size_t) (played % (int64_t) std::max (size, 1));

                if (s.direction == ArpDirection::random)
                {
                    randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
                    index = randomState % (uint32_t) std::max (size, 1);
                }

                ++played;
                const auto note = std::clamp (notes[index] + transpose, 0, 127);
                voice.setStepPitchEnvelope (-1, 0.0f);
                if (sounding && listener != nullptr)
                    listener->noteEnded();

                voice.noteOn ((float) note, false, dsp::Transition::retrigger);
                started = note;

                if (listener != nullptr)
                    listener->noteStarted (note, false);
                sounding = true;
                gateCountdown = std::clamp (s.gate, minGate, maxGate) * 0.01 * stepSamples;
            }

            ++tick;
        }

        samplesToNext -= 1.0;

        if (gateCountdown >= 0.0 && --gateCountdown < 0.0 && sounding)
        {
            voice.noteOff();
            sounding = false;

            if (listener != nullptr)
                listener->noteEnded();
        }

        return started;
    }
}
