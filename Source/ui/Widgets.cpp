#include "Widgets.h"

#include <SB64Fonts.h>

namespace sb64::ui
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        constexpr float startAngle = -150.0f * pi / 180.0f, angleRange = 300.0f * pi / 180.0f;

        juce::Point<float> polar (juce::Point<float> centre, float angle, float distance)
        {
            // angle in radians, clockwise from 12 o'clock
            return centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * distance;
        }

        juce::Typeface::Ptr typeface (const void* data, size_t size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, size);
        }

        juce::ColourGradient radial (juce::Point<float> centre, float radius,
                                     std::initializer_list<std::pair<float, juce::uint32>> stops)
        {
            auto it = stops.begin();
            const auto first = *it;
            const auto last = *(stops.end() - 1);
            juce::ColourGradient gradient (juce::Colour (first.second), centre, juce::Colour (last.second),
                                           centre + juce::Point<float> (radius, 0.0f), true);

            for (++it; it != stops.end() - 1; ++it)
                gradient.addColour (it->first, juce::Colour (it->second));

            return gradient;
        }

        // seven soft lobes, like the knobs of the original finish
        juce::Path starPath (juce::Point<float> centre, float radius, float rotation)
        {
            constexpr int lobes = 7, points = 180;
            constexpr float depth = 0.13f;
            juce::Path path;

            for (int i = 0; i <= points; ++i)
            {
                const auto a = (float) i / (float) points * 2.0f * pi;
                const auto r = radius * (1.0f - depth * 0.5f * (1.0f - std::cos ((float) lobes * a)));
                const auto p = polar (centre, a + rotation, r);
                i == 0 ? path.startNewSubPath (p) : path.lineTo (p);
            }

            path.closeSubPath();
            return path;
        }

        void drawHighlight (juce::Graphics& g, juce::Point<float> centre, float rx, float ry, float rotation, float alpha)
        {
            juce::Path ellipse;
            ellipse.addEllipse (-rx, -ry, rx * 2.0f, ry * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.fillPath (ellipse, juce::AffineTransform::rotation (rotation).translated (centre));
        }

        juce::Path hexagon (juce::Point<float> centre, float radius)
        {
            juce::Path path;

            for (int i = 0; i < 6; ++i)
            {
                const auto a = pi / 6.0f + (float) i * pi / 3.0f;
                const juce::Point<float> p (centre.x + radius * std::cos (a), centre.y + radius * std::sin (a));
                i == 0 ? path.startNewSubPath (p) : path.lineTo (p);
            }

            path.closeSubPath();
            return path;
        }

        juce::ColourGradient nutGradient (juce::Point<float> centre, float radius)
        {
            return radial (centre - juce::Point<float> (radius * 0.2f, radius * 0.3f), radius * 1.6f,
                           { { 0.0f, 0xffe9e9ea }, { 0.6f, 0xff9a9b9f }, { 1.0f, 0xff55565a } });
        }

        void fillShadowCircle (juce::Graphics& g, juce::Point<float> centre, float radius, float alpha)
        {
            juce::Path circle;
            circle.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            juce::DropShadow (juce::Colours::black.withAlpha (alpha), juce::roundToInt (radius * 0.4f), { 2, 6 }).drawForPath (g, circle);
        }
    }

    juce::Font labelFont (float size, float letterSpacing)
    {
        static const auto face = typeface (SB64Fonts::MichromaRegular_ttf, SB64Fonts::MichromaRegular_ttfSize);
        juce::Font font (juce::FontOptions (face).withPointHeight (size));
        font.setExtraKerningFactor (letterSpacing * size / font.getHeight());
        return font;
    }

    juce::Font segmentFont (float size)
    {
        static const auto face = typeface (SB64Fonts::DSEG14ClassicBold_ttf, SB64Fonts::DSEG14ClassicBold_ttfSize);
        juce::Font font (juce::FontOptions (face).withPointHeight (size));
        font.setExtraKerningFactor (0.08f * size / font.getHeight());
        return font;
    }

    void drawText (juce::Graphics& g, const juce::String& text, float x, float baseline, const juce::Font& font,
                   juce::Justification justification)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, x, baseline);
        // letter spacing also follows the last character; leave it out when aligning
        const auto width = glyphs.getBoundingBox (0, -1, true).getWidth() - font.getExtraKerningFactor() * font.getHeight();

        if (justification.testFlags (juce::Justification::horizontallyCentred))
            glyphs.moveRangeOfGlyphs (0, -1, -width * 0.5f, 0.0f);
        else if (justification.testFlags (juce::Justification::right))
            glyphs.moveRangeOfGlyphs (0, -1, -width, 0.0f);

        glyphs.draw (g);
    }

    void drawGlow (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float alpha)
    {
        if (alpha <= 0.0f)
            return;

        juce::ColourGradient gradient (colour.withAlpha (alpha), centre, colour.withAlpha (0.0f),
                                       centre + juce::Point<float> (radius, 0.0f), true);
        gradient.addColour (0.45, colour.withAlpha (alpha * 0.45f));
        g.setGradientFill (gradient);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    }

    void drawTickScale (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        // 51 fine ticks over 300 degrees, every fifth one longer
        for (int i = 0; i <= 50; ++i)
        {
            const auto a = startAngle + angleRange * (float) i / 50.0f;
            const auto isLong = i % 5 == 0;
            g.setColour (isLong ? colours::tickLong : colours::tick);
            g.drawLine ({ polar (centre, a, radius + 9.0f), polar (centre, a, radius + (isLong ? 22.0f : 15.0f)) },
                        isLong ? 2.0f : 1.2f);
        }
    }

    void drawStarKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        // shadow under the knob
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), juce::roundToInt (r * 0.16f), { 2, 7 })
            .drawForPath (g, starPath (c, r + 2.0f, angle));

        // gloss-black body, lit from the top left (the light doesn't turn with the knob)
        const auto star = starPath (c, r, angle);
        g.setGradientFill (radial (c + juce::Point<float> (-0.24f * r, -0.4f * r), 1.7f * r,
                                   { { 0.0f, 0xff5a5b60 }, { 0.22f, 0xff26272a }, { 0.6f, 0xff0d0d0e }, { 1.0f, 0xff050505 } }));
        g.fillPath (star);
        g.setColour (juce::Colour (0xff3c3d41));
        g.strokePath (star, juce::PathStrokeType (1.0f));

        // top cap
        const auto cap = juce::Rectangle<float> (r * 1.24f, r * 1.24f).withCentre (c);
        g.setGradientFill (radial (c + juce::Point<float> (-0.124f * r, -0.248f * r), 0.99f * r,
                                   { { 0.0f, 0xff3b3c40 }, { 0.5f, 0xff151516 }, { 1.0f, 0xff070707 } }));
        g.fillEllipse (cap);
        g.setColour (juce::Colour (0xff2a2b2e));
        g.drawEllipse (cap, 1.0f);

        drawHighlight (g, c + juce::Point<float> (-0.28f * r, -0.36f * r), 0.32f * r, 0.14f * r, -pi / 6.0f, 0.12f);

        // pointer
        g.setColour (juce::Colour (0xfff5f5f5));
        g.drawLine ({ polar (c, angle, r * 0.12f), polar (c, angle, r * 0.9f) }, juce::jmax (4.0f, r * 0.09f));
        const auto tipWidth = juce::jmax (4.0f, r * 0.09f);
        for (auto d : { r * 0.12f, r * 0.9f })
            g.fillEllipse (juce::Rectangle<float> (tipWidth, tipWidth).withCentre (polar (c, angle, d)));
    }

    void drawAppIcon (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto size = juce::jmin (area.getWidth(), area.getHeight());
        const auto tile = juce::Rectangle<float> (size, size).withCentre (area.getCentre());
        const auto u = size / 100.0f;

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1d20), tile.getTopLeft(), juce::Colour (0xff0e0e10),
                                                 tile.getBottomLeft(), false));
        g.fillRoundedRectangle (tile.reduced (2.0f * u), 18.0f * u);
        g.setColour (juce::Colour (0xff2b2b2e));
        g.drawRoundedRectangle (tile.reduced (2.0f * u), 18.0f * u, 1.5f * u);

        const auto c = tile.getCentre();
        drawTickScale (g, c, 26.0f * u);
        drawStarKnob (g, c, 26.0f * u, 40.0f * pi / 180.0f);
        drawGlow (g, c + juce::Point<float> (27.0f * u, 27.0f * u), 13.0f * u, colours::led, 0.7f);
        g.setGradientFill (radial (c + juce::Point<float> (25.0f * u, 24.0f * u), 9.0f * u,
                                   { { 0.0f, 0xffffb3b3 }, { 0.45f, 0xffff5c5c }, { 1.0f, 0xffd42a2a } }));
        g.fillEllipse (juce::Rectangle<float> (13.0f * u, 13.0f * u).withCentre (c + juce::Point<float> (27.0f * u, 27.0f * u)));
    }

    //==============================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, float knobRadius)
        : paramId (id),
          parameter (state.getParameter (id)),
          attachment (*parameter, [this] (float v)
          {
              value = parameter->convertTo0to1 (v);
              repaint();
          }),
          radius (knobRadius)
    {
        attachment.sendInitialUpdate();
    }

    void Knob::place (juce::Point<float> centre)
    {
        const auto half = radius + 26.0f;
        setBounds (juce::Rectangle<float> (half * 2.0f, half * 2.0f).withCentre (centre).toNearestInt());
    }

    void Knob::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        drawTickScale (g, c, radius);
        drawStarKnob (g, c, radius, startAngle + angleRange * value);
    }

    bool Knob::hitTest (int x, int y)
    {
        return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) <= radius + 6.0f;
    }

    void Knob::setValue (float normalised)
    {
        attachment.setValueAsPartOfGesture (parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised)));

        if (onTouch != nullptr)
            onTouch();
    }

    void Knob::mouseDown (const juce::MouseEvent& e)
    {
        dragging = true;
        dragStartValue = value;
        dragStartY = e.position.y;
        attachment.beginGesture();

        if (onTouch != nullptr)
            onTouch();
    }

    void Knob::mouseDrag (const juce::MouseEvent& e)
    {
        // Shift: fine adjustment; the start point follows so switching doesn't jump
        const auto range = e.mods.isShiftDown() ? 1200.0f : 240.0f;
        const auto next = dragStartValue + (dragStartY - e.position.y) / range;

        if (next < 0.0f || next > 1.0f)
        {
            dragStartValue = juce::jlimit (0.0f, 1.0f, next);
            dragStartY = e.position.y;
        }

        setValue (next);

        if (e.mods.isShiftDown())
        {
            dragStartValue = value;
            dragStartY = e.position.y;
        }
    }

    void Knob::mouseUp (const juce::MouseEvent&)
    {
        if (dragging)
            attachment.endGesture();

        dragging = false;
    }

    void Knob::mouseDoubleClick (const juce::MouseEvent&)
    {
        attachment.setValueAsCompleteGesture (parameter->convertFrom0to1 (parameter->getDefaultValue()));

        if (onTouch != nullptr)
            onTouch();
    }

    void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        const auto delta = (wheel.isReversed ? -wheel.deltaY : wheel.deltaY) * (e.mods.isShiftDown() ? 0.03f : 0.15f);
        attachment.setValueAsCompleteGesture (parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, value + delta)));

        if (onTouch != nullptr)
            onTouch();
    }

    //==============================================================================
    Encoder::Encoder (float r) : radius (r) {}

    void Encoder::place (juce::Point<float> centre)
    {
        const auto half = radius + 26.0f;
        setBounds (juce::Rectangle<float> (half * 2.0f, half * 2.0f).withCentre (centre).toNearestInt());
    }

    void Encoder::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        drawTickScale (g, c, radius);
        drawStarKnob (g, c, radius, angle);
    }

    bool Encoder::hitTest (int x, int y)
    {
        return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) <= radius + 6.0f;
    }

    void Encoder::turn (int steps, bool fine)
    {
        angle += (float) steps * pi / 12.0f;
        repaint();

        if (onStep != nullptr)
            onStep (steps, fine);
    }

    void Encoder::mouseDown (const juce::MouseEvent& e)
    {
        lastY = e.position.y;
        travel = 0.0f;
        moved = false;
    }

    void Encoder::mouseDrag (const juce::MouseEvent& e)
    {
        // one step per 14 units of travel
        travel += lastY - e.position.y;
        lastY = e.position.y;
        constexpr float stepTravel = 14.0f;

        if (std::abs (travel) >= stepTravel)
        {
            const auto steps = (int) (travel / stepTravel);
            travel -= (float) steps * stepTravel;
            moved = true;
            turn (steps, e.mods.isShiftDown());
        }
    }

    void Encoder::mouseUp (const juce::MouseEvent& e)
    {
        if (! moved && e.mouseWasClicked() && onClick != nullptr)
            onClick();
    }

    void Encoder::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // a wheel notch is about 0.125 (more on trackpads, which add up)
        wheelRest += (wheel.isReversed ? -wheel.deltaY : wheel.deltaY) * 8.0f;
        const auto steps = (int) wheelRest;

        if (steps != 0)
        {
            wheelRest -= (float) steps;
            turn (steps, e.mods.isShiftDown());
        }
    }

    //==============================================================================
    Toggle::Toggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, std::vector<float> values)
        : positions (std::move (values)),
          attachment (*state.getParameter (paramId), [this] (float v)
          {
              auto best = 0;
              for (size_t i = 1; i < positions.size(); ++i)
                  if (std::abs (positions[i] - v) < std::abs (positions[(size_t) best] - v))
                      best = (int) i;
              index = best;
              repaint();
          })
    {
        attachment.sendInitialUpdate();
    }

    void Toggle::place (juce::Point<float> centre)
    {
        setBounds (juce::Rectangle<float> (64.0f, 100.0f).withCentre (centre).toNearestInt());
    }

    void Toggle::setIndex (int newIndex)
    {
        newIndex = juce::jlimit (0, (int) positions.size() - 1, newIndex);

        if (newIndex != index)
            attachment.setValueAsCompleteGesture (positions[(size_t) newIndex]);

        if (onTouch != nullptr)
            onTouch();
    }

    void Toggle::mouseDown (const juce::MouseEvent& e)
    {
        // click above the nut moves the lever up, below moves it down; in the middle it cycles
        const auto dy = e.position.y - (float) getHeight() * 0.5f;

        if (dy < -12.0f)
            setIndex (index - 1);
        else if (dy > 12.0f)
            setIndex (index + 1);
        else
            setIndex ((index + 1) % (int) positions.size());
    }

    void Toggle::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre();

        fillShadowCircle (g, c + juce::Point<float> (0.0f, 4.0f), 26.0f, 0.6f);

        const auto nut = hexagon (c, 25.0f);
        g.setGradientFill (nutGradient (c, 25.0f));
        g.fillPath (nut);
        g.setColour (juce::Colour (0xff3f4044));
        g.strokePath (nut, juce::PathStrokeType (1.0f));

        const auto ring = juce::Rectangle<float> (30.0f, 30.0f).withCentre (c);
        g.setColour (juce::Colour (0xff2a2b2e));
        g.fillEllipse (ring);
        g.setColour (juce::Colour (0xff8e8f93));
        g.drawEllipse (ring, 2.0f);

        // lever: -1 up, 0 centre (seen end-on), 1 down
        const auto count = (int) positions.size();
        const auto offset = count == 3 ? (float) (index - 1) : (index == 0 ? -1.0f : 1.0f);
        const auto tipY = c.y + offset * (offset == 0.0f ? 4.0f : 30.0f);

        juce::Path bat;
        bat.startNewSubPath (c.x - 5.0f, c.y);
        bat.lineTo (c.x - 6.5f, tipY);
        bat.lineTo (c.x + 6.5f, tipY);
        bat.lineTo (c.x + 5.0f, c.y);
        bat.closeSubPath();

        juce::ColourGradient chrome (juce::Colour (0xff7c7d80), c.x - 6.5f, 0.0f, juce::Colour (0xff5f6064), c.x + 6.5f, 0.0f, false);
        chrome.addColour (0.35, juce::Colour (0xfff2f2f3));
        chrome.addColour (0.55, juce::Colour (0xffb9babd));
        g.setGradientFill (chrome);
        g.fillPath (bat);
        g.setColour (juce::Colour (0xff6d6e72));
        g.strokePath (bat, juce::PathStrokeType (1.0f));

        const auto tip = juce::Rectangle<float> (15.0f, 15.0f).withCentre ({ c.x, tipY });
        g.setGradientFill (nutGradient (tip.getCentre(), 7.5f));
        g.fillEllipse (tip);
        g.setColour (juce::Colour (0xff55565a));
        g.drawEllipse (tip, 1.0f);
    }

    //==============================================================================
    Led::Led (juce::Colour c) : colour (c)
    {
        setInterceptsMouseClicks (false, false);
    }

    void Led::place (juce::Point<float> centre)
    {
        setBounds (juce::Rectangle<float> (40.0f, 40.0f).withCentre (centre).toNearestInt());
    }

    void Led::setLit (float brightness)
    {
        brightness = juce::jlimit (0.0f, 1.0f, brightness);

        if (std::abs (brightness - lit) > 0.01f)
        {
            lit = brightness;
            repaint();
        }
    }

    void Led::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        drawGlow (g, c, 19.0f, colour, 0.5f * lit);

        g.setColour (juce::Colour (0xff1a0a0a));
        g.fillEllipse (juce::Rectangle<float> (15.0f, 15.0f).withCentre (c));
        g.setColour (juce::Colour (0xff333333));
        g.drawEllipse (juce::Rectangle<float> (15.0f, 15.0f).withCentre (c), 1.0f);

        const auto lens = juce::Rectangle<float> (12.0f, 12.0f).withCentre (c);
        const auto off = colour.darker (2.2f).withMultipliedSaturation (0.6f);
        g.setGradientFill (radial (c - juce::Point<float> (1.5f, 1.5f), 7.0f,
                                   { { 0.0f, off.interpolatedWith (colour.brighter (0.6f), lit).getARGB() },
                                     { 1.0f, off.interpolatedWith (colour, lit).getARGB() } }));
        g.fillEllipse (lens);
        g.setColour (juce::Colours::white.withAlpha (0.25f + 0.35f * lit));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c - juce::Point<float> (2.0f, 2.0f)));
    }

    //==============================================================================
    SegmentDisplay::SegmentDisplay()
    {
        setInterceptsMouseClicks (false, false);
    }

    void SegmentDisplay::setText (const juce::String& newText)
    {
        auto t = newText.toUpperCase().substring (0, 3);

        // numbers sit on the right, words on the left; "!" is an empty cell in the segment font
        const auto numeric = t.containsOnly ("0123456789-");
        while (t.length() < 3)
            t = numeric ? "!" + t : t + "!";

        if (t != text)
        {
            text = t;
            repaint();
        }
    }

    void SegmentDisplay::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0a0b));
        g.fillRoundedRectangle (bounds.reduced (1.0f), 8.0f);
        g.setColour (juce::Colour (0xff2b2b2e));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 8.0f, 2.0f);

        const auto window = bounds.reduced (10.0f, 8.0f);
        g.setColour (juce::Colour (0xff1a0505));
        g.fillRoundedRectangle (window, 4.0f);

        const auto font = segmentFont (50.0f);
        const auto x = bounds.getCentreX();
        const auto baseline = 66.0f;

        g.setColour (juce::Colour (0xff3a0d0d));
        drawText (g, "~~~", x, baseline, font);

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, baseline);
        const auto width = glyphs.getBoundingBox (0, -1, true).getWidth() - font.getExtraKerningFactor() * font.getHeight();
        glyphs.moveRangeOfGlyphs (0, -1, x - width * 0.5f, 0.0f);

        juce::Path lit;
        glyphs.createPath (lit);
        juce::DropShadow (colours::led.withAlpha (0.9f), 9, {}).drawForPath (g, lit);
        g.setColour (colours::ledSoft);
        g.fillPath (lit);
    }

    //==============================================================================
    RoundButton::RoundButton (Cap c) : cap (c) {}

    void RoundButton::place (juce::Point<float> centre)
    {
        setBounds (juce::Rectangle<float> (56.0f, 56.0f).withCentre (centre).toNearestInt());
    }

    void RoundButton::setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    bool RoundButton::hitTest (int x, int y)
    {
        return getLocalBounds().toFloat().getCentre().getDistanceFrom ({ (float) x, (float) y }) <= 23.0f;
    }

    void RoundButton::mouseDown (const juce::MouseEvent&)
    {
        down = true;
        repaint();

        if (onPress != nullptr)
            onPress();
    }

    void RoundButton::mouseUp (const juce::MouseEvent&)
    {
        down = false;
        repaint();

        if (onRelease != nullptr)
            onRelease();
    }

    void RoundButton::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        fillShadowCircle (g, c + juce::Point<float> (0.0f, 4.0f), 21.0f, 0.7f);

        g.setColour (juce::Colour (0xff2a2a2c));
        g.fillEllipse (juce::Rectangle<float> (42.0f, 42.0f).withCentre (c));

        if (lit)
            drawGlow (g, c, 30.0f, colours::led, 0.45f);

        const auto top = c + juce::Point<float> (0.0f, down ? 0.0f : -2.0f);
        const auto capArea = juce::Rectangle<float> (36.0f, 36.0f).withCentre (top);
        const auto shade = down ? 0.85f : 1.0f;

        if (cap == Cap::pink)
            g.setGradientFill (radial (top - juce::Point<float> (3.0f, 5.0f), 27.0f,
                                       { { 0.0f, lit ? 0xffffe2e2 : 0xffffd2d2 }, { 0.55f, lit ? 0xffffa0a0 : 0xffff8f8f }, { 1.0f, 0xffcc5050 } }));
        else
            g.setGradientFill (radial (top - juce::Point<float> (3.0f, 5.0f), 27.0f,
                                       { { 0.0f, 0xffe2e0e0 }, { 0.6f, 0xffa9a6a6 }, { 1.0f, 0xff6e6b6b } }));

        g.fillEllipse (capArea);

        if (shade < 1.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (1.0f - shade));
            g.fillEllipse (capArea);
        }

        g.setColour (juce::Colours::black.withAlpha (0.33f));
        g.drawEllipse (capArea, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 7.0f).withCentre (top + juce::Point<float> (-5.0f, -7.0f)));
    }

    //==============================================================================
    StepKey::StepKey (bool isBeat) : beat (isBeat) {}

    void StepKey::place (juce::Point<float> centre)
    {
        setBounds (juce::Rectangle<float> (92.0f, 92.0f).withCentre (centre).toNearestInt());
    }

    void StepKey::setState (int newState)
    {
        if (newState != state)
        {
            state = newState;
            repaint();
        }
    }

    void StepKey::mouseDown (const juce::MouseEvent& e)
    {
        down = true;
        repaint();

        if (onPress != nullptr)
            onPress (e);
    }

    void StepKey::mouseUp (const juce::MouseEvent&)
    {
        down = false;
        repaint();

        if (onRelease != nullptr)
            onRelease();
    }

    void StepKey::paint (juce::Graphics& g)
    {
        const auto c = getLocalBounds().toFloat().getCentre() + juce::Point<float> (0.0f, down ? 1.5f : 0.0f);

        const auto outer = juce::Rectangle<float> (82.0f, 76.0f).withCentre (c + juce::Point<float> (0.0f, 1.0f));
        g.setColour (juce::Colour (beat ? 0xffc9c9cc : 0xff1c1c1e));
        g.fillRoundedRectangle (outer, 6.0f);
        g.setColour (juce::Colour (beat ? 0xffefefef : 0xff3a3a3d));
        g.drawRoundedRectangle (outer, 6.0f, 1.5f);

        const auto inner = juce::Rectangle<float> (72.0f, 66.0f).withCentre (c + juce::Point<float> (0.0f, 1.0f));
        g.setColour (juce::Colour (beat ? 0xffb4b4b8 : 0xff141416));
        g.fillRoundedRectangle (inner, 4.0f);

        drawGlow (g, c, 40.0f, colours::led, state == 2 ? 0.6f : (state == 1 ? 0.22f : 0.0f));

        const auto dome = juce::Rectangle<float> (54.0f, 54.0f).withCentre (c);

        if (state == 0)
            g.setGradientFill (radial (c + juce::Point<float> (-4.3f, -8.1f), 40.0f,
                                       { { 0.0f, 0xff7a5c5c }, { 0.5f, 0xff4a3434 }, { 1.0f, 0xff2a1d1d } }));
        else
            g.setGradientFill (radial (c + juce::Point<float> (-4.3f, -8.1f), 40.0f,
                                       { { 0.0f, 0xffffb3b3 }, { 0.45f, 0xffff5c5c }, { 1.0f, 0xffd42a2a } }));

        g.setOpacity (state == 1 ? 0.72f : 1.0f);
        g.fillEllipse (dome);
        g.setOpacity (1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 10.0f).withCentre (c + juce::Point<float> (-7.0f, -11.0f)));
    }
}
