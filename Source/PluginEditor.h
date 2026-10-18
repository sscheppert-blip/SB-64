#pragma once

#include "PluginProcessor.h"
#include "ui/Widgets.h"

// The front panel, laid out in design coordinates (designWidth x designHeight, as in
// design/mockup.html) and scaled as a whole.
class FrontPanel final : public juce::Component,
                         private juce::Timer
{
public:
    static constexpr int designWidth = 1580, designHeight = 960;
    static constexpr int numKeys = 16;

    explicit FrontPanel (SB64AudioProcessor&);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;

    // shows a text on the segment display for a while (then back to the tempo or the open menu)
    void showText (const juce::String&, double seconds = 1.5);

private:
    void timerCallback() override;
    void updateDisplay (double now);
    void updateKeys();
    void selectMenu (int steps);
    void changeMenuValue (int steps, bool fine);
    void pressKey (int key, bool accent);
    void releaseKey (int key);
    void setShift (bool on);

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
    std::array<sb64::ui::StepKey*, numKeys> keys {};
    std::array<int, numKeys> keyNotes {};   // the note each held key started (-1 = up)

    // segment display: a transient text, else the open menu, else the tempo
    juce::String transientText;
    double transientUntil = 0.0, menuUntil = 0.0, menuNameUntil = 0.0;
    int menuIndex = 0;
    double clipUntil = 0.0;

    int baseNote = 36;   // the first step key plays C2
    bool shift = false;
};

class SB64AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SB64AudioProcessorEditor (SB64AudioProcessor&);

    void resized() override;

private:
    static constexpr int minWidth = 790, maxWidth = 2370, defaultWidth = 1185;

    SB64AudioProcessor& processor;
    FrontPanel panel;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SB64AudioProcessorEditor)
};
