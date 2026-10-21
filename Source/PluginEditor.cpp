#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "Menu.h"
#include "Parameters.h"
#include "dsp/Scales.h"
#include "dsp/Sequencer.h"

#include <utility>

namespace
{
    namespace id = sb64::params::id;
    namespace ui = sb64::ui;
    namespace seq = sb64::seq;
    namespace colours = sb64::ui::colours;
    using sb64::Patterns;

    constexpr float keyY = 791.0f, buttonY = 697.0f;
    float keyX (int key) { return 73.0f + 95.1f * (float) key; }

    const char* const keyLabels[] { "ACC", "SLIDE", "MOD", "PITCH ENV", "LAST STEP", "PLAY MODE", "TRANSP", "SCALE",
                                    "ARPEGG", "LFO", "RND PATT", "CLEAR", "COPY", "PASTE", "BANK", "PATTERN" };

    // shift functions on the step keys
    enum Function { fnAccent = 0, fnSlide = 1, fnMod = 2, fnPitchEnv = 3, fnLastStep = 4, fnPlayMode = 5, fnTranspose = 6,
                    fnScale = 7, fnArp = 8, fnLfo = 9, fnRandom = 10, fnClear = 11, fnCopy = 12, fnPaste = 13,
                    fnBank = 14, fnPattern = 15 };

    juce::String signedText (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); }

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

    void setParameter (juce::AudioProcessorValueTreeState& state, const char* paramId, float plainValue)
    {
        if (auto* parameter = state.getParameter (paramId))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
            parameter->endChangeGesture();
        }
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
    auto& rate = knob (id::lfoRate, 1063, 550, 48);
    rate.onTouch = [this]
    {
        lfoViewUntil = juce::Time::getMillisecondCounterHiRes() + 2000.0;
        showText (sb64::menu::valueText (processor.getState(), id::lfoRate));
    };
    knob (id::lfoLevel, 1270, 550, 48);
    knob (id::envAmount, 1470, 550, 48);

    // SELECT/GATE and BACK/NOTE: the selected step's gate and note, else the menu
    auto& select = make<ui::Encoder> (52.0f);
    select.place ({ 685, 605 });
    select.onStep = [this] (int steps, bool fine) { turnSelect (steps, fine); };
    select.onClick = [this] { clickSelect(); };

    auto& note = make<ui::Encoder> (52.0f);
    note.place ({ 890, 605 });
    note.onStep = [this] (int steps, bool fine) { turnNote (steps, fine); };
    note.onClick = [this] { back(); };

    display = &make<ui::SegmentDisplay>();
    display->setBounds (682, 409, 206, 92);

    gateLed = &make<ui::Led> (colours::led);
    gateLed->place ({ 663, 260 });
    accentLed = &make<ui::Led> (colours::ledYellow);
    accentLed->place ({ 1053, 258 });
    lfoLed = &make<ui::Led> (colours::led);
    lfoLed->place ({ 1137, 470 });

    // buttons
    shiftButton = &make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    shiftButton->place ({ 72, buttonY });
    shiftButton->onPress = [this] { setShift (! shift); };

    playButton = &make<ui::RoundButton> (ui::RoundButton::Cap::pink);
    playButton->place ({ 168, buttonY });
    playButton->onPress = [this] { play(); };

    auto& stopButton = make<ui::RoundButton> (ui::RoundButton::Cap::pink);
    stopButton.place ({ 262, buttonY });
    stopButton.onPress = [this] { stop(); };

    recButton = &make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    recButton->place ({ 357, buttonY });
    recButton->onPress = [this]
    {
        if (shift)
        {
            setShift (false);
            setKeyboardMode (! keyboard);
            return;
        }

        setRecording (! processor.isRecording());
    };

    auto& barDown = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    barDown.place ({ 1213, buttonY });
    barDown.onPress = [this] { showBar (bar - 1); };

    auto& barUp = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    barUp.place ({ 1308, buttonY });
    barUp.onPress = [this] { showBar (bar + 1); };

    auto& octaveDown = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    octaveDown.place ({ 1404, buttonY });
    octaveDown.onPress = [this] { octave (-1); };

    auto& octaveUp = make<ui::RoundButton> (ui::RoundButton::Cap::grey);
    octaveUp.place ({ 1500, buttonY });
    octaveUp.onPress = [this] { octave (1); };

    for (int i = 0; i < numKeys; ++i)
    {
        auto& key = make<ui::StepKey> (i % 4 == 0);
        key.place ({ keyX (i), keyY });
        key.onPress = [this, i] (const juce::MouseEvent& e) { pressKey (i, e.mods.isShiftDown()); };
        key.onRelease = [this, i] { releaseKey (i); };
        keys[(size_t) i] = &key;
    }

    updateKeys (juce::Time::getMillisecondCounterHiRes());
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

seq::Pattern& FrontPanel::pattern()
{
    return processor.getPatterns().selected();
}

void FrontPanel::showText (const juce::String& text, double seconds)
{
    transientText = text;
    transientUntil = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
    updateDisplay (juce::Time::getMillisecondCounterHiRes());
}

//==============================================================================
// Menu

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

void FrontPanel::openMenuAt (const char* paramId)
{
    const auto& items = sb64::menu::items();

    for (size_t i = 0; i < items.size(); ++i)
        if (juce::String (items[i].paramId) == paramId)
            menuIndex = (int) i;

    menuUntil = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    selectMenu (0);
}

void FrontPanel::changeMenuValue (int steps, bool fine)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now >= menuUntil)
    {
        selectMenu (0);   // shows which setting NOTE changes
        return;
    }

    const juce::String item (sb64::menu::items()[(size_t) menuIndex].paramId);

    if (item == sb64::menu::presetItem)
        processor.setCurrentProgram (processor.getCurrentProgram() + steps);
    else
        sb64::menu::step (processor.getState(), item, steps, fine);

    transientUntil = menuNameUntil = 0.0;
    menuUntil = now + 5000.0;
    updateDisplay (now);
}

//==============================================================================
// Encoders

juce::String FrontPanel::stepFieldText (const seq::Step& step) const
{
    switch (stepField)
    {
        case StepField::chance:
            if (step.chance <= 0)
                return "ALL";
            if (step.chance < seq::firstRatio)
                return juce::String (seq::chancePercent[step.chance]);
            return juce::String (step.chance - seq::firstRatio + 1) + "/8";

        case StepField::cent:
            return signedText (step.cent);

        case StepField::gate:
            break;
    }

    return juce::String (step.gate);
}

void FrontPanel::clickSelect()
{
    // with a step selected: switches between its gate, chance and fine tune
    if (selectedStep >= 0 && ! processor.isRecording() && keyMode == KeyMode::steps)
    {
        stepField = stepField == StepField::gate ? StepField::chance
                  : stepField == StepField::chance ? StepField::cent : StepField::gate;
        showText (stepField == StepField::gate ? "GAT" : stepField == StepField::chance ? "PRB" : "CNT", 0.8);
        return;
    }

    selectMenu (0);
}

bool FrontPanel::liveMod (int steps)
{
    const auto playing = processor.getDisplay().step.load();

    if (keyMode != KeyMode::mod || selectedStep >= 0 || ! processor.getDisplay().running.load() || playing < 0)
        return false;

    const auto now = juce::Time::getMillisecondCounterHiRes();

    // a new turn starts from the playing step's value
    if (now >= liveModUntil)
        liveModValue = pattern().get (playing).mod;

    liveModValue = juce::jlimit (seq::minMod, seq::maxMod, liveModValue + steps);
    auto step = pattern().get (playing);
    step.mod = liveModValue;
    pattern().set (playing, step);
    liveModStep = playing;
    liveModUntil = now + 1000.0;
    showText (signedText (liveModValue));
    return true;
}

bool FrontPanel::showsLfo (double now) const
{
    if (now < lfoViewUntil)
        return true;

    if (now >= menuUntil)
        return false;

    const juce::String item (sb64::menu::items()[(size_t) menuIndex].paramId);
    return item == id::lfoShape || item == id::lfoSync || item == id::lfoRange || item == id::lfoReset || item == id::lfoPhase;
}

void FrontPanel::turnSelect (int steps, bool fine)
{
    if (liveMod (steps * (fine ? 1 : 8)))
        return;

    if (selectedStep >= 0 && ! processor.isRecording())
    {
        auto step = pattern().get (selectedStep);

        if (keyMode == KeyMode::mod)
        {
            step.mod = juce::jlimit (seq::minMod, seq::maxMod, step.mod + steps * (fine ? 1 : 8));
            pattern().set (selectedStep, step);
            showText (signedText (step.mod));
            return;
        }

        if (keyMode == KeyMode::pitchEnv)
        {
            step.pitchDecay = juce::jlimit (0, seq::maxPitchEnv, step.pitchDecay + steps);
            pattern().set (selectedStep, step);
            showText ("DC" + juce::String (step.pitchDecay));
            return;
        }

        switch (stepField)
        {
            case StepField::gate:   step.gate = juce::jlimit (seq::minGate, seq::maxGate, step.gate + steps * (fine ? 1 : 5)); break;
            case StepField::chance: step.chance = juce::jlimit (0, seq::numChances - 1, step.chance + steps); break;
            case StepField::cent:   step.cent = juce::jlimit (-seq::maxCent, seq::maxCent, step.cent + steps * (fine ? 1 : 5)); break;
        }

        pattern().set (selectedStep, step);
        showText (stepFieldText (step));
        return;
    }

    selectMenu (steps);
}

void FrontPanel::turnNote (int steps, bool fine)
{
    if (liveMod (steps))
        return;

    if (keyMode == KeyMode::scale && scaleDegree >= 0 && editedScale() >= 0)
    {
        // detunes the picked note of the user scale
        auto& users = processor.getUserScales();
        const auto user = editedScale();
        users.setCents (user, scaleDegree, users.cents (user, scaleDegree) + steps * (fine ? 1 : 5));
        showText (signedText (users.cents (user, scaleDegree)));
        return;
    }

    if (selectedStep >= 0 && ! processor.isRecording() && keyMode == KeyMode::mod)
    {
        auto step = pattern().get (selectedStep);
        step.mod = juce::jlimit (seq::minMod, seq::maxMod, step.mod + steps);
        pattern().set (selectedStep, step);
        showText (signedText (step.mod));
        return;
    }

    if (selectedStep >= 0 && ! processor.isRecording() && keyMode == KeyMode::pitchEnv)
    {
        auto step = pattern().get (selectedStep);
        step.pitchDepth = juce::jlimit (0, seq::maxPitchEnv, step.pitchDepth + steps);
        pattern().set (selectedStep, step);
        showText (step.pitchDepth > 0 ? "PE" + juce::String (step.pitchDepth) : juce::String ("OFF"));
        return;
    }

    if (selectedStep >= 0 && ! processor.isRecording())
    {
        // turning the note also switches the step on
        auto step = pattern().get (selectedStep);
        step.note = juce::jlimit (seq::lowestNote, seq::highestNote, step.note + steps);
        step.on = true;
        pattern().set (selectedStep, step);
        showText (sb64::menu::noteName (step.note));
        return;
    }

    changeMenuValue (steps, fine);
}

void FrontPanel::back()
{
    // BACK: leaves a step, the menu, a pending function or the accent/slide view, one at a time
    if (pending != Pending::none)
        pending = Pending::none;
    else if (juce::Time::getMillisecondCounterHiRes() < menuUntil)
        menuUntil = 0.0;
    else if (selectedStep >= 0)
    {
        selectedStep = -1;
        stepField = StepField::gate;
    }
    else if (keyMode != KeyMode::steps)
        keyMode = KeyMode::steps;
    else if (keyboard)
        setKeyboardMode (false);

    transientUntil = 0.0;
}

//==============================================================================
// Transport

void FrontPanel::play()
{
    setParameter (processor.getState(), id::run, 1.0f);
}

void FrontPanel::stop()
{
    // STOP while stopped: back to the start
    if (processor.getState().getRawParameterValue (id::run)->load() < 0.5f)
    {
        processor.setRecordStep (pattern().first());
        showBar (pattern().first() / seq::stepsPerBar);
    }

    setParameter (processor.getState(), id::run, 0.0f);
    followPlayhead = true;
}

void FrontPanel::setRecording (bool on)
{
    processor.setRecording (on);

    if (on)
    {
        // step recording starts at the selected step (or the first)
        processor.setRecordStep (selectedStep >= 0 ? selectedStep : pattern().first());
        selectedStep = -1;
    }

    pending = Pending::none;
    showText (on ? "REC" : "---", 0.8);
}

void FrontPanel::setKeyboardMode (bool on)
{
    keyboard = on;
    selectedStep = -1;
    pending = Pending::none;

    for (int i = 0; i < numKeys; ++i)
        releaseKey (i);

    showText (on ? "KBD" : "STP", 0.8);
}

int FrontPanel::editedScale() const
{
    return juce::roundToInt (processor.getState().getRawParameterValue (id::scale)->load()) - seq::numFactoryScales;
}

void FrontPanel::showBar (int newBar)
{
    bar = juce::jlimit (0, seq::numBars - 1, newBar);
    followPlayhead = false;
    showText ("BR" + juce::String (bar + 1), 0.8);
}

void FrontPanel::octave (int direction)
{
    if (keyMode == KeyMode::transpose)
    {
        transposeOctave = juce::jlimit (-2, 1, transposeOctave + direction);
        showText ("O" + signedText (transposeOctave));
        return;
    }

    if (selectedStep >= 0 && ! processor.isRecording())
    {
        auto step = pattern().get (selectedStep);
        step.note = juce::jlimit (seq::lowestNote, seq::highestNote, step.note + 12 * direction);
        pattern().set (selectedStep, step);
        showText (sb64::menu::noteName (step.note));
        return;
    }

    baseNote = juce::jlimit (12, 84, baseNote + 12 * direction);
    showText (sb64::menu::noteName (baseNote));
}

//==============================================================================
// Step keys

void FrontPanel::pressKey (int key, bool shiftModifier)
{
    // SHIFT (latched) + key: the key's function
    if (shift)
    {
        setShift (false);
        function (key);
        return;
    }

    if (pending != Pending::none)
    {
        choose (key);
        return;
    }

    if (processor.isRecording() || keyboard)
    {
        // a keyboard; the computer's Shift key plays an accent
        releaseKey (key);
        const auto note = juce::jlimit (0, 127, baseNote + key);
        keyNotes[(size_t) key] = note;
        processor.getKeyboardState().noteOn (1, note, shiftModifier ? 1.0f : 0.7f);
        showText (sb64::menu::noteName (note), 0.8);
        return;
    }

    if (shiftModifier)
    {
        function (key);
        return;
    }

    const auto index = bar * seq::stepsPerBar + key;
    auto step = pattern().get (index);

    switch (keyMode)
    {
        case KeyMode::accent:
            step.accent = ! step.accent;
            pattern().set (index, step);
            break;

        case KeyMode::slide:
            step.slide = ! step.slide;
            pattern().set (index, step);
            break;

        case KeyMode::mod:
            // selects the step; the encoders set its value
            selectedStep = index;
            showText (signedText (step.mod), 0.8);
            break;

        case KeyMode::transpose:
        {
            // keys 1..13: up 0..12 semitones (plus the octave from ±1 OCT); key 16: back to 0
            const auto semitones = key < 13 ? key + 12 * transposeOctave : 0;
            processor.setLiveTranspose (semitones);
            transposeKey = key;
            showText ("T" + signedText (processor.getLiveTranspose()), 0.8);
            break;
        }

        case KeyMode::scale:
        {
            // keys 1..12: the notes above the root; a key adds or removes its note and picks it for NOTE
            if (key >= 12 || editedScale() < 0)
                break;

            auto& users = processor.getUserScales();
            const auto user = editedScale();

            if (key == scaleDegree || ((users.mask (user) >> key) & 1u) == 0)
                users.setMask (user, users.mask (user) ^ (1u << key));

            scaleDegree = key;
            const auto inScale = ((users.mask (user) >> key) & 1u) != 0;
            showText (inScale ? signedText (users.cents (user, key)) : juce::String ("OFF"), 0.8);
            break;
        }

        case KeyMode::pitchEnv:
            selectedStep = index;
            showText (step.pitchDepth > 0 ? "PE" + juce::String (step.pitchDepth) : juce::String ("OFF"), 0.8);
            break;

        case KeyMode::steps:
            // an empty step comes on and is selected; a selected one goes off again
            if (! step.on)
            {
                step.on = true;
                pattern().set (index, step);
            }
            else if (index == selectedStep)
            {
                step.on = false;
                pattern().set (index, step);
            }

            selectedStep = index;
            showText (step.on ? sb64::menu::noteName (step.note) : juce::String ("OFF"), 0.8);
            break;
    }
}

void FrontPanel::releaseKey (int key)
{
    // without latch, a transpose only lasts while its key is held
    if (keyMode == KeyMode::transpose && key == std::exchange (transposeKey, -1)
        && processor.getState().getRawParameterValue (id::transposeLatch)->load() < 0.5f)
        processor.setLiveTranspose (0);

    if (const auto note = std::exchange (keyNotes[(size_t) key], -1); note >= 0)
        processor.getKeyboardState().noteOff (1, note, 0.0f);
}

void FrontPanel::function (int key)
{
    auto& patterns = processor.getPatterns();

    switch (key)
    {
        case fnAccent:
        case fnSlide:
        case fnMod:
        case fnPitchEnv:
        {
            const auto mode = key == fnAccent ? KeyMode::accent : key == fnSlide ? KeyMode::slide
                            : key == fnMod ? KeyMode::mod : KeyMode::pitchEnv;
            keyMode = keyMode == mode ? KeyMode::steps : mode;
            selectedStep = -1;
            showText (keyMode == KeyMode::accent ? "ACC" : keyMode == KeyMode::slide ? "SLD"
                      : keyMode == KeyMode::mod ? "MOD" : keyMode == KeyMode::pitchEnv ? "PEN" : "STP", 0.8);
            break;
        }

        case fnPlayMode:
            pending = Pending::playMode;
            showText (seq::playModeNames[pattern().playMode()], 0.8);
            break;
        case fnTranspose:
            keyMode = keyMode == KeyMode::transpose ? KeyMode::steps : KeyMode::transpose;
            selectedStep = -1;
            showText (keyMode == KeyMode::transpose ? "T" + signedText (processor.getLiveTranspose()) : juce::String ("STP"), 0.8);
            break;
        case fnScale:
            // a user scale: the keys edit it; a factory scale: the menu picks one
            if (editedScale() >= 0)
            {
                keyMode = keyMode == KeyMode::scale ? KeyMode::steps : KeyMode::scale;
                scaleDegree = -1;
                selectedStep = -1;
                showText (keyMode == KeyMode::scale ? "U" + juce::String (editedScale() + 1) : juce::String ("STP"), 0.8);
            }
            else
            {
                openMenuAt (id::scale);
            }
            break;

        case fnArp:
        {
            const auto on = processor.getState().getRawParameterValue (id::arp)->load() < 0.5f;
            setParameter (processor.getState(), id::arp, on ? 1.0f : 0.0f);
            showText (on ? "ARP" : "OFF", 0.8);
            break;
        }

        case fnRandom:
            processor.randomisePattern();
            selectedStep = -1;
            showText ("RND", 0.8);
            break;

        case fnLastStep:
            // a second SHIFT + LAST STEP picks the first step instead
            pending = pending == Pending::lastStep ? Pending::firstStep : Pending::lastStep;
            showText (pending == Pending::lastStep ? "LST" : "FST", 0.8);
            break;
        case fnClear:    pending = Pending::clear;    showText ("CLR", 0.8); break;
        case fnBank:     pending = Pending::bank;     showText ("BNK", 0.8); break;
        case fnPattern:  pending = Pending::pattern;  showText ("PAT", 0.8); break;
        case fnLfo:      openMenuAt (id::lfoShape); break;

        case fnCopy:
            clipboard.copyFrom (pattern());
            hasClipboard = true;
            showText ("CPY", 0.8);
            break;

        case fnPaste:
            if (hasClipboard)
            {
                pattern().copyFrom (clipboard);
                showText ("PST", 0.8);
            }
            break;

        default:
            break;
    }

    juce::ignoreUnused (patterns);
}

void FrontPanel::choose (int key)
{
    auto& patterns = processor.getPatterns();
    const auto running = processor.getDisplay().running.load();
    const auto what = std::exchange (pending, Pending::none);

    switch (what)
    {
        case Pending::lastStep:
        {
            const auto last = bar * seq::stepsPerBar + key;
            pattern().setLast (last);
            showText ("L" + juce::String (last + 1));
            break;
        }

        case Pending::firstStep:
        {
            const auto first = bar * seq::stepsPerBar + key;
            pattern().setFirst (first);
            showText ("F" + juce::String (first + 1));
            break;
        }

        case Pending::playMode:
            if (key < seq::numPlayModes)
            {
                pattern().setPlayMode (key);
                showText (seq::playModeNames[key]);
            }
            break;

        case Pending::bank:
        {
            if (key >= Patterns::numBanks)
                return;

            const auto index = key * Patterns::patternsPerBank + patterns.selectedIndex() % Patterns::patternsPerBank;
            patterns.select (index, running);
            selectedStep = -1;
            showText (Patterns::name (index));
            break;
        }

        case Pending::pattern:
        {
            const auto index = patterns.selectedIndex() / Patterns::patternsPerBank * Patterns::patternsPerBank + key;

            // the pattern that's already selected: back to how it was when it was picked
            if (index == patterns.selectedIndex())
            {
                patterns.reload();
                selectedStep = -1;
                showText ("RLD");
                break;
            }

            patterns.select (index, running);
            selectedStep = -1;
            bar = 0;
            showText (Patterns::name (index));

            // further keys right after it chain patterns
            pending = Pending::chain;
            chainUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
            break;
        }

        case Pending::chain:
        {
            const auto index = patterns.selectedIndex() / Patterns::patternsPerBank * Patterns::patternsPerBank + key;
            patterns.addToChain (index);
            showText ("C" + juce::String (patterns.chainLength()), 0.8);
            pending = Pending::chain;
            chainUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
            break;
        }

        case Pending::clear:
            if (key == fnClear)
            {
                pattern().clear();
                selectedStep = -1;
                showText ("CLR");
            }
            break;

        case Pending::none:
            break;
    }
}

void FrontPanel::setShift (bool on)
{
    shift = on;
    shiftButton->setLit (on);
    repaint (0, 830, designWidth, 40);   // the step key labels
}

//==============================================================================

void FrontPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    auto& d = processor.getDisplay();

    if (d.peak.exchange (0.0f) >= 0.99f)
        clipUntil = now + 300.0;

    gateLed->setLit (d.gate.load() ? 1.0f : 0.0f);
    accentLed->setLit (d.accent.load() || now < clipUntil ? 1.0f : 0.0f);
    lfoLed->setLit (d.lfoHigh.load() ? 1.0f : 0.0f);

    // PLAY lights while the sequence runs and blinks while it waits for the host
    const auto running = d.running.load();
    const auto armed = processor.getState().getRawParameterValue (id::run)->load() > 0.5f;
    const auto blink = std::fmod (now, 500.0) < 250.0;
    playButton->setLit (running || (armed && blink));
    recButton->setLit (processor.isRecording());

    // MOD live: the value goes on into each step that comes along while the encoder is still moving
    if (keyMode == KeyMode::mod && now < liveModUntil && running && d.step.load() >= 0 && d.step.load() != liveModStep)
    {
        liveModStep = d.step.load();
        auto step = pattern().get (liveModStep);
        step.mod = liveModValue;
        pattern().set (liveModStep, step);
    }

    if (pending == Pending::chain && now > chainUntil)
        pending = Pending::none;

    if (! running)
        followPlayhead = true;
    else if (followPlayhead && d.step.load() >= 0)
        bar = d.step.load() / seq::stepsPerBar;

    updateKeys (now);
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
        const auto value = juce::String (item.paramId) == sb64::menu::presetItem
                               ? juce::String (sb64::presets::factory()[(size_t) processor.getCurrentProgram()].code)
                               : sb64::menu::valueText (processor.getState(), item.paramId);
        display->setText (now < menuNameUntil ? juce::String (item.name) : value);
    }
    else if (pending != Pending::none)
    {
        display->setText (pending == Pending::lastStep ? "LST" : pending == Pending::firstStep ? "FST"
                          : pending == Pending::playMode ? juce::String (seq::playModeNames[pattern().playMode()])
                          : pending == Pending::clear ? "CLR"
                          : pending == Pending::bank ? "BNK" : pending == Pending::chain ? "CHN" : "PAT");
    }
    else if (processor.isRecording() && ! processor.getDisplay().running.load())
    {
        display->setText ("R" + juce::String (processor.getRecordStep() + 1));   // the step a note goes into
    }
    else if (selectedStep >= 0)
    {
        const auto step = pattern().get (selectedStep);

        if (keyMode == KeyMode::mod)
            display->setText (signedText (step.mod));
        else if (keyMode == KeyMode::pitchEnv)
            display->setText (step.pitchDepth > 0 ? "PE" + juce::String (step.pitchDepth) : juce::String ("OFF"));
        else
            display->setText (step.on ? sb64::menu::noteName (step.note) : juce::String ("OFF"));
    }
    else
    {
        display->setText (juce::String (juce::roundToInt (processor.getDisplay().bpm.load())));
    }
}

void FrontPanel::updateKeys (double now)
{
    auto& patterns = processor.getPatterns();
    const auto& p = pattern();
    const auto blink = std::fmod (now, 400.0) < 200.0;
    const auto playing = processor.getDisplay().step.load();

    for (int k = 0; k < numKeys; ++k)
    {
        auto& key = *keys[(size_t) k];
        const auto index = bar * seq::stepsPerBar + k;
        const auto step = p.get (index);
        const auto inRange = index >= p.first() && index <= p.last();
        int state = 0;

        switch (pending)
        {
            case Pending::lastStep:
                state = index == p.last() ? 2 : (inRange ? 1 : 0);
                break;

            case Pending::firstStep:
                state = index == p.first() ? 2 : (inRange ? 1 : 0);
                break;

            case Pending::playMode:
                state = k >= seq::numPlayModes ? 0 : (k == p.playMode() ? 2 : 1);
                break;

            case Pending::bank:
                state = k >= Patterns::numBanks ? 0 : (k == patterns.selectedIndex() / Patterns::patternsPerBank ? 2 : 1);
                break;

            case Pending::chain:
            {
                const auto candidate = patterns.selectedIndex() / Patterns::patternsPerBank * Patterns::patternsPerBank + k;
                auto inChain = candidate == patterns.selectedIndex();
                for (int c = 0; c < patterns.chainLength(); ++c)
                    inChain = inChain || patterns.chainEntry (c) == candidate;
                state = inChain ? 2 : (patterns.pattern (candidate).isEmpty() ? 0 : 1);
                break;
            }

            case Pending::pattern:
            {
                const auto candidate = patterns.selectedIndex() / Patterns::patternsPerBank * Patterns::patternsPerBank + k;
                state = candidate == patterns.selectedIndex() ? (patterns.isQueued() && ! blink ? 0 : 2)
                                                              : (patterns.pattern (candidate).isEmpty() ? 0 : 1);
                break;
            }

            case Pending::clear:
                state = k == fnClear && blink ? 2 : 0;
                break;

            case Pending::none:
                if (showsLfo (now) && ! processor.isRecording() && ! keyboard && keyMode == KeyMode::steps && selectedStep < 0)
                {
                    // the LFO's cycle as a running light
                    state = k == juce::jlimit (0, numKeys - 1, (int) (processor.getDisplay().lfoPhase.load() * numKeys)) ? 2 : 0;
                }
                else if (keyMode == KeyMode::transpose && ! processor.isRecording() && ! keyboard)
                {
                    // the transpose key lights; key 1 (no change) glows dimly
                    const auto lit = processor.getLiveTranspose() - 12 * transposeOctave;
                    state = k < 13 && k == lit ? 2 : (k == 0 || k == 15 ? 1 : 0);
                }
                else if (keyMode == KeyMode::scale && ! processor.isRecording() && ! keyboard)
                {
                    // the user scale's notes; the picked one blinks
                    const auto user = editedScale();
                    const auto inScale = user >= 0 && k < 12 && ((processor.getUserScales().mask (user) >> k) & 1u) != 0;
                    state = k == scaleDegree ? (blink ? 2 : 0) : (inScale ? 2 : 0);
                }
                else if (processor.isRecording() || keyboard)
                {
                    // a keyboard: playing notes light up, the white keys glow dimly
                    const auto note = baseNote + k;
                    state = processor.getKeyboardState().isNoteOnForChannels (0xffff, note) ? 2 : (isNatural (note) ? 1 : 0);
                }
                else if (keyMode == KeyMode::steps)
                {
                    state = ! inRange ? 0 : (index == playing ? 2 : (step.on ? 1 : 0));

                    if (index == selectedStep)
                        state = blink ? 2 : 0;
                }
                else if (keyMode == KeyMode::mod || keyMode == KeyMode::pitchEnv)
                {
                    const auto set = keyMode == KeyMode::mod ? step.mod != 0 : step.pitchDepth > 0;
                    state = index == selectedStep ? (blink ? 2 : 0) : (set ? 2 : (index == playing ? 1 : 0));
                }
                else
                {
                    const auto flag = keyMode == KeyMode::accent ? step.accent : step.slide;
                    state = flag ? 2 : (index == playing ? 1 : 0);
                }
                break;
        }

        key.setState (state);
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
