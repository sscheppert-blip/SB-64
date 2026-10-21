#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory sounds: knob and switch settings, offered to the host as programs and on the panel as the
// menu item PRS. Loading one sets the sound only (not the patterns, tempo or sequencer settings).
namespace sb64::presets
{
    struct Preset
    {
        const char* code;   // three characters for the display
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // plain values; the rest of the sound goes to its default
    };

    const std::vector<Preset>& factory();

    // the parameters a preset sets (everything else stays as it is)
    bool isSoundParameter (const juce::String& paramId);

    void apply (juce::AudioProcessorValueTreeState&, const Preset&);
}
