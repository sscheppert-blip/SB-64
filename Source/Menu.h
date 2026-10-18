#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// The settings without a knob of their own: SELECT picks one, NOTE changes it. Everything shown on
// the three-character segment display is formatted here.
namespace sb64::menu
{
    struct Item
    {
        const char* paramId;
        const char* name;   // three characters for the display
    };

    const std::vector<Item>& items();

    // the parameter's current value as display text (three characters, upper case)
    juce::String valueText (juce::AudioProcessorValueTreeState&, const juce::String& paramId);

    // moves a menu parameter by whole steps (NOTE encoder)
    void step (juce::AudioProcessorValueTreeState&, const juce::String& paramId, int steps, bool fine);

    // "C3", "F#4"
    juce::String noteName (int midiNote);
}
