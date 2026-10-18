#include "PluginEditor.h"
#include "Menu.h"
#include "Parameters.h"

namespace
{
    namespace id = sb64::params::id;
    namespace ui = sb64::ui;
    namespace colours = sb64::ui::colours;

    constexpr float keyY = 791.0f, buttonY = 697.0f;
    float keyX (int key) { return 73.0f + 95.1f * (float) key; }

    const char* const keyLabels[] { "ACC", "SLIDE", "MOD", "PITCH ENV", "LAST STEP", "PLAY MODE", "TRANSP", "SCALE",
                                    "ARPEGG", "LFO", "RND PATT", "CLEAR", "COPY", "PASTE", "BANK", "PATTERN" };

    struct KnobLabel
    {
        float x, baseline;
        const char* text;
        float size;
    };

    // panel labels (positions as in the mockup)
    const KnobLabel knobLabels[] {
        { 165, 407, "DRIVE", 17 },      { 385, 445, "DETUNE", 17 },   { 590, 425, "VCA ENV", 17 },
        { 980, 425, "VCF ENV", 17 },    { 1185, 445, "RESONANCE", 17 }, { 1405, 402, "CUTOFF", 17 },
        { 102, 634, "FM LVL", 17 },     { 307, 634, "SUB LVL", 17 },  { 510, 634, "NOISE LVL", 17 },
        { 685, 697, "SELECT/GATE", 17 }, { 890, 697, "BACK/NOTE", 17 }, { 1063, 634, "LFO RATE", 17 },
        { 1270, 634, "LFO LVL", 17 },   { 1470, 634, "ENV/CV LVL", 17 }, { 1470, 654, "CONFIG", 13.5f },
    };

    bool isNatural (int note)
    {
        const auto n = ((note % 12) + 12) % 12;
        return n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11;
    }
}

FrontPanel::FrontPanel (SB64AudioProcessor& p) : processor (p)
{
    setSize (designWidth, designHeight);
    setOpaque (true);
    keyNotes.fill (-1);

    // fine grain on the panel finish
    grain = juce::Image (juce::Image::ARGB, 256, 256, true);
    {
        juce::Random random (3);
        for (int y = 0; y < grain.getHeight(); ++y)
            for (int x = 0; x < grain.getWidth(); ++x)
                grain.setPixelAt (x, y, juce::Colours::white.withAlpha (random.nextFloat() * 0.05f));
    }

    // top row
    knob (id::drive, 165, 297, 70);
    knob (id::detune, 385, 345, 60);
    knob (id::vcaEnv, 590, 335, 48);
    toggle (id::wave, 740, 341, { 0.0f, 1.0f, 2.0f });   // square, saw, triangle
    toggle (id::filterMode, 823, 341, { 0.0f, 1.0f });   // band-pass, low-pass
    knob (id::vcfEnv, 980, 335, 48);
    knob (id::resonance, 1185, 345, 60);
    knob (id::cutoff, 1405, 292, 70);

    // second row
    knob (id::fmLevel, 102, 550, 48);
    knob (id::subLevel, 307, 550, 48);
    knob (id::noiseLevel, 510, 550, 48);
    knob (id::lfoRate, 1063, 550, 48);
    knob (id::lfoLevel, 1270, 550, 48);
    knob (id::envAmount, 1470, 550, 48);

    // SELECT picks a setting, NOTE changes it
    auto& select = make<ui::Encoder> (52.0f);
    select.place ({ 685, 605 });
    select.onStep = [this] (int steps, bool) { selectMenu (steps); };
    select.onClick = [this] { selectMenu (0); };

    auto& note = make<ui::Encoder> (52.0f);
    note.place ({ 890, 605 });
    note.onStep = [this] (int steps, bool fine) { changeMenuValue (steps, fine); };
    note.onClick = [this] { menuUntil = transientUntil = 0.0; };   // BACK: close the menu

    display = &make<ui::SegmentDisplay>();
    display->setBounds (682, 409, 206, 92);

    gateLed = &make<ui::Led> (colours::led);
    gateLed->place ({ 663, 260 });
    accentLed = &make<ui::Led> (colours::ledYellow);
    accentLed->place ({ 1053, 258 });
    lfoLed = &make<ui::Led> (colours::led);
    lfoLed->place ({ 1137, 470 });

    // transport and keyboard buttons
    auto& shiftButton = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    shiftButton.place ({ 72, buttonY });
    shiftButton.onPress = [this] { setShift (! shift); };

    // the sequencer buttons come with the sequencer
    for (auto [x, cap] : { std::pair { 168.0f, ui::RoundButton::Cap::pink }, { 262.0f, ui::RoundButton::Cap::pink },
                           { 357.0f, ui::RoundButton::Cap::grey }, { 1213.0f, ui::RoundButton::Cap::grey },
                           { 1308.0f, ui::RoundButton::Cap::grey } })
    {
        auto& button = make<ui::RoundButton> (cap);
        button.place ({ x, buttonY });
        button.onPress = [this] { showText ("SEQ", 0.8); };
    }

    auto& octaveDown = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    octaveDown.place ({ 1404, buttonY });
    octaveDown.onPress = [this]
    {
        baseNote = juce::jmax (12, baseNote - 12);
        showText (sb64::menu::noteName (baseNote));
    };

    auto& octaveUp = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    octaveUp.place ({ 1500, buttonY });
    octaveUp.onPress = [this]
    {
        baseNote = juce::jmin (84, baseNote + 12);
        showText (sb64::menu::noteName (baseNote));
    };

    // step keys: a keyboard for now (Shift: accent)
    for (int i = 0; i < numKeys; ++i)
    {
        auto& key = make<ui::StepKey> (i % 4 == 0);
        key.place ({ keyX (i), keyY });
        key.onPress = [this, i] (const juce::MouseEvent& e) { pressKey (i, shift || e.mods.isShiftDown()); };
        key.onRelease = [this, i] { releaseKey (i); };
        keys[(size_t) i] = &key;
    }

    updateKeys();
    updateDisplay (juce::Time::getMillisecondCounterHiRes());
    startTimerHz (30);
}

FrontPanel::~FrontPanel()
{
    for (int i = 0; i < numKeys; ++i)
        releaseKey (i);
}

ui::Knob& FrontPanel::knob (const char* paramId, float x, float y, float radius)
{
    auto& k = make<ui::Knob> (processor.getState(), paramId, radius);
    k.place ({ x, y });
    k.onTouch = [this, paramId] { showText (sb64::menu::valueText (processor.getState(), paramId)); };
    return k;
}

ui::Toggle& FrontPanel::toggle (const char* paramId, float x, float y, std::vector<float> positions)
{
    auto& t = make<ui::Toggle> (processor.getState(), paramId, std::move (positions));
    t.place ({ x, y });
    t.onTouch = [this, paramId] { showText (sb64::menu::valueText (processor.getState(), paramId)); };
    return t;
}

void FrontPanel::showText (const juce::String& text, double seconds)
{
    transientText = text;
    transientUntil = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
    updateDisplay (juce::Time::getMillisecondCounterHiRes());
}

void FrontPanel::selectMenu (int steps)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto count = (int) sb64::menu::items().size();

    // the first turn opens the menu where it was left
    if (now < menuUntil)
        menuIndex = ((menuIndex + steps) % count + count) % count;

    transientUntil = 0.0;
    menuNameUntil = now + 800.0;
    menuUntil = now + 5000.0;
    updateDisplay (now);
}

void FrontPanel::changeMenuValue (int steps, bool fine)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now >= menuUntil)
    {
        selectMenu (0);   // shows which setting NOTE changes
        return;
    }

    sb64::menu::step (processor.getState(), sb64::menu::items()[(size_t) menuIndex].paramId, steps, fine);
    transientUntil = menuNameUntil = 0.0;
    menuUntil = now + 5000.0;
    updateDisplay (now);
}

void FrontPanel::updateDisplay (double now)
{
    if (now < transientUntil)
    {
        display->setText (transientText);
    }
    else if (now < menuUntil)
    {
        const auto& item = sb64::menu::items()[(size_t) menuIndex];
        display->setText (now < menuNameUntil ? juce::String (item.name)
                                              : sb64::menu::valueText (processor.getState(), item.paramId));
    }
    else
    {
        display->setText (juce::String (juce::roundToInt (processor.getDisplay().bpm.load())));
    }
}

void FrontPanel::pressKey (int key, bool accent)
{
    releaseKey (key);
    const auto note = juce::jlimit (0, 127, baseNote + key);
    keyNotes[(size_t) key] = note;
    processor.getKeyboardState().noteOn (1, note, accent ? 1.0f : 0.7f);
    showText (sb64::menu::noteName (note), 0.8);
}

void FrontPanel::releaseKey (int key)
{
    if (const auto note = std::exchange (keyNotes[(size_t) key], -1); note >= 0)
        processor.getKeyboardState().noteOff (1, note, 0.0f);
}

void FrontPanel::setShift (bool on)
{
    shift = on;
    repaint (0, 830, designWidth, 40);   // the step key labels
}

void FrontPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    auto& d = processor.getDisplay();

    if (d.peak.exchange (0.0f) >= 0.99f)
        clipUntil = now + 300.0;

    gateLed->setLit (d.gate.load() ? 1.0f : 0.0f);
    accentLed->setLit (d.accent.load() || now < clipUntil ? 1.0f : 0.0f);
    lfoLed->setLit (d.lfoHigh.load() ? 1.0f : 0.0f);

    updateKeys();
    updateDisplay (now);
}

void FrontPanel::updateKeys()
{
    // playing notes light up; the white keys of the octave glow dimly
    auto& keyboard = processor.getKeyboardState();

    for (int i = 0; i < numKeys; ++i)
    {
        const auto note = baseNote + i;
        keys[(size_t) i]->setState (keyboard.isNoteOnForChannels (0xffff, note) ? 2 : (isNatural (note) ? 1 : 0));
    }
}

void FrontPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (colours::background);

    // chassis
    g.setColour (juce::Colour (0xff09090a));
    g.fillRoundedRectangle (bounds, 22.0f);

    const auto face = bounds.reduced (6.0f);
    juce::ColourGradient finish (juce::Colour (0xff1b1b1d), 0.0f, face.getY(), juce::Colour (0xff111113), 0.0f, face.getBottom(), false);
    finish.addColour (0.5, juce::Colour (0xff151517));
    g.setGradientFill (finish);
    g.fillRoundedRectangle (face, 18.0f);
    g.setTiledImageFill (grain, 0, 0, 1.0f);
    g.fillRoundedRectangle (face, 18.0f);
    g.setColour (juce::Colour (0xff2b2b2e));
    g.drawRoundedRectangle (face, 18.0f, 2.0f);

    // title
    g.setColour (colours::title);
    ui::drawText (g, "SYNTH BASS", 785.0f, 140.0f, ui::labelFont (50.0f, 0.34f));

    // knob labels
    for (const auto& label : knobLabels)
    {
        g.setColour (colours::label);
        ui::drawText (g, label.text, label.x, label.baseline, ui::labelFont (label.size));
    }

    // wave switch symbols and filter switch labels
    {
        auto wave = [&g] (float x, float y, auto&& draw)
        {
            juce::Path path;
            draw (path, x, y);
            g.setColour (colours::label);
            g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        wave (688.0f, 303.0f, [] (juce::Path& p, float x, float y)
        {
            p.startNewSubPath (x - 11, y + 6); p.lineTo (x - 11, y - 6); p.lineTo (x, y - 6);
            p.lineTo (x, y + 6); p.lineTo (x + 11, y + 6); p.lineTo (x + 11, y - 6);
        });
        wave (688.0f, 341.0f, [] (juce::Path& p, float x, float y)
        {
            p.startNewSubPath (x - 11, y + 6); p.lineTo (x, y - 6); p.lineTo (x, y + 6);
            p.lineTo (x + 11, y - 6); p.lineTo (x + 11, y + 6);
        });
        wave (688.0f, 379.0f, [] (juce::Path& p, float x, float y)
        {
            p.startNewSubPath (x - 12, y + 6); p.lineTo (x - 6, y - 6); p.lineTo (x, y + 6);
            p.lineTo (x + 6, y - 6); p.lineTo (x + 12, y + 6);
        });

        ui::drawText (g, "BP", 863.0f, 308.0f, ui::labelFont (14.0f), juce::Justification::left);
        ui::drawText (g, "LP", 863.0f, 378.0f, ui::labelFont (14.0f), juce::Justification::left);
    }

    // button labels
    {
        g.setColour (colours::label);
        g.drawRect (juce::Rectangle<float> (42.0f, buttonY + 27.0f, 60.0f, 22.0f), 1.6f);
        ui::drawText (g, "SHIFT", 72.0f, buttonY + 43.0f, ui::labelFont (12.5f));

        const auto iconY = buttonY + 38.0f;
        juce::Path play, stop, rec;
        play.addTriangle (161.0f, iconY - 9.0f, 177.0f, iconY, 161.0f, iconY + 9.0f);
        stop.addRectangle (254.0f, iconY - 8.0f, 16.0f, 16.0f);
        rec.addEllipse (348.0f, iconY - 9.0f, 18.0f, 18.0f);

        for (auto* icon : { &play, &stop, &rec })
            g.strokePath (*icon, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered));

        // ◂ BAR ▸
        juce::Path arrows;
        arrows.addTriangle (1209.0f, buttonY + 39.0f, 1215.0f, buttonY + 35.5f, 1215.0f, buttonY + 42.5f);
        arrows.addTriangle (1312.0f, buttonY + 39.0f, 1306.0f, buttonY + 35.5f, 1306.0f, buttonY + 42.5f);
        g.fillPath (arrows);
        ui::drawText (g, "BAR", 1261.0f, buttonY + 44.0f, ui::labelFont (14.0f));
        ui::drawText (g, "-1 OCT", 1404.0f, buttonY + 44.0f, ui::labelFont (14.0f));
        ui::drawText (g, "+1 OCT", 1500.0f, buttonY + 44.0f, ui::labelFont (14.0f));
    }

    // step key labels (Shift lights them)
    for (int i = 0; i < numKeys; ++i)
    {
        const juce::String text (keyLabels[i]);
        g.setColour (shift ? juce::Colours::white : colours::label.withAlpha (0.92f));
        ui::drawText (g, text, keyX (i), keyY + 62.0f, ui::labelFont (text.length() > 7 ? 10.5f : 12.5f, 0.0f));
    }

    // model name
    g.setColour (colours::title);
    ui::drawText (g, "SB-64", 48.0f, 909.0f, ui::labelFont (30.0f, 0.18f), juce::Justification::left);
    g.setColour (colours::labelDim);
    ui::drawText (g, "64 STEP ANALOG BASS", 1535.0f, 905.0f, ui::labelFont (14.0f), juce::Justification::right);
}

//==============================================================================
SB64AudioProcessorEditor::SB64AudioProcessorEditor (SB64AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    addAndMakeVisible (panel);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) FrontPanel::designWidth / FrontPanel::designHeight);
    setResizeLimits (minWidth, minWidth * FrontPanel::designHeight / FrontPanel::designWidth,
                     maxWidth, maxWidth * FrontPanel::designHeight / FrontPanel::designWidth);

    const auto saved = processor.getEditorWidth();
    const auto width = juce::jlimit (minWidth, maxWidth, saved > 0 ? saved : defaultWidth);
    setSize (width, juce::roundToInt ((double) width * FrontPanel::designHeight / FrontPanel::designWidth));
    sizeRestored = true;
}

void SB64AudioProcessorEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) FrontPanel::designWidth));

    if (sizeRestored)
        processor.setEditorWidth (getWidth());
}
