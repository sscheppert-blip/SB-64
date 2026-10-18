#pragma once

#include "Lfo.h"

#include <array>
#include <cstdint>
#include <vector>

// The SB-64 voice: one oscillator (square, saw, triangle) with a sub square one octave down, noise,
// LFO pitch FM up to audio rate, a two-delay-line detune ensemble, a two-pole filter with slew-limited
// integrators (low-pass / band-pass, self-oscillating), amp and filter envelopes, accent, slide, a
// pitch envelope and a dry/overdrive crossfade. JUCE-free; runs at the (oversampled) rate given to
// prepare().
namespace sb64::dsp
{
    enum class Wave { square, saw, triangle };
    enum class FilterMode { bandPass, lowPass };

    // knob mappings, shared with the display
    float cutoffHz (float knob) noexcept;            // 30 Hz .. 17.6 kHz
    float vcaDecaySeconds (float knob) noexcept;     // 40 ms .. 5 s, the top of the range holds (drone)
    float vcfDecaySeconds (float knob) noexcept;     // 30 ms .. 3 s
    float lfoHz (float knob) noexcept;               // free running: 0.05 Hz .. 2 kHz
    float pitchEnvSeconds (float knob) noexcept;     // pitch envelope decay: 10 ms .. 300 ms
    inline constexpr float droneFrom = 0.97f;        // VCA ENV from here on never decays

    struct Settings
    {
        // panel knobs, 0..1
        float drive = 0.0f, detune = 0.0f, vcaEnv = 0.45f, vcfEnv = 0.35f, resonance = 0.4f, cutoff = 0.4f,
              fmLevel = 0.0f, subLevel = 0.0f, noiseLevel = 0.0f, lfoLevel = 0.0f, envAmount = 0.5f;

        Wave wave = Wave::saw;
        FilterMode filter = FilterMode::lowPass;

        LfoShape lfoShape = LfoShape::sine;
        float lfoHz = 2.0f;
        bool lfoReset = false;          // restart the LFO with every new note
        float lfoStartPhase = 0.0f;     // 0..1

        float slideSeconds = 0.1f;
        int pitchEnvDepth = 0;          // 0 (off) .. 9
        float pitchEnvDecay = 0.4f;     // 0..1
        float tuneHz = 0.0f;            // A = 440 Hz + tune
        float volume = 0.7f;            // 0..1
    };

    class Voice
    {
    public:
        static constexpr float pitchEnvRange = 36.0f;   // semitones at depth 9

        Voice();

        void prepare (double sampleRate);
        void setSettings (const Settings&) noexcept;

        // note in semitones (MIDI numbering, fractions allowed). With slide and the gate still on, the
        // pitch glides from the current note and the envelopes keep running.
        void noteOn (float note, bool accent, bool slide) noexcept;
        void noteOff() noexcept;
        void setBend (float semitones) noexcept { bend = semitones; }
        void panic() noexcept;

        // host sync: the LFO's phase in cycles (0..1)
        void setLfoPhase (float cycle) noexcept { lfo.setPhase (cycle); }
        float getLfoValue() const noexcept { return lfoValue; }

        bool isGateOn() const noexcept { return gate; }
        bool isAccent() const noexcept { return accent; }
        float getAmpEnvelope() const noexcept { return vca.level; }

        void render (float* out, int numSamples) noexcept;

    private:
        struct Smoothed
        {
            float value = 0.0f, target = 0.0f;
            void step (float coef) noexcept { value += (target - value) * coef; }
            void jump() noexcept { value = target; }
        };

        struct Envelope
        {
            enum class Stage { idle, attack, decay, release } stage = Stage::idle;
            float level = 0.0f, attackStep = 0.0f, decayCoef = 1.0f, releaseCoef = 1.0f;
        };

        float renderSample() noexcept;
        float oscillator (float frequency) noexcept;
        float detuneEnsemble (float x) noexcept;
        float filter (float x, float frequency) noexcept;
        float overdrive (float x) noexcept;
        void updateTimes() noexcept;
        float noise() noexcept;

        Settings settings;
        double sampleRate = 192000.0;
        float smoothing = 0.01f;

        // pitch
        float note = 48.0f, slideFrom = 48.0f, slideTo = 48.0f, slidePos = 1.0f, slideStep = 0.0f;
        float bend = 0.0f, pitchEnv = 0.0f, pitchEnvCoef = 0.0f;
        bool gate = false, accent = false;

        // oscillator
        float phase = 0.0f;
        bool subHigh = false;

        // detune: two modulated delay lines
        std::vector<float> delayLine;
        int delayWrite = 0, delaySize = 0;
        double ensemblePhase[2] { 0.0, 0.37 };
        float ensembleTone = 0.0f, ensembleToneCoef = 0.0f;

        // filter
        float lp = 0.0f, bp = 0.0f;

        // envelopes
        Envelope vca, vcf;
        Smoothed accentGain, accentOpen;

        // drive and output
        float lowShelf = 0.0f, lowShelfCoef = 0.0f, dcIn = 0.0f, dcOut = 0.0f, dcCoef = 0.0f;

        Smoothed cutoffOct, resonance, subLevel, noiseLevel, detune, drive, fmDepth, lfoLevel, envAmount, volume;

        Lfo lfo;
        float lfoValue = 0.0f;
        uint32_t noiseSeed = 0x9e3779b9u;
    };
}
