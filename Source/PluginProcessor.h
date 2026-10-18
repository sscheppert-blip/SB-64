#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <clap-juce-extensions/clap-juce-extensions.h>

#include "dsp/Voice.h"

class SB64AudioProcessor final : public juce::AudioProcessor,
                                 public clap_juce_extensions::clap_properties
{
public:
    SB64AudioProcessor();
    ~SB64AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Front panel interface
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // the step keys play notes like a keyboard (message thread)
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    struct Display
    {
        std::atomic<bool> gate { false }, accent { false }, lfoHigh { false };
        std::atomic<float> bpm { 120.0f };
        std::atomic<float> peak { 0.0f };   // max since the panel last read it
        std::atomic<int> note { -1 };       // last played note
    };

    Display& getDisplay() noexcept { return display; }

    // editor width, saved with the project
    int getEditorWidth() const noexcept { return editorWidth.load(); }
    void setEditorWidth (int width) noexcept { editorWidth = width; }

private:
    sb64::dsp::Settings readSettings (double bpm) const noexcept;
    void handleMidi (const juce::MidiMessage&) noexcept;
    void playNote (int note, int velocity, bool slide) noexcept;

    juce::AudioProcessorValueTreeState state;

    struct Refs
    {
        std::atomic<float>* drive, * detune, * vcaEnv, * vcfEnv, * resonance, * cutoff, * fmLevel, * subLevel,
                          * noiseLevel, * lfoRate, * lfoLevel, * envAmount, * wave, * filterMode,
                          * lfoShape, * lfoSync, * lfoRange, * lfoReset, * lfoPhase, * slideTime, * accentVel,
                          * pitchEnvDepth, * pitchEnvDecay, * bendRange, * tune, * volume;
    } refs {};

    sb64::dsp::Voice voice;
    juce::MidiKeyboardState keyboardState;

    // held notes, oldest first; the newest one sounds
    std::vector<int> heldNotes;
    int pitchWheel = 8192;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> zeroBuffer, oversampledBuffer;
    int oversamplingFactor = 1;
    int maxChunk = 512;
    bool hostWasPlaying = false;

    std::atomic<int> editorWidth { 0 };
    Display display;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SB64AudioProcessor)
};
