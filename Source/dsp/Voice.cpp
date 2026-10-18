#include "Voice.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sb64::dsp
{
    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double twoPi = 6.283185307179586;

        // the square is slightly asymmetric, which gives it a little of the even harmonics
        constexpr float squareDuty = 0.484f;

        // detune ensemble: two delay lines, each swept by its own slow sine
        constexpr float ensembleBaseMs[2] { 6.0f, 9.5f };
        constexpr float ensembleRateHz[2] { 0.9f, 1.35f };
        constexpr float ensembleDepthMs = 1.8f;

        // filter integrators clip their input current at this level (slew limiting)
        constexpr float slewLimit = 1.5f, rail = 1.5f;

        // cutoff modulation ranges in octaves
        constexpr float envOctaves = 6.0f, accentOctaves = 1.0f, lfoOctaves = 3.0f;
        constexpr float fmOctaves = 3.0f;

        // accent: louder and with longer decays
        constexpr float normalLevel = 0.72f, accentDecayStretch = 1.5f;

        constexpr float ampAttackSeconds = 0.0015f, filterAttackSeconds = 0.0005f;

        // polyBLEP residual for a step at phase 0, t in [0, 1)
        float blep (float t, float dt) noexcept
        {
            if (t < dt)
            {
                t /= dt;
                return t + t - t * t - 1.0f;
            }

            if (t > 1.0f - dt)
            {
                t = (t - 1.0f) / dt;
                return t * t + t + t + 1.0f;
            }

            return 0.0f;
        }

        // smooth limiter, about tanh (x / limit) * limit
        float softClip (float x, float limit) noexcept
        {
            const auto u = std::clamp (x / limit, -3.0f, 3.0f);
            return limit * u * (27.0f + u * u) / (27.0f + 9.0f * u * u);
        }

        float decayCoefficient (float seconds, double sampleRate) noexcept
        {
            if (! std::isfinite (seconds))
                return 1.0f;

            // -60 dB after the given time
            return (float) std::exp (-6.907755 / (std::max (0.001, (double) seconds) * sampleRate));
        }
    }

    float cutoffHz (float knob) noexcept         { return 30.0f * std::exp2 (std::clamp (knob, 0.0f, 1.0f) * 9.2f); }
    float vcfDecaySeconds (float knob) noexcept  { return 0.03f * std::pow (100.0f, std::clamp (knob, 0.0f, 1.0f)); }
    float lfoHz (float knob) noexcept            { return 0.05f * std::exp2 (std::clamp (knob, 0.0f, 1.0f) * 15.3f); }
    float pitchEnvSeconds (float knob) noexcept  { return 0.01f * std::pow (30.0f, std::clamp (knob, 0.0f, 1.0f)); }

    float vcaDecaySeconds (float knob) noexcept
    {
        if (knob >= droneFrom)
            return std::numeric_limits<float>::infinity();

        return 0.04f * std::pow (125.0f, std::clamp (knob, 0.0f, 1.0f) / droneFrom);
    }

    Voice::Voice()
    {
        accentGain.value = accentGain.target = normalLevel;
    }

    void Voice::prepare (double rate)
    {
        sampleRate = rate;
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.005 * rate)));   // 5 ms

        delaySize = (int) std::ceil (rate * 0.025) + 4;
        delayLine.assign ((size_t) delaySize, 0.0f);
        delayWrite = 0;
        ensembleToneCoef = (float) (1.0 - std::exp (-twoPi * 6000.0 / rate));
        lowShelfCoef = (float) (1.0 - std::exp (-twoPi * 120.0 / rate));
        dcCoef = (float) std::exp (-twoPi * 8.0 / rate);

        lfo.prepare (rate);
        panic();
        setSettings (settings);

        for (auto* s : { &cutoffOct, &resonance, &subLevel, &noiseLevel, &detune, &drive, &fmDepth, &lfoLevel,
                         &envAmount, &volume, &accentGain, &accentOpen })
            s->jump();
    }

    void Voice::setSettings (const Settings& s) noexcept
    {
        settings = s;

        cutoffOct.target = std::log2 (cutoffHz (s.cutoff));
        resonance.target = std::clamp (s.resonance, 0.0f, 1.0f);
        subLevel.target = std::clamp (s.subLevel, 0.0f, 1.0f);
        noiseLevel.target = s.noiseLevel * s.noiseLevel;
        detune.target = std::clamp (s.detune, 0.0f, 1.0f);
        drive.target = std::clamp (s.drive, 0.0f, 1.0f);
        fmDepth.target = s.fmLevel * s.fmLevel * fmOctaves;
        lfoLevel.target = s.lfoLevel * s.lfoLevel * lfoOctaves;
        envAmount.target = s.envAmount * envOctaves;
        volume.target = s.volume * s.volume * 1.3f;

        lfo.setShape (s.lfoShape);
        lfo.setFrequency (s.lfoHz);
        updateTimes();
    }

    void Voice::updateTimes() noexcept
    {
        const auto stretch = accent ? accentDecayStretch : 1.0f;
        const auto vcaTime = vcaDecaySeconds (settings.vcaEnv) * stretch;

        vca.attackStep = (float) (1.0 / (ampAttackSeconds * sampleRate));
        vca.decayCoef = decayCoefficient (vcaTime, sampleRate);
        // after the gate the sound fades in a quarter of the decay time (a drone keeps going)
        vca.releaseCoef = decayCoefficient (std::isfinite (vcaTime) ? std::clamp (vcaTime * 0.25f, 0.008f, 1.5f) : vcaTime,
                                            sampleRate);

        vcf.attackStep = (float) (1.0 / (filterAttackSeconds * sampleRate));
        vcf.decayCoef = vcf.releaseCoef = decayCoefficient (vcfDecaySeconds (settings.vcfEnv) * stretch, sampleRate);

        pitchEnvCoef = decayCoefficient (pitchEnvSeconds (settings.pitchEnvDecay), sampleRate);
    }

    void Voice::noteOn (float newNote, bool withAccent, Transition transition, float glideSeconds) noexcept
    {
        accent = withAccent;
        accentGain.target = accent ? 1.0f : normalLevel;
        accentOpen.target = accent ? accentOctaves : 0.0f;
        updateTimes();

        if (gate && transition == Transition::slide)
        {
            // glide from where the pitch is now; the envelopes keep running
            slideFrom = note;
            slideTo = newNote;
            slidePos = 0.0f;
            const auto seconds = glideSeconds > 0.0f ? glideSeconds : settings.slideSeconds;
            slideStep = (float) (1.0 / (std::max (0.001f, seconds) * sampleRate));
            return;
        }

        if (gate && transition == Transition::legato)
        {
            // straight to the new pitch, without restarting the envelopes
            note = slideFrom = slideTo = newNote;
            slidePos = 1.0f;
            return;
        }

        note = slideFrom = slideTo = newNote;
        slidePos = 1.0f;
        gate = true;

        // the envelopes restart from their current level (no click)
        vca.stage = Envelope::Stage::attack;
        vcf.stage = Envelope::Stage::attack;
        pitchEnv = (float) std::clamp (settings.pitchEnvDepth, 0, 9) / 9.0f;

        if (settings.lfoReset)
            lfo.setPhase (settings.lfoStartPhase);
    }

    void Voice::noteOff() noexcept
    {
        gate = false;

        if (vca.stage != Envelope::Stage::idle)
            vca.stage = Envelope::Stage::release;
    }

    void Voice::panic() noexcept
    {
        gate = false;
        vca = {};
        vcf = {};
        updateTimes();
        pitchEnv = 0.0f;
        lp = bp = 0.0f;
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        lowShelf = dcIn = dcOut = ensembleTone = 0.0f;
    }

    float Voice::noise() noexcept
    {
        noiseSeed ^= noiseSeed << 13;
        noiseSeed ^= noiseSeed >> 17;
        noiseSeed ^= noiseSeed << 5;
        return (float) (int32_t) noiseSeed * (1.0f / 2147483648.0f);
    }

    float Voice::oscillator (float frequency) noexcept
    {
        const auto dt = frequency / (float) sampleRate;
        phase += dt;

        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            subHigh = ! subHigh;   // the sub is a flip-flop on the main oscillator: exactly one octave down
        }

        float wave = 0.0f;

        switch (settings.wave)
        {
            case Wave::saw:
                wave = 2.0f * phase - 1.0f - blep (phase, dt);
                break;

            case Wave::square:
            {
                auto fall = phase - squareDuty;
                if (fall < 0.0f) fall += 1.0f;
                wave = (phase < squareDuty ? 1.0f : -1.0f) + blep (phase, dt) - blep (fall, dt);
                break;
            }

            case Wave::triangle:
                wave = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                break;
        }

        // sub square: its own phase runs at half speed, made from the flip-flop state
        const auto subPhase = 0.5f * (phase + (subHigh ? 1.0f : 0.0f));
        auto subFall = subPhase - 0.5f;
        if (subFall < 0.0f) subFall += 1.0f;
        const auto sub = (subPhase < 0.5f ? 1.0f : -1.0f) + blep (subPhase, dt * 0.5f) - blep (subFall, dt * 0.5f);

        const auto osc = detuneEnsemble (wave);
        return 0.55f * osc + 0.55f * subLevel.value * sub;
    }

    float Voice::detuneEnsemble (float x) noexcept
    {
        delayLine[(size_t) delayWrite] = x;
        const auto amount = detune.value;
        float wet = 0.0f;

        if (amount > 0.0001f)
        {
            for (int i = 0; i < 2; ++i)
            {
                ensemblePhase[i] += ensembleRateHz[i] / sampleRate;
                if (ensemblePhase[i] >= 1.0) ensemblePhase[i] -= 1.0;

                const auto ms = ensembleBaseMs[i] + amount * ensembleDepthMs * (float) std::sin (twoPi * ensemblePhase[i]);
                auto read = (float) delayWrite - ms * 0.001f * (float) sampleRate;
                if (read < 0.0f) read += (float) delaySize;

                const auto i0 = (int) read;
                const auto frac = read - (float) i0;
                const auto a = delayLine[(size_t) i0];
                const auto b = delayLine[(size_t) ((i0 + 1) % delaySize)];
                wet += a + (b - a) * frac;
            }
        }

        if (++delayWrite >= delaySize)
            delayWrite = 0;

        // bucket-brigade lines are dark
        ensembleTone += (wet - ensembleTone) * ensembleToneCoef;
        const auto mix = amount * 0.8f;
        return (x + mix * ensembleTone) / (1.0f + 0.6f * mix);
    }

    float Voice::filter (float x, float frequency) noexcept
    {
        // Two integrators in a state-variable loop. Each integrator's input is limited like the
        // current of a programmable op-amp, so big signals slew instead of growing: a gritty edge when
        // driven and a square-ish self-oscillation at full resonance, while the low end stays.
        const auto f = 2.0f * std::sin (pi * std::min (frequency, 0.2f * (float) sampleRate) / (float) sampleRate);
        const auto res = resonance.value;
        const auto damping = 1.6f * (1.0f - res) - 0.08f * res;

        // the integrator outputs end at the supply rails: full resonance runs rail to rail
        lp = std::clamp (lp + f * softClip (bp, slewLimit), -rail, rail);
        const auto hp = x - lp - damping * bp;
        bp = std::clamp (bp + f * softClip (hp, slewLimit), -rail, rail);

        return settings.filter == FilterMode::lowPass ? lp : 1.2f * bp;
    }

    float Voice::overdrive (float x) noexcept
    {
        // the overdrive path lifts the lows before clipping, a little asymmetrically
        lowShelf += (x - lowShelf) * lowShelfCoef;
        const auto boosted = x + 1.2f * lowShelf;
        constexpr float bias = 0.2f;
        const auto od = 0.45f * (std::tanh (5.0f * boosted + bias) - std::tanh (bias));

        const auto mix = drive.value;
        return x * (1.0f - mix) + od * mix;
    }

    float Voice::renderSample() noexcept
    {
        for (auto* s : { &cutoffOct, &resonance, &subLevel, &noiseLevel, &detune, &drive, &fmDepth, &lfoLevel,
                         &envAmount, &volume, &accentGain, &accentOpen })
            s->step (smoothing);

        lfoValue = lfo.next();

        // pitch: slide along a fast-start, soft-landing curve
        if (slidePos < 1.0f)
        {
            slidePos = std::min (1.0f, slidePos + slideStep);
            const auto y = (1.0f - std::exp (-4.0f * slidePos)) / (1.0f - std::exp (-4.0f));
            note = slideFrom + (slideTo - slideFrom) * y;
        }

        pitchEnv *= pitchEnvCoef;
        const auto semitones = note + bend + pitchEnv * pitchEnvRange + fmDepth.value * 12.0f * lfoValue;
        const auto frequency = std::clamp ((440.0f + settings.tuneHz) * std::exp2 ((semitones - 69.0f) / 12.0f),
                                           1.0f, 0.45f * (float) sampleRate);

        // envelopes
        auto advance = [] (Envelope& e, float peak)
        {
            switch (e.stage)
            {
                case Envelope::Stage::attack:
                    e.level += e.attackStep * peak;
                    if (e.level >= peak) { e.level = peak; e.stage = Envelope::Stage::decay; }
                    break;
                case Envelope::Stage::decay:   e.level *= e.decayCoef; break;
                case Envelope::Stage::release: e.level *= e.releaseCoef; break;
                case Envelope::Stage::idle:    break;
            }

            if (e.stage != Envelope::Stage::attack && e.level < 1.0e-5f)
            {
                e.level = 0.0f;
                e.stage = Envelope::Stage::idle;
            }
        };

        advance (vca, 1.0f);
        advance (vcf, 1.0f);

        // sources into the filter
        const auto in = oscillator (frequency) + 0.6f * noiseLevel.value * noise();

        const auto cutoffOctaves = cutoffOct.value + vcf.level * (envAmount.value + accentOpen.value)
                                 + lfoLevel.value * lfoValue;
        const auto filtered = filter (in, std::exp2 (std::min (cutoffOctaves, 15.0f)));

        const auto amp = filtered * vca.level * accentGain.value;
        auto out = overdrive (amp) * volume.value;

        // DC blocker
        const auto y = out - dcIn + dcCoef * dcOut;
        dcIn = out;
        dcOut = y;
        return y;
    }

    void Voice::render (float* out, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            out[i] = renderSample();
    }
}
