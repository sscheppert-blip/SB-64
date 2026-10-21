#include "Menu.h"

#include "Parameters.h"
#include "dsp/Arp.h"
#include "dsp/Scales.h"
#include "dsp/Sequencer.h"
#include "dsp/Voice.h"

namespace sb64::menu
{
    namespace id = params::id;
    namespace seq = sb64::seq;

    const std::vector<Item>& items()
    {
        static const std::vector<Item> list {
            { presetItem,        "PRS" },
            { id::tempo,         "BPM" },
            { id::clock,         "CLK" },
            { id::swing,         "SWG" },
            { id::transpose,     "TRP" },
            { id::scale,         "SCL" },
            { id::root,          "KEY" },
            { id::transposeLatch, "TLT" },
            { id::masterSlide,   "MSL" },
            { id::arp,           "ARP" },
            { id::arpTempo,      "ABP" },
            { id::arpDir,        "ADR" },
            { id::arpOct,        "AOC" },
            { id::arpGate,       "AGT" },
            { id::arpRhythm,     "ARH" },
            { id::arpSource,     "ASR" },
            { id::rndDensity,    "DNS" },
            { id::rndAccent,     "RAC" },
            { id::rndSlide,      "RSL" },
            { id::rndOctaves,    "ROC" },
            { id::rndMod,        "RMD" },
            { id::rndPitchEnv,   "RPE" },
            { id::rndChance,     "RPR" },
            { id::rndGateLow,    "RGL" },
            { id::rndGateHigh,   "RGH" },
            { id::lfoShape,      "LFO" },
            { id::lfoSync,       "SYN" },
            { id::lfoRange,      "RNG" },
            { id::lfoReset,      "RST" },
            { id::lfoPhase,      "PHS" },
            { id::accentVel,     "ACC" },
            { id::bendRange,     "BND" },
            { id::tune,          "TUN" },
            { id::volume,        "VOL" },
        };
        return list;
    }

    juce::String noteName (int midiNote)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
    }

    namespace
    {
        juce::String divisionText (double cyclesPerBeat)
        {
            if (cyclesPerBeat < 1.0)
                return "/" + juce::String (juce::roundToInt (1.0 / cyclesPerBeat));

            return juce::String (juce::roundToInt (cyclesPerBeat)) + "X";
        }
    }

    juce::String valueText (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    {
        auto* parameter = state.getParameter (paramId);
        if (parameter == nullptr)
            return {};

        const auto value = parameter->convertFrom0to1 (parameter->getValue());
        const auto whole = juce::roundToInt (value);

        if (paramId == id::lfoShape)
        {
            static const char* shapes[] { "SIN", "TRI", "SAW", "RMP", "SQR", "TDS", "DST", "TAR", "S-H", "NOS" };
            return shapes[juce::jlimit (0, 9, whole)];
        }

        if (paramId == id::lfoSync || paramId == id::lfoReset)
            return whole != 0 ? "ON" : "OFF";

        if (paramId == id::lfoRange)
            return whole != 0 ? "HI" : "LO";

        if (paramId == id::lfoPhase)
            return juce::String (whole * 45);

        if (paramId == id::clock)
            return whole != 0 ? "HST" : "INT";

        if (paramId == id::scale)
        {
            const auto scale = juce::jlimit (0, seq::numScales - 1, whole);
            return seq::isUserScale (scale) ? "U" + juce::String (scale - seq::numFactoryScales + 1) : juce::String (seq::scaleNames[scale]);
        }

        if (paramId == id::root)
            return noteName (whole + 12).dropLastCharacters (1);

        if (paramId == id::arp || paramId == id::transposeLatch)
            return whole != 0 ? "ON" : "OFF";

        if (paramId == id::arpDir)
            return seq::arpDirectionNames[juce::jlimit (0, 3, whole)];

        if (paramId == id::arpSource)
            return whole == 2 ? "PAT" : (whole == 1 ? "SCL" : "KEY");

        if (paramId == id::transpose)
            return whole > 0 ? "+" + juce::String (whole) : juce::String (whole);

        if (paramId == id::arpTempo)
            return whole >= 20 ? juce::String (whole) : juce::String ("SEQ");

        if (paramId == id::arpOct || paramId == id::arpGate || paramId == id::arpRhythm || paramId == id::rndDensity
            || paramId == id::rndAccent || paramId == id::rndSlide || paramId == id::rndOctaves || paramId == id::rndMod
            || paramId == id::rndPitchEnv || paramId == id::rndChance || paramId == id::rndGateLow || paramId == id::rndGateHigh)
            return juce::String (whole);

        if (paramId == id::tempo || paramId == id::swing || paramId == id::accentVel || paramId == id::bendRange
            || paramId == id::masterSlide)
            return juce::String (whole);

        if (paramId == id::tune)
            return juce::String (value, std::abs (value) < 9.95f ? 1 : 0);

        if (paramId == id::wave)
        {
            static const char* waves[] { "SQR", "SAW", "TRI" };
            return waves[juce::jlimit (0, 2, whole)];
        }

        if (paramId == id::filterMode)
            return whole != 0 ? "LP" : "BP";

        if (paramId == id::vcaEnv && value >= dsp::droneFrom)
            return "INF";

        if (paramId == id::lfoRate && *state.getRawParameterValue (id::lfoSync) > 0.5f)
            return divisionText (params::lfoCyclesPerBeat (value, *state.getRawParameterValue (id::lfoRange) > 0.5f));

        // knobs: 0..100
        return juce::String (juce::roundToInt (value * 100.0f));
    }

    void step (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, int steps, bool fine)
    {
        auto* parameter = state.getParameter (paramId);
        if (parameter == nullptr || steps == 0)
            return;

        const auto& range = parameter->getNormalisableRange();
        const auto value = range.convertFrom0to1 (parameter->getValue());
        float next = value;

        if (paramId == id::tune)
            next = value + (float) steps * (fine ? 0.1f : 0.5f);
        else if (paramId == id::arpTempo)
            next = value < 20.0f ? (steps > 0 ? 20.0f + (float) (steps - 1) * (fine ? 1.0f : 5.0f) : 0.0f)
                                 : (value + (float) steps * (fine ? 1.0f : 5.0f) < 20.0f ? 0.0f : value + (float) steps * (fine ? 1.0f : 5.0f));
        else if (paramId == id::arpGate || paramId == id::rndDensity || paramId == id::rndAccent || paramId == id::rndSlide
                 || paramId == id::rndPitchEnv || paramId == id::rndChance || paramId == id::rndGateLow || paramId == id::rndGateHigh
                 || paramId == id::masterSlide)
            next = value + (float) steps * (fine ? 1.0f : 5.0f);
        else if (paramId == id::accentVel)
            next = value + (float) steps * (fine ? 1.0f : 4.0f);
        else if (range.interval >= 1.0f || dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr
                 || dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
            next = value + (float) steps;
        else
            next = value + (float) steps * (fine ? 0.002f : 0.01f) * (range.end - range.start);

        next = juce::jlimit (range.start, range.end, next);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (range.convertTo0to1 (next));
        parameter->endChangeGesture();
    }
}
