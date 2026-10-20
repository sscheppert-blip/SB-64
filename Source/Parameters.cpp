#include "Parameters.h"

#include "dsp/Voice.h"

namespace sb64::params
{
    namespace
    {
        using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

        juce::String percent (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; }

        void addKnob (Layout& layout, const char* paramId, const juce::String& name, float defaultValue,
                      std::function<juce::String (float, int)> text = percent)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { paramId, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), defaultValue,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
        }

        juce::String hz (float value)
        {
            return value < 1000.0f ? juce::String (value, value < 10.0f ? 2 : (value < 100.0f ? 1 : 0)) + " Hz"
                                   : juce::String (value / 1000.0f, 2) + " kHz";
        }

        juce::StringArray scaleChoices()
        {
            juce::StringArray names { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic", "Whole Tone",
                                      "Major Blues", "Minor Blues" };
            for (int u = 1; u <= 16; ++u)
                names.add ("User " + juce::String (u));
            return names;
        }

        juce::String seconds (float value)
        {
            return value < 1.0f ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms" : juce::String (value, 2) + " s";
        }
    }

    int lfoDivisionIndex (float rateKnob)
    {
        return juce::jlimit (0, numLfoDivisions - 1, juce::roundToInt (rateKnob * (float) (numLfoDivisions - 1)));
    }

    double lfoCyclesPerBeat (float rateKnob, bool highRange)
    {
        const auto i = (size_t) lfoDivisionIndex (rateKnob);
        return highRange ? lfoDivisionsHigh[i] : lfoDivisionsLow[i];
    }

    Layout createLayout (bool standalone)
    {
        Layout layout;

        addKnob (layout, id::drive, "Drive", 0.2f);
        addKnob (layout, id::detune, "Detune", 0.0f);
        addKnob (layout, id::vcaEnv, "VCA Env", 0.45f, [] (float v, int)
        {
            return v >= dsp::droneFrom ? juce::String ("Drone") : seconds (dsp::vcaDecaySeconds (v));
        });
        addKnob (layout, id::vcfEnv, "VCF Env", 0.35f, [] (float v, int) { return seconds (dsp::vcfDecaySeconds (v)); });
        addKnob (layout, id::resonance, "Resonance", 0.6f);
        addKnob (layout, id::cutoff, "Cutoff", 0.35f, [] (float v, int) { return hz (dsp::cutoffHz (v)); });
        addKnob (layout, id::fmLevel, "FM Level", 0.0f);
        addKnob (layout, id::subLevel, "Sub Level", 0.0f);
        addKnob (layout, id::noiseLevel, "Noise Level", 0.0f);
        addKnob (layout, id::lfoRate, "LFO Rate", 0.35f, [] (float v, int) { return hz (dsp::lfoHz (v)); });
        addKnob (layout, id::lfoLevel, "LFO Level", 0.0f);
        addKnob (layout, id::envAmount, "Env Amount", 0.55f);

        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::wave, 1 }, "Wave",
                                                                  juce::StringArray { "Square", "Saw", "Triangle" }, 1));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::filterMode, 1 }, "Filter",
                                                                  juce::StringArray { "Band-pass", "Low-pass" }, 1));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id::lfoShape, 1 }, "LFO Shape",
            juce::StringArray { "Sine", "Triangle", "Saw", "Ramp", "Square", "Triple Saw", "Double Saw + Tri",
                                "Triple Ramp", "Sample & Hold", "Noise" }, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::lfoSync, 1 }, "LFO Sync", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::lfoRange, 1 }, "LFO Sync Range",
                                                                  juce::StringArray { "Low", "High" }, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::lfoReset, 1 }, "LFO Reset", false));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id::lfoPhase, 1 }, "LFO Phase", 0, 7, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return juce::String (v * 45) + juce::String::fromUTF8 ("\xc2\xb0"); })));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::slideTime, 1 }, "Slide Time", juce::NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 90.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::accentVel, 1 }, "Accent Velocity", 1, 127, 100));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::pitchEnvDepth, 1 }, "Pitch Env Depth", 0, 9, 0));
        addKnob (layout, id::pitchEnvDecay, "Pitch Env Decay", 0.4f, [] (float v, int) { return seconds (dsp::pitchEnvSeconds (v)); });
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::bendRange, 1 }, "Bend Range", 0, 12, 2,
                                                               juce::AudioParameterIntAttributes().withLabel ("st")));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::tune, 1 }, "Tune", juce::NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        addKnob (layout, id::volume, "Volume", 0.7f);

        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::run, 1 }, "Run", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::tempo, 1 }, "Tempo", juce::NormalisableRange<float> (20.0f, 480.0f, 1.0f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("BPM")));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::clock, 1 }, "Clock",
                                                                  juce::StringArray { "Internal", "Host" }, standalone ? 0 : 1));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::swing, 1 }, "Swing", 0, 50, 0,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::playMode, 1 }, "Play Mode",
                                                                  juce::StringArray { "Forward", "Backward", "Random", "Ping-Pong", "Ping-Pong 2" }, 0));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::transpose, 1 }, "Transpose", -24, 24, 0,
                                                               juce::AudioParameterIntAttributes().withLabel ("st")));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::scale, 1 }, "Scale", scaleChoices(), 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::root, 1 }, "Scale Root",
                                                                  juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::masterSlide, 1 }, "Master Slide", 0, 100, 0,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id::arp, 1 }, "Arpeggiator", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::arpDir, 1 }, "Arp Direction",
                                                                  juce::StringArray { "Up", "Down", "Up & Down", "Random" }, 0));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::arpOct, 1 }, "Arp Octaves", 1, 3, 1));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::arpGate, 1 }, "Arp Gate", 10, 100, 50,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::arpRhythm, 1 }, "Arp Rhythm", 1, 6, 1));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::arpSource, 1 }, "Arp Source",
                                                                  juce::StringArray { "Keys", "Scale" }, 0));

        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::rndDensity, 1 }, "Random Density", 10, 90, 70,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::rndAccent, 1 }, "Random Accents", 0, 100, 25,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::rndSlide, 1 }, "Random Slides", 0, 100, 20,
                                                               juce::AudioParameterIntAttributes().withLabel ("%")));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::rndOctaves, 1 }, "Random Octaves", 1, 3, 1));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id::rndMod, 1 }, "Random Mod", 0, 64, 0));

        return layout;
    }
}
