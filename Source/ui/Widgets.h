#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Front panel parts after design/mockup.html: gloss-black star knobs on tick scales, chrome toggles on
// hex nuts, LEDs, the red segment display, round buttons and the square step keys with lit domes.
// All sizes are in panel design units (the panel is scaled as a whole).
namespace sb64::ui
{
    namespace colours
    {
        inline const juce::Colour background { 0xff0c0c0d };
        inline const juce::Colour label      { 0xffe9e9ea };
        inline const juce::Colour labelDim   { 0xff8d8e92 };
        inline const juce::Colour title      { 0xfff4f4f5 };
        inline const juce::Colour tick       { 0xff6f7075 };
        inline const juce::Colour tickLong   { 0xffa9aaae };
        inline const juce::Colour led        { 0xffff2a2a };
        inline const juce::Colour ledSoft    { 0xffff6b6b };
        inline const juce::Colour ledYellow  { 0xffffd23a };
    }

    // Michroma, size in design units like CSS font-size; letterSpacing in em
    juce::Font labelFont (float size, float letterSpacing = 0.04f);
    juce::Font segmentFont (float size);

    // single line of text on a baseline, like SVG <text>
    void drawText (juce::Graphics&, const juce::String&, float x, float baseline, const juce::Font&,
                   juce::Justification = juce::Justification::centred);

    void drawGlow (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, float alpha);
    void drawStarKnob (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
    void drawTickScale (juce::Graphics&, juce::Point<float> centre, float radius);

    // the app icon: a star knob over a lit step-key dome on the black panel
    void drawAppIcon (juce::Graphics&, juce::Rectangle<float> area);

    //==============================================================================
    // Star knob on a tick scale, bound to a parameter. Drag up/down (Shift: fine), wheel,
    // double-click = default.
    class Knob final : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, float radius);

        void place (juce::Point<float> centre);
        void paint (juce::Graphics&) override;
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        // called whenever the user moves the knob
        std::function<void()> onTouch;
        const juce::String paramId;

    private:
        void setValue (float normalised);

        juce::RangedAudioParameter* parameter = nullptr;
        juce::ParameterAttachment attachment;
        float radius, value = 0.0f, dragStartValue = 0.0f, dragStartY = 0.0f;
        bool dragging = false;
    };

    // Endless encoder with the same knob: turns in steps (drag, wheel), click = press.
    class Encoder final : public juce::Component
    {
    public:
        explicit Encoder (float radius);

        void place (juce::Point<float> centre);
        void paint (juce::Graphics&) override;
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        std::function<void (int steps, bool fine)> onStep;
        std::function<void()> onClick;

    private:
        void turn (int steps, bool fine);

        float radius, angle = 0.0f, lastY = 0.0f, travel = 0.0f;
        bool moved = false;
        float wheelRest = 0.0f;
    };

    // Chrome toggle on a hex nut. positions[i] is the parameter value for lever position i, from top to
    // bottom; two positions = up/down, three = up/centre/down. Click above or below the nut to move it.
    class Toggle final : public juce::Component
    {
    public:
        Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, std::vector<float> positions);

        void place (juce::Point<float> centre);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void()> onTouch;

    private:
        void setIndex (int index);

        std::vector<float> positions;
        int index = 0;
        juce::ParameterAttachment attachment;
    };

    class Led final : public juce::Component
    {
    public:
        explicit Led (juce::Colour);

        void place (juce::Point<float> centre);
        void setLit (float brightness);
        void paint (juce::Graphics&) override;

    private:
        juce::Colour colour;
        float lit = 0.0f;
    };

    // Three 14-segment characters; unlit segments glow faintly.
    class SegmentDisplay final : public juce::Component
    {
    public:
        SegmentDisplay();

        void setText (const juce::String&);   // up to three characters, numbers right-aligned
        juce::String getText() const { return text.removeCharacters ("!"); }
        void paint (juce::Graphics&) override;

    private:
        juce::String text;
    };

    // Round push button (grey or pink cap); momentary.
    class RoundButton final : public juce::Component
    {
    public:
        enum class Cap { grey, pink };

        explicit RoundButton (Cap);

        void place (juce::Point<float> centre);
        void setLit (bool);
        void paint (juce::Graphics&) override;
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onPress, onRelease;

    private:
        Cap cap;
        bool down = false, lit = false;
    };

    // Square step key with a red dome: off, dim or lit. Beat keys (1, 5, 9, 13) have a light frame.
    class StepKey final : public juce::Component
    {
    public:
        explicit StepKey (bool beat);

        void place (juce::Point<float> centre);
        void setState (int state);   // 0 off, 1 dim, 2 lit
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (const juce::MouseEvent&)> onPress;
        std::function<void()> onRelease;

    private:
        bool beat;
        int state = 0;
        bool down = false;
    };
}
