#include "Menu.h"

#include "Parameters.h"
#include "dsp/Voice.h"

namespace sb64::menu
{
    namespace id = params::id;

    const std::vector<Item>& items()
    {
        static const std::vector<Item> list {
            { id::lfoShape,      "LFO" },
            { id::lfoSync,       "SYN" },
            { id::lfoRange,      "RNG" },
            { id::lfoReset,      "RST" },
            { id::lfoPhase,      "PHS" },
            { id::slideTime,     "SLD" },
            { id::accentVel,     "ACC" },
            { id::pitchEnvDepth, "PED" },
            { id::pitchEnvDecay, "PET" },
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

        if (paramId == id::slideTime || paramId == id::accentVel || paramId == id::pitchEnvDepth || paramId == id::bendRange)
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

        if (paramId == id::slideTime)
            next = value + (float) steps * (fine ? 1.0f : 5.0f);
        else if (paramId == id::tune)
            next = value + (float) steps * (fine ? 0.1f : 0.5f);
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
