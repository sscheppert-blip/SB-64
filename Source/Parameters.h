#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sb64::params
{
    namespace id
    {
        // panel knobs
        inline constexpr const char* drive      = "drive";
        inline constexpr const char* detune     = "detune";
        inline constexpr const char* vcaEnv     = "vcaEnv";
        inline constexpr const char* vcfEnv     = "vcfEnv";
        inline constexpr const char* resonance  = "resonance";
        inline constexpr const char* cutoff     = "cutoff";
        inline constexpr const char* fmLevel    = "fmLevel";
        inline constexpr const char* subLevel   = "subLevel";
        inline constexpr const char* noiseLevel = "noiseLevel";
        inline constexpr const char* lfoRate    = "lfoRate";
        inline constexpr const char* lfoLevel   = "lfoLevel";
        inline constexpr const char* envAmount  = "envAmount";

        // panel switches
        inline constexpr const char* wave       = "wave";
        inline constexpr const char* filterMode = "filterMode";

        // menu (SELECT / NOTE encoders)
        inline constexpr const char* lfoShape      = "lfoShape";
        inline constexpr const char* lfoSync       = "lfoSync";
        inline constexpr const char* lfoRange      = "lfoRange";
        inline constexpr const char* lfoReset      = "lfoReset";
        inline constexpr const char* lfoPhase      = "lfoPhase";
        inline constexpr const char* slideTime     = "slideTime";
        inline constexpr const char* accentVel     = "accentVelocity";
        inline constexpr const char* pitchEnvDepth = "pitchEnvDepth";
        inline constexpr const char* pitchEnvDecay = "pitchEnvDecay";
        inline constexpr const char* bendRange     = "bendRange";
        inline constexpr const char* tune          = "tune";
        inline constexpr const char* volume        = "volume";
    }

    // synced LFO: cycles per quarter note for each LFO RATE position, low and high range
    inline constexpr int numLfoDivisions = 10;
    inline constexpr double lfoDivisionsLow[numLfoDivisions]  { 1.0 / 64, 1.0 / 32, 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8 };
    inline constexpr double lfoDivisionsHigh[numLfoDivisions] { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 1, 2, 4, 8, 16, 32 };

    int lfoDivisionIndex (float rateKnob);
    double lfoCyclesPerBeat (float rateKnob, bool highRange);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
