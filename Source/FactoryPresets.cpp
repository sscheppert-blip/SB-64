#include "FactoryPresets.h"
#include "Parameters.h"

#include <optional>

namespace sb64::presets
{
    namespace id = params::id;

    namespace
    {
        // LFO RATE knob position for a synced division (0 = slowest .. 9)
        constexpr float division (int index) { return (float) index / 9.0f; }

        enum Wave { square = 0, saw = 1, triangle = 2 };
        enum Filter { bandPass = 0, lowPass = 1 };
        enum Shape { sine = 0, tri = 1, falling = 2, ramp = 3, squareLfo = 4, tripleSaw = 5, doubleSaw = 6, tripleRamp = 7,
                     sampleHold = 8 };
    }

    const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> list {
            { "INI", "Init", {} },
            { "ACD", "Classic Acid",
              { { id::wave, saw }, { id::cutoff, 0.4f }, { id::resonance, 0.75f }, { id::vcfEnv, 0.35f }, { id::envAmount, 0.6f },
                { id::vcaEnv, 0.45f }, { id::drive, 0.2f } } },
            { "SQR", "Square Squelch",
              { { id::wave, square }, { id::cutoff, 0.38f }, { id::resonance, 0.72f }, { id::vcfEnv, 0.3f }, { id::envAmount, 0.62f },
                { id::drive, 0.35f } } },
            { "RUB", "Rubber Bass",
              { { id::wave, triangle }, { id::cutoff, 0.45f }, { id::resonance, 0.3f }, { id::subLevel, 0.6f }, { id::envAmount, 0.3f },
                { id::vcfEnv, 0.25f }, { id::vcaEnv, 0.5f }, { id::drive, 0.15f } } },
            { "SWM", "Swarm",
              { { id::wave, saw }, { id::detune, 0.6f }, { id::subLevel, 0.4f }, { id::cutoff, 0.53f }, { id::resonance, 0.35f },
                { id::drive, 0.3f }, { id::vcaEnv, 0.6f }, { id::envAmount, 0.4f } } },
            { "DRV", "Overdrive",
              { { id::wave, saw }, { id::drive, 0.85f }, { id::cutoff, 0.43f }, { id::resonance, 0.62f }, { id::envAmount, 0.5f },
                { id::vcfEnv, 0.32f } } },
            { "BPZ", "Band Zap",
              { { id::wave, square }, { id::filterMode, bandPass }, { id::resonance, 0.82f }, { id::cutoff, 0.51f },
                { id::envAmount, 0.7f }, { id::vcfEnv, 0.2f }, { id::drive, 0.3f } } },
            { "MTL", "Metal FM",
              { { id::wave, square }, { id::lfoShape, sine }, { id::lfoRate, 0.86f }, { id::fmLevel, 0.5f }, { id::cutoff, 0.59f },
                { id::resonance, 0.3f }, { id::envAmount, 0.35f } } },
            { "PLK", "Pluck",
              { { id::wave, triangle }, { id::vcaEnv, 0.2f }, { id::vcfEnv, 0.15f }, { id::cutoff, 0.5f }, { id::resonance, 0.45f },
                { id::envAmount, 0.75f } } },
            { "HAT", "Noise Hat",
              { { id::wave, square }, { id::noiseLevel, 0.95f }, { id::cutoff, 0.94f }, { id::resonance, 0.2f }, { id::drive, 0.7f },
                { id::vcaEnv, 0.03f }, { id::envAmount, 0.0f } } },
            { "DRN", "Drone",
              { { id::wave, saw }, { id::vcaEnv, 1.0f }, { id::detune, 0.5f }, { id::lfoShape, tri }, { id::lfoRate, 0.2f },
                { id::lfoLevel, 0.35f }, { id::resonance, 0.6f }, { id::cutoff, 0.4f }, { id::envAmount, 0.2f } } },
            { "WOB", "Wobble",
              { { id::wave, saw }, { id::lfoSync, 1.0f }, { id::lfoRate, division (6) }, { id::lfoShape, sine }, { id::lfoLevel, 0.6f },
                { id::cutoff, 0.36f }, { id::resonance, 0.6f }, { id::subLevel, 0.5f }, { id::vcaEnv, 0.8f } } },
            { "STP", "Stepper",
              { { id::wave, saw }, { id::lfoShape, tripleRamp }, { id::lfoSync, 1.0f }, { id::lfoRate, division (7) },
                { id::lfoReset, 1.0f }, { id::lfoLevel, 0.5f }, { id::cutoff, 0.4f }, { id::resonance, 0.65f } } },
            { "SUB", "Deep Sub",
              { { id::wave, triangle }, { id::subLevel, 0.9f }, { id::cutoff, 0.3f }, { id::resonance, 0.1f }, { id::envAmount, 0.2f },
                { id::vcaEnv, 0.55f }, { id::drive, 0.25f } } },
            { "SHR", "Random Steps",
              { { id::wave, square }, { id::lfoShape, sampleHold }, { id::lfoSync, 1.0f }, { id::lfoRate, division (8) },
                { id::lfoLevel, 0.45f }, { id::cutoff, 0.43f }, { id::resonance, 0.7f }, { id::drive, 0.4f } } },
            { "WHS", "Whistle",
              { { id::wave, triangle }, { id::filterMode, bandPass }, { id::resonance, 0.95f }, { id::cutoff, 0.57f },
                { id::envAmount, 0.25f }, { id::vcaEnv, 0.7f } } },
        };
        return list;
    }

    bool isSoundParameter (const juce::String& paramId)
    {
        static const juce::StringArray sound { id::drive, id::detune, id::vcaEnv, id::vcfEnv, id::resonance, id::cutoff,
                                               id::fmLevel, id::subLevel, id::noiseLevel, id::lfoRate, id::lfoLevel, id::envAmount,
                                               id::wave, id::filterMode, id::lfoShape, id::lfoSync, id::lfoRange, id::lfoReset,
                                               id::lfoPhase };
        return sound.contains (paramId);
    }

    void apply (juce::AudioProcessorValueTreeState& state, const Preset& preset)
    {
        auto set = [&state] (const juce::String& paramId, std::optional<float> plain)
        {
            if (auto* parameter = state.getParameter (paramId))
            {
                const auto normalised = plain ? parameter->convertTo0to1 (*plain) : parameter->getDefaultValue();
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        };

        for (auto* parameter : state.processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter); withId != nullptr && isSoundParameter (withId->paramID))
                set (withId->paramID, std::nullopt);

        for (const auto& [paramId, value] : preset.values)
            set (paramId, value);
    }
}
