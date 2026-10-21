#pragma once

#include <array>
#include "PluginProcessor.h"
#include "ui/Widgets.h"

// The front panel, laid out in design coordinates (designWidth x designHeight, as in
// design/mockup.html) and scaled as a whole.
//
// Step keys show one bar of the selected pattern: a click on an empty step switches it on and
// selects it, a click on the selected step switches it off; SELECT/GATE and BACK/NOTE then set its gate
// and note. SHIFT + key = the function printed under it. In REC the keys are a keyboard whose notes go
// into the pattern.
class FrontPanel final : public juce::Component,
                         private juce::Timer
{
public:
    static constexpr int designWidth = 1580, designHeight = 960;
    static constexpr int numKeys = 16;

    explicit FrontPanel (SB64AudioProcessor&);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;

    // shows a text on the segment display for a while
    void showText (const juce::String&, double seconds = 1.5);

    // for the tests: the panel's controls by name
    void pressKey (int key, bool shiftModifier = false);
    void releaseKey (int key);
    void setShift (bool on);
    void turnSelect (int steps, bool fine = false);
    void turnNote (int steps, bool fine = false);
    void back();
    void play();
    void stop();
    void setRecording (bool on);
    void setKeyboardMode (bool on);   // the keys play notes (SHIFT + REC)
    void showBar (int bar);
    void octave (int direction);
    void clickSelectEncoder() { clickSelect(); }
    int getSelectedStep() const noexcept { return selectedStep; }
    juce::String getDisplayText() const { return display->getText(); }

private:
    // what the step keys do without SHIFT
    enum class KeyMode { steps, accent, slide, mod, pitchEnv, scale, transpose };
    // a function waiting for a step key; after a pattern, further keys chain patterns for a while
    enum class Pending { none, lastStep, firstStep, playMode, bank, pattern, chain, clear };
    // what SELECT/GATE changes on the selected step (a click on the encoder switches)
    enum class StepField { gate, chance, cent };

    void timerCallback() override;
    void updateDisplay (double now);
    void updateKeys (double now);
    void selectMenu (int steps);
    void openMenuAt (const char* paramId);
    void changeMenuValue (int steps, bool fine);
    void function (int key);
    void choose (int key);
    void clickSelect();
    juce::String stepFieldText (const sb64::seq::Step&) const;
    sb64::seq::Pattern& pattern();

    template <typename T, typename... Args>
    T& make (Args&&... args)
    {
        auto& item = parts.emplace_back (std::make_unique<T> (std::forward<Args> (args)...));
        addAndMakeVisible (*item);
        return static_cast<T&> (*item);
    }

    sb64::ui::Knob& knob (const char* paramId, float x, float y, float radius);
    sb64::ui::Toggle& toggle (const char* paramId, float x, float y, std::vector<float> positions);

    SB64AudioProcessor& processor;
    std::vector<std::unique_ptr<juce::Component>> parts;
    juce::Image grain;

    sb64::ui::SegmentDisplay* display = nullptr;
    sb64::ui::Led* gateLed = nullptr;
    sb64::ui::Led* accentLed = nullptr;
    sb64::ui::Led* lfoLed = nullptr;
    sb64::ui::RoundButton* shiftButton = nullptr;
    sb64::ui::RoundButton* playButton = nullptr;
    sb64::ui::RoundButton* recButton = nullptr;
    std::array<sb64::ui::StepKey*, numKeys> keys {};
    std::array<int, numKeys> keyNotes {};   // the note each held key started in REC (-1 = up)

    // segment display: a transient text, the open menu, a pending function, else the step or tempo
    juce::String transientText;
    double transientUntil = 0.0, menuUntil = 0.0, menuNameUntil = 0.0;
    int menuIndex = 0;
    double clipUntil = 0.0;

    KeyMode keyMode = KeyMode::steps;
    Pending pending = Pending::none;
    StepField stepField = StepField::gate;
    double chainUntil = 0.0;
    bool keyboard = false;
    int transposeOctave = 0, transposeKey = -1;   // KeyMode::transpose

    // MOD live: while the sequence runs, turning the encoders in MOD mode writes into the playing steps
    bool liveMod (int steps);
    int liveModValue = 0, liveModStep = -1;
    double liveModUntil = 0.0;

    // LFO RATE shown as a running light on the keys
    double lfoViewUntil = 0.0;
    bool showsLfo (double now) const;
    int scaleDegree = -1;   // the user scale's note being edited (KeyMode::scale)
    int editedScale() const;
    int bar = 0, selectedStep = -1;
    bool followPlayhead = true;   // the shown bar follows the playing step until BAR is pressed
    int baseNote = 36;            // REC keyboard: the first key plays C2
    bool shift = false;

    sb64::seq::Pattern clipboard;
    bool hasClipboard = false;
};

class SB64AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SB64AudioProcessorEditor (SB64AudioProcessor&);

    void resized() override;
    FrontPanel& getPanel() noexcept { return panel; }

private:
    static constexpr int minWidth = 790, maxWidth = 2370, defaultWidth = 1185;

    SB64AudioProcessor& processor;
    FrontPanel panel;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SB64AudioProcessorEditor)
};
