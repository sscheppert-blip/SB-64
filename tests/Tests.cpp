// Offline tests for the SB-64 voice and processor, plus render/snapshot tools.

#include "Menu.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Patterns.h"
#include "dsp/Generator.h"
#include "dsp/Lfo.h"
#include "dsp/Scales.h"
#include "dsp/Voice.h"

#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace
{
    namespace dsp = sb64::dsp;
    namespace id = sb64::params::id;

    int failures = 0;

    void check (bool condition, const char* what)
    {
        std::printf ("%s  %s\n", condition ? "PASS" : "FAIL", what);

        if (! condition)
            ++failures;
    }

    constexpr double testRate = 192000.0;

    std::vector<float> renderNote (const dsp::Settings& s, double seconds, float note = 45.0f, bool accent = false,
                                   double gateSeconds = -1.0)
    {
        dsp::Voice voice;
        voice.setSettings (s);
        voice.prepare (testRate);
        voice.noteOn (note, accent, dsp::Transition::retrigger);

        std::vector<float> out ((size_t) (seconds * testRate));
        const auto gateEnd = gateSeconds < 0.0 ? out.size() : (size_t) (gateSeconds * testRate);
        voice.render (out.data(), (int) gateEnd);

        if (gateEnd < out.size())
        {
            voice.noteOff();
            voice.render (out.data() + gateEnd, (int) (out.size() - gateEnd));
        }

        return out;
    }

    float peak (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
    {
        float p = 0.0f;
        for (auto i = from; i < std::min (to, x.size()); ++i)
            p = std::max (p, std::abs (x[i]));
        return p;
    }

    double rms (const std::vector<float>& x, size_t from, size_t to)
    {
        double e = 0.0;
        to = std::min (to, x.size());
        for (auto i = from; i < to; ++i)
            e += (double) x[i] * x[i];
        return std::sqrt (e / (double) std::max<size_t> (1, to - from));
    }

    bool allFinite (const std::vector<float>& x)
    {
        for (auto v : x)
            if (! std::isfinite (v))
                return false;
        return true;
    }

    // upward zero crossings per second (pitch of a clean periodic signal)
    double frequencyOf (const std::vector<float>& x, size_t from, size_t to)
    {
        int crossings = 0;
        size_t first = 0, last = 0;
        to = std::min (to, x.size());

        for (auto i = from + 1; i < to; ++i)
        {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                if (crossings == 0) first = i;
                last = i;
                ++crossings;
            }
        }

        return crossings > 1 ? (crossings - 1) * testRate / (double) (last - first) : 0.0;
    }

    // a clean, open voice for pitch measurements
    dsp::Settings openSettings (dsp::Wave wave)
    {
        dsp::Settings s;
        s.wave = wave;
        s.cutoff = 0.62f;
        s.resonance = 0.0f;
        s.envAmount = 0.0f;
        s.drive = 0.0f;
        s.vcaEnv = 1.0f;
        return s;
    }

    void setParam (SB64AudioProcessor& processor, const juce::String& paramId, float plainValue)
    {
        auto* parameter = processor.getState().getParameter (paramId);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }
}

//==============================================================================
static void testOscillator()
{
    const auto expected = 440.0 * std::pow (2.0, (45 - 69) / 12.0);   // A2, 110 Hz

    for (auto wave : { dsp::Wave::saw, dsp::Wave::square, dsp::Wave::triangle })
    {
        const auto out = renderNote (openSettings (wave), 0.5);
        const auto f = frequencyOf (out, (size_t) (0.1 * testRate), out.size());
        check (std::abs (f - expected) < 0.5, "oscillator plays the note's frequency");
        check (allFinite (out) && peak (out) < 1.0f, "oscillator output is finite and below full scale");
    }

    // the sub alone is one octave down
    auto s = openSettings (dsp::Wave::square);
    s.subLevel = 1.0f;
    s.cutoff = 0.3f;   // ~200 Hz: mostly the sub's fundamental and the main one left
    const auto out = renderNote (s, 0.5);
    std::vector<float> sub (out.size());
    // remove the main oscillator by averaging over its period (the sub stays)
    const auto period = (size_t) std::lround (testRate / expected);
    double acc = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        acc += out[i];
        if (i >= period) acc -= out[i - period];
        sub[i] = (float) (acc / (double) period);
    }
    const auto fSub = frequencyOf (sub, (size_t) (0.1 * testRate), sub.size());
    check (std::abs (fSub - expected * 0.5) < 0.5, "sub oscillator is one octave down");

    // tune shifts A
    s = openSettings (dsp::Wave::saw);
    s.tuneHz = 10.0f;
    const auto tuned = renderNote (s, 0.5, 69.0f);
    check (std::abs (frequencyOf (tuned, (size_t) (0.1 * testRate), tuned.size()) - 450.0) < 1.0, "tune moves A to 450 Hz");
}

static void testEnvelopes()
{
    dsp::Settings s;
    s.vcaEnv = 0.2f;   // short
    auto shortNote = renderNote (s, 1.0);
    s.vcaEnv = 0.8f;
    auto longNote = renderNote (s, 1.0);
    const auto late = (size_t) (0.4 * testRate);
    check (rms (shortNote, late, late + 9600) < rms (longNote, late, late + 9600) * 0.2, "VCA ENV sets the decay");

    // top of the knob: a drone that doesn't decay, not even after the gate
    s.vcaEnv = 1.0f;
    const auto drone = renderNote (s, 2.0, 45.0f, false, 0.2);
    check (rms (drone, (size_t) (1.7 * testRate), (size_t) (1.9 * testRate)) > 0.5 * rms (drone, (size_t) (0.1 * testRate), (size_t) (0.19 * testRate)),
           "VCA ENV at the top drones");

    // the gate ends the note in a fraction of the decay
    s.vcaEnv = 0.8f;
    const auto gated = renderNote (s, 1.0, 45.0f, false, 0.1);
    check (rms (gated, late, late + 9600) < rms (longNote, late, late + 9600) * 0.1, "the end of the gate releases the note");

    // accent is louder
    s.vcaEnv = 0.5f;
    const auto normal = renderNote (s, 0.2);
    const auto accented = renderNote (s, 0.2, 45.0f, true);
    check (rms (accented, 0, accented.size()) > rms (normal, 0, normal.size()) * 1.2, "accent is louder");

    // filter envelope: brighter at the start than later
    s = dsp::Settings {};
    s.vcaEnv = 1.0f;
    s.cutoff = 0.32f;
    s.envAmount = 1.0f;
    s.resonance = 0.0f;
    s.vcfEnv = 0.5f;
    const auto swept = renderNote (s, 1.0);
    auto brightness = [&swept] (double at)
    {
        // energy of the first difference relative to the signal: high when bright
        const auto from = (size_t) (at * testRate), to = from + 4800;
        double d = 0.0, e = 0.0;
        for (auto i = from + 1; i < to; ++i)
        {
            d += std::pow (swept[i] - swept[i - 1], 2.0);
            e += std::pow (swept[i], 2.0);
        }
        return d / std::max (1.0e-12, e);
    };
    check (brightness (0.01) > 4.0 * brightness (0.8), "VCF ENV opens the filter at the start of a note");
}

static void testFilter()
{
    // full resonance oscillates on its own, low-pass and band-pass
    for (auto mode : { dsp::FilterMode::lowPass, dsp::FilterMode::bandPass })
    {
        dsp::Settings s;
        s.filter = mode;
        s.resonance = 1.0f;
        s.cutoff = 0.5f;
        s.envAmount = 0.0f;
        s.vcaEnv = 1.0f;
        s.wave = dsp::Wave::triangle;
        const auto out = renderNote (s, 1.0, 24.0f);
        check (allFinite (out), "self-oscillating filter stays finite");
        check (rms (out, (size_t) (0.7 * testRate), out.size()) > 0.05, "full resonance self-oscillates");
        std::printf ("      self-oscillation peak %.3f, rms %.3f\n", peak (out), rms (out, (size_t) (0.7 * testRate), out.size()));
        check (peak (out) < 2.0f, "self-oscillation is bounded");
    }

    // resonance keeps the bass (low-pass): the level of a low note hardly drops
    dsp::Settings s = openSettings (dsp::Wave::saw);
    s.cutoff = 0.45f;
    s.resonance = 0.0f;
    const auto flat = renderNote (s, 0.5, 33.0f);
    s.resonance = 0.85f;
    const auto resonant = renderNote (s, 0.5, 33.0f);
    // compare the fundamental region with a 60 Hz low-pass (moving average over 1/60 s)
    auto lowBand = [] (const std::vector<float>& x)
    {
        const auto n = (size_t) (testRate / 120.0);
        std::vector<float> y (x.size());
        double acc = 0.0;
        for (size_t i = 0; i < x.size(); ++i) { acc += x[i]; if (i >= n) acc -= x[i - n]; y[i] = (float) (acc / (double) n); }
        return rms (y, (size_t) (0.2 * testRate), y.size());
    };
    check (lowBand (resonant) > 0.6 * lowBand (flat), "high resonance keeps the bass");
}

static void testSlideAndPitchEnv()
{
    dsp::Voice voice;
    auto s = openSettings (dsp::Wave::saw);
    s.slideSeconds = 0.1f;
    voice.setSettings (s);
    voice.prepare (testRate);
    voice.noteOn (45.0f, false, dsp::Transition::retrigger);

    std::vector<float> out ((size_t) (0.6 * testRate));
    voice.render (out.data(), (int) (0.2 * testRate));
    voice.noteOn (57.0f, false, dsp::Transition::slide);   // slide up an octave
    voice.render (out.data() + (size_t) (0.2 * testRate), (int) (out.size() - (size_t) (0.2 * testRate)));

    const auto before = frequencyOf (out, (size_t) (0.1 * testRate), (size_t) (0.2 * testRate));
    const auto during = frequencyOf (out, (size_t) (0.21 * testRate), (size_t) (0.24 * testRate));
    const auto after = frequencyOf (out, (size_t) (0.4 * testRate), out.size());
    check (std::abs (before - 110.0) < 1.0 && std::abs (after - 220.0) < 1.0, "slide arrives at the new note");
    check (during > 120.0 && during < 215.0, "slide glides through the pitches in between");

    // pitch envelope: starts higher and falls back
    s = openSettings (dsp::Wave::saw);
    s.pitchEnvDepth = 9;
    s.pitchEnvDecay = 0.3f;
    const auto peow = renderNote (s, 0.5);
    check (frequencyOf (peow, 0, (size_t) (0.004 * testRate)) > 400.0, "pitch envelope starts high");
    check (std::abs (frequencyOf (peow, (size_t) (0.35 * testRate), peow.size()) - 110.0) < 1.0, "pitch envelope falls back to the note");
}

static void testLfo()
{
    bool inRange = true, unipolar = true;

    for (int shape = 0; shape < 8; ++shape)
    {
        for (int i = 0; i < 1000; ++i)
        {
            const auto v = dsp::lfoWave ((dsp::LfoShape) shape, (float) i / 1000.0f);
            inRange = inRange && v >= -1.0001f && v <= 1.0001f;
            if (shape >= (int) dsp::LfoShape::tripleSaw)
                unipolar = unipolar && v >= 0.0f;
        }
    }

    check (inRange, "LFO shapes stay within -1..1");
    check (unipolar, "the three special LFO shapes are unipolar");
    check (std::abs (dsp::lfoWave (dsp::LfoShape::tripleRamp, 0.3333f) - 0.3f) < 0.01f
           && std::abs (dsp::lfoWave (dsp::LfoShape::tripleRamp, 0.6666f) - 0.6f) < 0.01f
           && std::abs (dsp::lfoWave (dsp::LfoShape::tripleRamp, 0.999f) - 1.0f) < 0.01f, "triple ramp rises to 30, 60 and 100 %");

    dsp::Lfo lfo;
    lfo.prepare (1000.0);
    lfo.setShape (dsp::LfoShape::noise);
    lfo.setFrequency (5.0f);
    float lo = 0.0f, hi = 0.0f;
    for (int i = 0; i < 5000; ++i) { const auto v = lfo.next(); lo = std::min (lo, v); hi = std::max (hi, v); }
    check (lo < -0.3f && hi > 0.3f && lo >= -1.0f && hi <= 1.0f, "noise LFO wanders within range");

    // audio-rate FM changes the sound without blowing up
    auto s = openSettings (dsp::Wave::saw);
    s.fmLevel = 1.0f;
    s.lfoHz = dsp::lfoHz (1.0f);
    const auto fm = renderNote (s, 0.3);
    check (allFinite (fm) && peak (fm) < 1.0f, "audio-rate FM stays finite and in range");
}

static void testDetuneAndDrive()
{
    auto s = openSettings (dsp::Wave::saw);
    const auto dry = renderNote (s, 0.6);
    s.detune = 1.0f;
    const auto wide = renderNote (s, 0.6);
    double diff = 0.0;
    for (size_t i = (size_t) (0.2 * testRate); i < dry.size(); ++i)
        diff += std::pow (dry[i] - wide[i], 2.0);
    check (allFinite (wide) && diff > 1.0, "DETUNE changes the sound");
    // the comb filtering adds zero crossings, so measure the pitch below the overtones
    s.cutoff = 0.3f;
    const auto low = renderNote (s, 0.6);
    check (std::abs (frequencyOf (low, (size_t) (0.2 * testRate), low.size()) - 110.0) < 3.0, "DETUNE keeps the pitch");

    s = dsp::Settings {};
    s.drive = 1.0f;
    s.resonance = 1.0f;
    s.envAmount = 1.0f;
    s.subLevel = 1.0f;
    s.noiseLevel = 1.0f;
    s.volume = 1.0f;
    const auto hot = renderNote (s, 0.5, 30.0f, true);
    std::printf ("      everything at full: peak %.3f\n", peak (hot));
    check (allFinite (hot) && peak (hot) < 1.5f, "everything at full stays finite");
}

static void testProcessor()
{
    SB64AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);

    auto run = [&] (juce::MidiBuffer midi, int blocks)
    {
        float p = 0.0f;
        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            auto events = i == 0 ? midi : juce::MidiBuffer();
            processor.processBlock (buffer, events);
            p = std::max (p, buffer.getMagnitude (0, 0, 512));
        }
        return p;
    };

    check (run ({}, 4) == 0.0f, "silent without notes");

    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 90), 10);
    const auto level = run (on, 20);
    check (level > 0.05f && level < 1.0f, "a MIDI note plays below full scale");
    check (processor.getDisplay().gate.load(), "gate LED follows the note");

    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
    run (off, 100);
    check (run ({}, 2) < 0.01f && ! processor.getDisplay().gate.load(), "note off fades out");

    // the step keys play through the keyboard state
    processor.getKeyboardState().noteOn (1, 40, 0.7f);
    check (run ({}, 10) > 0.05f, "panel keys play notes");
    processor.getKeyboardState().noteOff (1, 40, 0.0f);
    run ({}, 100);

    // accent from the velocity
    juce::MidiBuffer hard;
    hard.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 127), 0);
    run (hard, 4);
    check (processor.getDisplay().accent.load(), "velocity above the threshold accents");

    // state round trip
    setParam (processor, id::cutoff, 0.77f);
    setParam (processor, id::lfoShape, 7.0f);
    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    SB64AudioProcessor other;
    other.setStateInformation (saved.getData(), (int) saved.getSize());
    check (std::abs (other.getState().getRawParameterValue (id::cutoff)->load() - 0.77f) < 1.0e-4f
           && juce::roundToInt (other.getState().getRawParameterValue (id::lfoShape)->load()) == 7, "state saves and loads");
}

static void testMenu()
{
    SB64AudioProcessor processor;
    auto& state = processor.getState();

    check (sb64::menu::valueText (state, id::lfoShape) == "SIN", "menu shows the LFO shape");
    sb64::menu::step (state, id::lfoShape, 9, false);
    check (sb64::menu::valueText (state, id::lfoShape) == "NOS", "NOTE steps through the shapes");
    sb64::menu::step (state, id::lfoShape, 5, false);
    check (sb64::menu::valueText (state, id::lfoShape) == "NOS", "stepping stops at the end");

    setParam (processor, id::lfoSync, 1.0f);
    setParam (processor, id::lfoRate, 0.0f);
    check (sb64::menu::valueText (state, id::lfoRate) == "/64", "synced LFO rate shows the division");
    setParam (processor, id::lfoRange, 1.0f);
    setParam (processor, id::lfoRate, 1.0f);
    check (sb64::menu::valueText (state, id::lfoRate) == "32X", "high range reaches 32x");
    check (sb64::menu::noteName (36) == "C2" && sb64::menu::noteName (61) == "C#4", "note names");

    for (const auto& item : sb64::menu::items())
        check ((state.getParameter (item.paramId) != nullptr || juce::String (item.paramId) == sb64::menu::presetItem)
               && juce::String (item.name).length() == 3, item.name);
}

static void testEditor()
{
    SB64AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    check (editor != nullptr && editor->getWidth() > 0, "editor opens");

    // turning a knob from the panel changes its parameter
    auto* panel = editor->getChildComponent (0);
    sb64::ui::Knob* cutoff = nullptr;
    for (auto* child : panel->getChildren())
        if (auto* k = dynamic_cast<sb64::ui::Knob*> (child); k != nullptr && k->paramId == id::cutoff)
            cutoff = k;

    check (cutoff != nullptr, "panel has a cutoff knob");

    if (cutoff != nullptr)
    {
        const auto before = processor.getState().getRawParameterValue (id::cutoff)->load();
        const auto centre = cutoff->getLocalBounds().getCentre().toFloat();
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto time = juce::Time::getCurrentTime();
        auto event = [&] (juce::Point<float> at)
        {
            return juce::MouseEvent (source, at, {}, juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                     cutoff, cutoff, time, centre, time, 1, false);
        };
        cutoff->mouseDown (event (centre));
        cutoff->mouseDrag (event (centre - juce::Point<float> (0.0f, 48.0f)));
        cutoff->mouseUp (event (centre - juce::Point<float> (0.0f, 48.0f)));
        const auto after = processor.getState().getRawParameterValue (id::cutoff)->load();
        check (std::abs (after - before - 0.2f) < 0.01f, "dragging a knob up turns it up");
    }
}

//==============================================================================
// Sequencer

namespace
{
    namespace seq = sb64::seq;

    // Host transport for the processor tests
    struct TestPlayHead final : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        bool playing = false;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (playing);
            return info;
        }
    };

    // 48 kHz, blocks of 100 samples; at 120 BPM a 16th lasts 6000 samples (60 blocks)
    struct SeqRig
    {
        SB64AudioProcessor processor;
        juce::AudioBuffer<float> buffer { 2, 100 };
        TestPlayHead head;
        int64_t sample = 0;

        SeqRig (bool host = false)
        {
            processor.setPlayConfigDetails (0, 2, 48000.0, 100);
            processor.prepareToPlay (48000.0, 100);
            setParam (processor, id::clock, host ? 1.0f : 0.0f);
            setParam (processor, id::tempo, 120.0f);
            setParam (processor, id::vcaEnv, 0.97f);   // a drone: the level only drops when the gate ends
            pattern().clear();

            if (host)
                processor.setPlayHead (&head);
        }

        seq::Pattern& pattern() { return processor.getPatterns().selected(); }

        void setStep (int index, int note, int gate = 50, bool accent = false, bool slide = false)
        {
            seq::Step s;
            s.on = true;
            s.note = note;
            s.gate = gate;
            s.accent = accent;
            s.slide = slide;
            pattern().set (index, s);
        }

        // runs the given number of blocks; returns the gate state after each one
        std::vector<bool> run (int blocks, juce::MidiBuffer midi = {})
        {
            std::vector<bool> gates;

            for (int i = 0; i < blocks; ++i)
            {
                buffer.clear();
                auto events = i == 0 ? midi : juce::MidiBuffer();
                processor.processBlock (buffer, events);
                if (head.playing)
                    head.ppq += 100.0 * head.bpm / 60.0 / 48000.0;
                sample += 100;
                gates.push_back (processor.getDisplay().gate.load());
            }

            return gates;
        }

        int step() { return processor.getDisplay().step.load(); }
    };
}

static void testSequencer()
{
    {
        SeqRig rig;
        rig.setStep (0, 45, 50);
        rig.setStep (4, 47, 25);
        rig.run (5);
        check (! rig.processor.getDisplay().running.load() && rig.step() < 0, "the sequencer waits for PLAY");

        setParam (rig.processor, id::run, 1.0f);
        auto gates = rig.run (60 * 6);
        check (gates[0] && gates[25] && ! gates[31], "gate 50 % lasts half a step");
        check (! gates[61] && ! gates[200], "empty steps stay silent");
        check (gates[241] && gates[250] && ! gates[257], "gate 25 % on step 5");
        check (rig.step() == 5, "the playing step advances with the clock");

        // last step: four steps loop
        rig.pattern().setLast (3);
        rig.run (60 * 4);
        bool inRange = true;
        for (int i = 0; i < 60 * 8; ++i)
        {
            rig.run (1);
            inRange = inRange && rig.step() >= 0 && rig.step() <= 3;
        }
        check (inRange, "LAST STEP shortens the loop");

        setParam (rig.processor, id::run, 0.0f);
        rig.run (2);
        check (! rig.processor.getDisplay().running.load() && ! rig.processor.getDisplay().gate.load(), "STOP ends the note");
    }

    {
        // slide holds the gate into the next step; gate 100 % joins the next note too
        SeqRig rig;
        rig.setStep (0, 45, 50, false, true);
        rig.setStep (1, 57, 100);
        rig.setStep (2, 45, 50);
        setParam (rig.processor, id::run, 1.0f);
        const auto gates = rig.run (60 * 3);
        bool held = true;
        for (int i = 0; i < 150; ++i)
            held = held && gates[(size_t) i];
        check (held, "slide and gate 100 % keep the gate on across steps");
        check (! gates[175], "a normal step after them ends its gate");
    }

    {
        // swing delays every second 16th by up to half a step
        SeqRig rig;
        rig.setStep (0, 45, 10);
        rig.setStep (1, 45, 10);
        setParam (rig.processor, id::swing, 50.0f);
        setParam (rig.processor, id::run, 1.0f);
        const auto gates = rig.run (120);
        check (! gates[65] && ! gates[85] && gates[92], "swing delays the offbeat");
    }

    {
        // host clock: the step follows the bar position
        SeqRig rig (true);
        for (int i = 0; i < 16; ++i)
            rig.setStep (i, 45);
        setParam (rig.processor, id::run, 1.0f);
        rig.head.ppq = 1.0;   // beat 2 = step 5
        rig.run (2);
        check (rig.step() < 0, "host clock waits while the host is stopped");
        rig.head.playing = true;
        rig.run (1);
        check (rig.step() == 4, "host clock starts at the host's position");
        rig.head.ppq = 3.5;   // jump to the 15th 16th
        rig.run (1);
        check (rig.step() == 14, "host clock follows a locate");
    }

    {
        // a pattern chosen while playing waits for the end of the playing one
        SeqRig rig;
        rig.setStep (0, 45);
        auto& patterns = rig.processor.getPatterns();
        patterns.pattern (1).clear();
        setParam (rig.processor, id::run, 1.0f);
        rig.run (60 * 2);
        patterns.select (1, true);
        rig.run (60 * 2);
        check (patterns.playingIndex() == 0 && patterns.isQueued(), "a new pattern waits while one plays");
        rig.run (60 * 13);
        check (patterns.playingIndex() == 1 && ! patterns.isQueued(), "the queued pattern starts after the last step");
    }

    {
        // REC while stopped: notes go into one step after the other
        SeqRig rig;
        rig.processor.setRecording (true);
        rig.processor.setRecordStep (2);
        juce::MidiBuffer notes;
        notes.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90), 0);
        notes.addEvent (juce::MidiMessage::noteOff (1, 50), 10);
        notes.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 127), 20);
        rig.run (2, notes);
        const auto a = rig.pattern().get (2), b = rig.pattern().get (3);
        check (a.on && a.note == 50 && ! a.accent && b.on && b.note == 52 && b.accent, "step recording writes notes in order");
        check (rig.processor.getRecordStep() == 4, "step recording moves on");

        // REC while playing: into the step at hand
        setParam (rig.processor, id::run, 1.0f);
        rig.run (60 * 5 + 10);   // just into step 6
        juce::MidiBuffer live;
        live.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 0);
        rig.run (1, live);
        check (rig.pattern().get (5).on && rig.pattern().get (5).note == 60, "live recording lands on the nearest step");
    }

    {
        // patterns are saved with the project
        SB64AudioProcessor processor;
        auto& p = processor.getPatterns().pattern (2 * 16 + 4);   // C5
        seq::Step s;
        s.on = true; s.note = 40; s.gate = 80; s.accent = true; s.slide = true;
        p.set (33, s);
        p.setLast (47);
        processor.getPatterns().select (2 * 16 + 4, false);

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        SB64AudioProcessor other;
        other.setStateInformation (saved.getData(), (int) saved.getSize());
        auto& q = other.getPatterns().pattern (2 * 16 + 4);
        check (q.get (33) == s && q.last() == 47 && other.getPatterns().selectedIndex() == 2 * 16 + 4, "patterns save and load");
        check (SB64AudioProcessor().getPatterns().pattern (0).isEmpty(), "a new instance starts with empty patterns, as the hardware");
        check (sb64::Patterns::name (2 * 16 + 4) == "C5" && sb64::Patterns::name (127) == "H16", "pattern names");
    }
}

static void testPanelSequencing()
{
    SB64AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
    auto& patterns = processor.getPatterns();
    patterns.selected().clear();
    auto& p = patterns.selected();

    panel.pressKey (3);
    check (p.get (3).on && panel.getSelectedStep() == 3, "a click switches an empty step on and selects it");
    panel.turnNote (2);
    check (p.get (3).note == 50 && panel.getDisplayText() == "D3", "NOTE sets the selected step's note");
    panel.turnSelect (2);
    check (p.get (3).gate == 20, "GATE sets its gate (a new step starts at 10 %)");
    panel.octave (1);
    check (p.get (3).note == 62, "+1 OCT moves the step up an octave");
    panel.pressKey (3);
    check (! p.get (3).on, "a second click switches it off");
    panel.back();
    check (panel.getSelectedStep() < 0, "BACK leaves the step");

    // SHIFT + ACC: the keys set accents
    panel.setShift (true);
    panel.pressKey (0);
    panel.pressKey (5);
    check (p.get (5).accent && ! p.get (5).on, "SHIFT + ACC turns the keys into accent switches");
    panel.back();
    panel.setShift (true);
    panel.pressKey (1);
    panel.pressKey (6);
    check (p.get (6).slide, "SHIFT + SLIDE sets slides");
    panel.back();

    // bars
    panel.showBar (2);
    panel.pressKey (0);
    check (p.get (32).on, "BAR shows the third bar");

    // LAST STEP
    panel.setShift (true);
    panel.pressKey (4);
    panel.pressKey (15);
    check (p.last() == 47, "SHIFT + LAST STEP, then a key sets the last step");
    panel.showBar (0);

    // COPY, PATTERN, PASTE
    panel.setShift (true);
    panel.pressKey (12);
    panel.setShift (true);
    panel.pressKey (15);
    panel.pressKey (2);
    check (patterns.selectedIndex() == 2, "SHIFT + PATTERN, then a key picks the pattern");
    panel.setShift (true);
    panel.pressKey (13);
    check (patterns.pattern (2).get (32).on && patterns.pattern (2).last() == 47, "COPY and PASTE copy a pattern");

    // BANK
    panel.setShift (true);
    panel.pressKey (14);
    panel.pressKey (3);
    check (patterns.selectedIndex() == 3 * 16 + 2, "SHIFT + BANK, then a key picks the bank");

    // CLEAR needs a second press
    patterns.select (2, false);
    panel.setShift (true);
    panel.pressKey (11);
    panel.pressKey (5);
    check (! patterns.pattern (2).isEmpty(), "CLEAR is cancelled by another key");
    panel.setShift (true);
    panel.pressKey (11);
    panel.pressKey (11);
    check (patterns.pattern (2).isEmpty(), "CLEAR, CLEAR empties the pattern");

    // PLAY / STOP
    panel.play();
    check (processor.getState().getRawParameterValue (id::run)->load() > 0.5f, "PLAY starts the sequencer");
    panel.stop();
    check (processor.getState().getRawParameterValue (id::run)->load() < 0.5f, "STOP stops it");

    // REC turns the keys into a keyboard
    panel.setRecording (true);
    panel.pressKey (0);
    check (processor.getKeyboardState().isNoteOn (1, 36), "in REC the keys play notes");
    panel.releaseKey (0);
    panel.setRecording (false);
}

//==============================================================================
// A little acid riff on the internal clock for listening: 16th notes at 125 BPM. Options:
//   seconds=<n>            length (default 8 s)
//   <paramId>=<value>      plain parameter value (e.g. cutoff=0.3 wave=0)
//   note=<n>               hold one note instead of the riff
//   seq                    play the selected pattern (the demo in A1) on the sequencer instead
static void testStepTracks()
{
    // the new step fields survive packing, the save format and old saves
    seq::Step s;
    s.on = true; s.note = 40; s.gate = 70; s.mod = -64; s.pitchDepth = 9; s.pitchDecay = 2; s.cent = -50; s.chance = 12;
    check (seq::Step::unpack (s.pack()) == s, "step tracks pack and unpack");
    s.mod = 63; s.cent = 50; s.chance = 3;
    check (seq::Step::unpack (s.pack()) == s, "positive mod and cent pack too");

    seq::Pattern saved, loaded;
    saved.set (5, s);
    sb64::Patterns::stepsFromString (loaded, sb64::Patterns::stepsToString (saved));
    check (loaded.get (5) == s, "step tracks are saved with the pattern");

    // a pattern saved before the step tracks: five digits per step
    juce::String old;
    for (int i = 0; i < seq::maxSteps; ++i)
        old << (i == 2 ? "0d9a4" : "01930");   // step 3: on, accent, note 36, gate 51
    sb64::Patterns::stepsFromString (loaded, old);
    const auto legacy = loaded.get (2);
    check (legacy.on && legacy.accent && legacy.note == 36 && legacy.mod == 0 && legacy.chance == 0
           && legacy.pitchDecay == seq::Step {}.pitchDecay, "old patterns still load");

    // play modes
    auto order = [] (seq::PlayMode mode)
    {
        juce::String text;
        for (int k = 0; k < seq::passLength (mode, 4); ++k)
            text << seq::stepInPass (mode, 2, 5, k);
        return text;
    };
    check (order (seq::PlayMode::forward) == "2345" && order (seq::PlayMode::backward) == "5432", "FWD and BWD");
    check (order (seq::PlayMode::pingPong) == "234543", "PP turns without repeating the ends");
    check (order (seq::PlayMode::pingPong2) == "23455432", "PP2 repeats the ends");

    // chances
    int plays = 0;
    for (int pass = 0; pass < 16; ++pass)
        plays += seq::chancePlays (seq::firstRatio, pass, 0) ? 1 : 0;
    check (plays == 2 && seq::chancePlays (seq::firstRatio, 0, 0) && ! seq::chancePlays (seq::firstRatio, 1, 0), "1/8 plays the first of eight passes");
    check (seq::chancePlays (3, 0, 49) && ! seq::chancePlays (3, 0, 50), "50 % compares with the random number");

    // scales
    check (seq::quantise (61, 1, 0) == 60 && seq::quantise (64, 1, 0) == 64, "major scale moves C# down to C");
    check (seq::quantise (56, 2, 9) == 55 && seq::quantise (57, 2, 9) == 57, "A minor moves G# down to G");

    // per-step pitch envelope on the voice
    {
        dsp::Voice voice;
        voice.setSettings (openSettings (dsp::Wave::saw));
        voice.prepare (testRate);
        voice.setStepPitchEnvelope (9, 0.5f);
        voice.noteOn (45.0f, false, dsp::Transition::retrigger);
        std::vector<float> out ((size_t) (0.5 * testRate));
        voice.render (out.data(), (int) out.size());
        check (frequencyOf (out, 0, (size_t) (0.004 * testRate)) > 400.0, "a step's own pitch envelope starts high");
        check (std::abs (frequencyOf (out, (size_t) (0.35 * testRate), out.size()) - 110.0) < 1.0, "and falls back to the note");
    }

    // random generator
    {
        seq::Pattern p;
        p.setLast (31);
        p.setFirst (16);
        seq::RandomSettings r;
        r.density = 90;
        r.scale = 2;
        r.root = 9;
        r.octaves = 2;
        seq::randomise (p, r, 1234);
        int on = 0;
        bool inScale = true, outsideUntouched = true;
        for (int i = 0; i < seq::maxSteps; ++i)
        {
            const auto step = p.get (i);
            if (i < 16 || i > 31)
                outsideUntouched = outsideUntouched && step == seq::Step {};
            else if (step.on)
            {
                ++on;
                inScale = inScale && seq::inScale (step.note, 2, 9);
            }
        }
        check (on >= 10 && inScale, "RND PATT writes notes of the scale");
        check (outsideUntouched, "RND PATT stays between the first and last step");
    }

    // arpeggiator notes
    {
        seq::ArpSettings a;
        a.octaves = 2;
        check (seq::Arp::sequence ({ 55, 48, 52 }, a) == std::vector<int> { 48, 52, 55, 60, 64, 67 }, "arp plays up over two octaves");
        a.octaves = 1;
        a.direction = seq::ArpDirection::upDown;
        check (seq::Arp::sequence ({ 48, 52, 55, 59 }, a) == std::vector<int> { 48, 52, 55, 59, 55, 52 }, "arp up & down");
        a.direction = seq::ArpDirection::down;
        a.source = seq::ArpSettings::Source::scale;
        a.scale = 3;
        check (seq::Arp::sequence ({ 50 }, a) == std::vector<int> { 59, 57, 54, 52, 50 }, "arp source SCL plays the scale from the key");
    }
}

static void testPhase3Sequencing()
{
    {
        // BWD on the sequencer
        SeqRig rig;
        for (int i = 0; i < 4; ++i)
            rig.setStep (i, 45);
        rig.pattern().setLast (3);
        rig.pattern().setPlayMode (1);
        setParam (rig.processor, id::run, 1.0f);
        juce::String steps;
        for (int i = 0; i < 6; ++i)
        {
            rig.run (i == 0 ? 1 : 60);
            steps << rig.step();
        }
        check (steps == "321032", "BWD plays the steps backwards");
    }

    {
        // a step at 1/8 plays on the first of eight passes (one-step pattern: one pass per step)
        SeqRig rig;
        rig.setStep (0, 45, 50);
        auto step = rig.pattern().get (0);
        step.chance = seq::firstRatio;
        rig.pattern().set (0, step);
        rig.pattern().setLast (0);
        setParam (rig.processor, id::run, 1.0f);
        const auto gates = rig.run (60 * 9);
        bool rest = true;
        for (int pass = 1; pass < 8; ++pass)
            rest = rest && ! gates[(size_t) (pass * 60 + 10)];
        check (gates[10] && rest && gates[8 * 60 + 10], "ratio 1/8 skips seven passes");
    }

    {
        // chain: A1, A2, A1, ...
        SeqRig rig;
        auto& patterns = rig.processor.getPatterns();
        rig.setStep (0, 45);
        patterns.pattern (1).clear();
        patterns.pattern (1).set (0, rig.pattern().get (0));
        patterns.select (0, false);
        patterns.addToChain (1);
        setParam (rig.processor, id::run, 1.0f);
        rig.run (60 * 20);
        const auto second = patterns.playingIndex();
        rig.run (60 * 16);
        const auto third = patterns.playingIndex();
        check (patterns.chainLength() == 2 && second == 1 && third == 0, "a chain plays its patterns in turn");
    }

    {
        // the arpeggiator plays held notes in 16ths
        SeqRig rig;
        setParam (rig.processor, id::arp, 1.0f);
        setParam (rig.processor, id::arpGate, 50.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90), 0);
        const auto gates = rig.run (240, midi);
        check (gates[10] && ! gates[40] && gates[70] && ! gates[100], "ARP plays the held notes in 16ths");
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
        off.addEvent (juce::MidiMessage::noteOff (1, 55), 0);
        const auto after = rig.run (120, off);
        check (! after[30] && ! after[90], "ARP stops when the keys are let go");
    }

    {
        // the panel: SHIFT functions for the step tracks
        SB64AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
        auto& p = processor.getPatterns().selected();
        p.clear();

        panel.setShift (true);
        panel.pressKey (2);   // MOD
        panel.pressKey (4);
        panel.turnNote (5);
        check (p.get (4).mod == 5 && panel.getDisplayText() == "+5", "SHIFT + MOD: a key picks the step, NOTE sets its offset");
        panel.back();
        panel.back();

        panel.setShift (true);
        panel.pressKey (3);   // PITCH ENV
        panel.pressKey (6);
        panel.turnNote (3);
        panel.turnSelect (2);
        check (p.get (6).pitchDepth == 3 && p.get (6).pitchDecay == 6, "SHIFT + PITCH ENV: NOTE sets the depth, SELECT the decay");
        panel.back();
        panel.back();

        panel.pressKey (7);
        panel.clickSelectEncoder();
        panel.turnSelect (3);
        check (p.get (7).chance == 3 && panel.getDisplayText() == "50", "a click on SELECT switches to the step's chance");
        panel.clickSelectEncoder();
        panel.turnSelect (-2, true);
        check (p.get (7).cent == -2, "and then to its fine tune");
        panel.back();

        panel.setShift (true);
        panel.pressKey (5);   // PLAY MODE
        panel.pressKey (3);
        check (p.playMode() == 3 && panel.getDisplayText() == "PP", "SHIFT + PLAY MODE, then a key: the pattern's play mode");

        panel.setShift (true);
        panel.pressKey (4);   // LAST STEP
        panel.setShift (true);
        panel.pressKey (4);   // again: FIRST STEP
        panel.pressKey (2);
        check (p.first() == 2, "SHIFT + LAST STEP twice, then a key: the first step");

        panel.setShift (true);
        panel.pressKey (8);   // ARPEGG
        check (processor.getState().getRawParameterValue (id::arp)->load() > 0.5f, "SHIFT + ARPEGG switches the arpeggiator on");

        p.clear();
        panel.setShift (true);
        panel.pressKey (10);   // RND PATT
        check (! p.isEmpty(), "SHIFT + RND PATT writes a random pattern");

        panel.setShift (true);
        panel.pressKey (15);   // PATTERN
        panel.pressKey (2);
        panel.pressKey (3);
        check (processor.getPatterns().selectedIndex() == 2 && processor.getPatterns().chainLength() == 2,
               "PATTERN, then more keys chain patterns");
    }
}


static void testUserScalesAndKeyboard()
{
    // user scales: notes and detune
    {
        seq::UserScales users;
        users.setMask (0, seq::scaleMasks[1]);   // major
        users.setCents (0, 4, -14);
        const auto u1 = seq::numFactoryScales;
        check (seq::quantise (61, u1, 0, &users) == 60 && seq::inScale (64, u1, 0, &users), "a user scale moves notes onto its own notes");
        check (std::abs (seq::tuning (64, u1, 0, &users) + 0.14f) < 1.0e-5f && seq::tuning (60, u1, 0, &users) == 0.0f,
               "a user scale detunes single notes (microtonal)");
        users.setMask (0, 0);
        check (users.mask (0) == 1u, "the root stays in a user scale");
    }

    {
        // saved with the project
        SB64AudioProcessor processor;
        processor.getUserScales().setMask (3, seq::scaleMasks[2]);
        processor.getUserScales().setCents (3, 7, 22);
        juce::MemoryBlock block;
        processor.getStateInformation (block);
        SB64AudioProcessor other;
        other.setStateInformation (block.getData(), (int) block.getSize());
        check (other.getUserScales().mask (3) == seq::scaleMasks[2] && other.getUserScales().cents (3, 7) == 22
               && other.getUserScales().isDefault (0), "user scales are saved with the project");
    }

    {
        // REC with the arpeggiator: its notes go into the pattern, one step after the other
        SeqRig rig;
        setParam (rig.processor, id::arp, 1.0f);
        setParam (rig.processor, id::arpGate, 40.0f);
        rig.processor.setRecording (true);
        rig.processor.setRecordStep (0);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90), 0);
        rig.run (60 * 4 - 30, midi);
        const auto& p = rig.pattern();
        check (p.get (0).note == 48 && p.get (1).note == 55 && p.get (2).note == 48 && p.get (3).note == 55
               && p.get (0).gate == 40 && ! p.get (4).on, "REC records the arpeggiator's notes");
    }

    {
        SB64AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
        auto& p = processor.getPatterns().selected();
        p.clear();

        // keyboard mode: the keys play notes without recording
        panel.setKeyboardMode (true);
        panel.pressKey (0);
        check (processor.getKeyboardState().isNoteOn (1, 36) && p.isEmpty(), "keyboard mode plays notes");
        panel.releaseKey (0);
        panel.setKeyboardMode (false);

        // SHIFT + SCALE on a user scale: the keys edit it
        setParam (processor, id::scale, (float) seq::numFactoryScales);   // U1
        panel.setShift (true);
        panel.pressKey (7);   // SCALE
        panel.pressKey (1);
        panel.pressKey (1);
        check ((processor.getUserScales().mask (0) & 2u) == 0, "a second press takes a note out of the user scale");
        panel.pressKey (4);
        panel.turnNote (-3);
        check (processor.getUserScales().cents (0, 4) == -15 && panel.getDisplayText() == "-15", "NOTE detunes the picked note");
        panel.pressKey (1);
        check ((processor.getUserScales().mask (0) & 2u) != 0, "a press puts it back");
    }
}


static void testVersion02()
{
    {
        // play mode and first step are saved with the pattern
        SB64AudioProcessor processor;
        auto& p = processor.getPatterns().pattern (5);
        p.setLast (11);
        p.setFirst (4);
        p.setPlayMode (4);
        juce::MemoryBlock block;
        processor.getStateInformation (block);
        SB64AudioProcessor other;
        other.setStateInformation (block.getData(), (int) block.getSize());
        const auto& q = other.getPatterns().pattern (5);
        check (q.playMode() == 4 && q.first() == 4 && q.last() == 11, "play mode and first step are saved per pattern");
    }

    {
        // MOD live: turning NOTE in MOD mode while the sequence runs writes into the playing step
        SeqRig rig;
        for (int i = 0; i < 16; ++i)
            rig.setStep (i, 45);
        std::unique_ptr<juce::AudioProcessorEditor> editor (rig.processor.createEditor());
        auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
        setParam (rig.processor, id::run, 1.0f);
        rig.run (60 * 2 + 10);
        panel.setShift (true);
        panel.pressKey (2);   // MOD
        panel.turnNote (7);
        check (rig.pattern().get (2).mod == 7 && rig.pattern().get (1).mod == 0, "MOD live writes into the playing step");
    }

    {
        // the arpeggiator on its own tempo
        SeqRig rig;
        setParam (rig.processor, id::arp, 1.0f);
        setParam (rig.processor, id::arpTempo, 60.0f);   // a 16th = 12000 samples = 120 blocks
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);
        const auto gates = rig.run (240, midi);
        check (gates[10] && gates[50] && ! gates[70] && gates[130], "ABP gives the arpeggiator its own tempo");
        check (sb64::menu::valueText (rig.processor.getState(), id::arpTempo) == "60", "ABP shows the tempo");
        setParam (rig.processor, id::arpTempo, 0.0f);
        check (sb64::menu::valueText (rig.processor.getState(), id::arpTempo) == "SEQ", "ABP 0 follows the sequencer");
    }

    {
        // factory sounds
        SB64AudioProcessor processor;
        check (processor.getNumPrograms() == 16 && processor.getProgramName (1) == "Classic Acid", "16 factory sounds as programs");
        setParam (processor, id::detune, 0.9f);
        setParam (processor, id::tempo, 133.0f);
        processor.setCurrentProgram (1);
        auto value = [&processor] (const char* paramId) { return processor.getState().getRawParameterValue (paramId)->load(); };
        check (std::abs (value (id::resonance) - 0.75f) < 1.0e-4f && value (id::detune) == 0.0f && value (id::tempo) == 133.0f,
               "a factory sound sets the sound, resets the rest of it and leaves the sequencer alone");

        bool loud = true, finite = true;
        for (int i = 0; i < processor.getNumPrograms(); ++i)
        {
            SeqRig rig;
            rig.processor.setCurrentProgram (i);
            setParam (rig.processor, id::run, 1.0f);
            sb64::Patterns::writeDemo (rig.processor.getPatterns().pattern (1));
            rig.processor.getPatterns().select (1, false);
            float peakLevel = 0.0f;
            for (int b = 0; b < 480; ++b)
            {
                rig.run (1);
                peakLevel = std::max (peakLevel, rig.buffer.getMagnitude (0, 0, 100));
                finite = finite && std::isfinite (rig.buffer.getSample (0, 50));
            }
            loud = loud && peakLevel > 0.02f && peakLevel < 1.5f;
            if (peakLevel <= 0.02f || peakLevel >= 1.5f)
                std::printf ("      %s peak %.3f\n", processor.getProgramName (i).toRawUTF8(), peakLevel);
        }
        check (loud && finite, "every factory sound plays the patterns at a sensible level");
    }
}


static void testTransposeReloadDrift()
{
    {
        // SHIFT + TRANSP: the keys transpose the sequence; with latch off only while held
        SB64AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
        panel.setShift (true);
        panel.pressKey (6);   // TRANSP
        panel.pressKey (5);
        panel.releaseKey (5);
        check (processor.getLiveTranspose() == 5 && panel.getDisplayText() == "T+5", "TRANSP mode: a key transposes (latched)");
        panel.octave (-1);
        panel.pressKey (3);
        check (processor.getLiveTranspose() == -9, "-1 OCT moves the transpose keys down an octave");
        panel.releaseKey (3);
        setParam (processor, id::transposeLatch, 0.0f);
        panel.pressKey (7);
        const auto held = processor.getLiveTranspose();
        panel.releaseKey (7);
        check (held == -5 && processor.getLiveTranspose() == 0, "without latch the transpose lasts while the key is held");
        panel.pressKey (15);
        check (processor.getLiveTranspose() == 0, "key 16 goes back to no transpose");
    }

    {
        // RELOAD: SHIFT + PATTERN and the pattern's own key undo the changes since it was picked
        SB64AudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto& panel = dynamic_cast<SB64AudioProcessorEditor&> (*editor).getPanel();
        auto& patterns = processor.getPatterns();
        patterns.select (3, false);
        const auto before = patterns.selected().get (0);
        processor.randomisePattern();
        panel.setShift (true);
        panel.pressKey (15);   // PATTERN
        panel.pressKey (3);    // A4, already selected
        check (patterns.selected().get (0) == before && patterns.selectedIndex() == 3 && panel.getDisplayText() == "RLD",
               "reloading the selected pattern undoes RND PATT");
    }

    {
        // random generator extras
        seq::Pattern p;
        p.setLast (63);
        seq::RandomSettings r;
        r.density = 90; r.pitchEnv = 100; r.chance = 100; r.gateLow = 40; r.gateHigh = 60;
        seq::randomise (p, r, 99);
        bool all = true;
        for (int i = 0; i < seq::maxSteps; ++i)
        {
            const auto step = p.get (i);
            if (step.on)
                all = all && step.pitchDepth > 0 && step.chance > 0 && step.gate >= 40 && step.gate <= 60;
        }
        check (all, "RND PATT: pitch envelopes, chances and the gate range");
    }

    {
        // drift: off = exact pitch; full = only a few cents away
        auto s = openSettings (dsp::Wave::saw);
        const auto exact = frequencyOf (renderNote (s, 1.0), (size_t) (0.2 * testRate), (size_t) testRate);
        s.drift = 1.0f;
        const auto drifted = frequencyOf (renderNote (s, 1.0), (size_t) (0.2 * testRate), (size_t) testRate);
        const auto cents = 1200.0 * std::log2 (drifted / 110.0);
        check (std::abs (exact - 110.0) < 0.2, "no drift: exact pitch");
        check (std::abs (cents) > 0.05 && std::abs (cents) < 12.0, "full drift: a few cents off");
    }
}


static void testHardwareFeatures()
{
    // a new step: C3 at 10 %
    check (seq::Step {}.note == 48 && seq::Step {}.gate == 10, "a new step is C3 with 10 % gate, as on the hardware");

    // cutoff: the bottom fifth of the knob barely moves
    check (dsp::cutoffHz (0.2f) < 46.0f && dsp::cutoffHz (0.0f) > 29.0f && dsp::cutoffHz (1.0f) > 17000.0f,
           "CUTOFF does little below 9 o'clock");

    {
        // the sub gets weaker on the lowest notes
        auto subShare = [] (float note)
        {
            auto s = openSettings (dsp::Wave::triangle);
            const auto plain = renderNote (s, 0.5, note);
            s.subLevel = 1.0f;
            const auto withSub = renderNote (s, 0.5, note);
            return rms (withSub, (size_t) (0.2 * testRate), withSub.size()) / rms (plain, (size_t) (0.2 * testRate), plain.size());
        };
        check (subShare (14.0f) < 0.85 * subShare (48.0f), "the sub is weaker on the lowest notes");
    }

    {
        // program change picks a pattern
        SeqRig rig;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 32, 2), 0);
        midi.addEvent (juce::MidiMessage::programChange (1, 5), 1);
        rig.run (1, midi);
        check (rig.processor.getPatterns().selectedIndex() == 2 * 16 + 5, "bank select + program change pick a pattern (C6)");
        juce::MidiBuffer high;
        high.addEvent (juce::MidiMessage::programChange (1, 20), 0);
        rig.run (1, high);
        check (rig.processor.getPatterns().selectedIndex() == 2 * 16 + 5, "program changes above 15 are ignored, as on the hardware");
    }

    {
        // MIDI out: the sequencer's notes, accent as velocity 127, a slide overlaps the next note
        SeqRig rig;
        rig.setStep (0, 45, 50, true, true);
        rig.setStep (1, 50, 50);
        setParam (rig.processor, id::run, 1.0f);
        std::vector<juce::MidiMessage> out;
        for (int b = 0; b < 120; ++b)
        {
            juce::MidiBuffer midi;
            rig.buffer.clear();
            rig.processor.processBlock (rig.buffer, midi);
            for (const auto m : midi)
                out.push_back (m.getMessage());
        }
        const auto ok = out.size() == 4 && out[0].isNoteOn() && out[0].getNoteNumber() == 45 && out[0].getVelocity() == 127
                     && out[1].isNoteOn() && out[1].getNoteNumber() == 50 && out[2].isNoteOff() && out[2].getNoteNumber() == 45
                     && out[3].isNoteOff() && out[3].getNoteNumber() == 50;
        check (ok, "MIDI out sends the sequencer's notes, slides overlapping");
    }

    {
        // arp source PAT: the pattern's notes in step order, moved by the key (relative to C3)
        seq::ArpSettings a;
        a.source = seq::ArpSettings::Source::pattern;
        const int notes[] { 48, 55, 51, 60 };
        a.patternNotes = notes;
        a.numPatternNotes = 4;
        check (seq::Arp::sequence ({ 50 }, a) == std::vector<int> { 50, 57, 53, 62 }, "arp source PAT plays the pattern's notes");
    }

    {
        // the mod wheel opens the filter
        auto brightness = [] (float wheel)
        {
            auto s = openSettings (dsp::Wave::saw);
            s.cutoff = 0.3f;
            dsp::Voice voice;
            voice.setSettings (s);
            voice.prepare (testRate);
            voice.setModWheel (wheel);
            voice.noteOn (45.0f, false, dsp::Transition::retrigger);
            std::vector<float> out ((size_t) (0.3 * testRate));
            voice.render (out.data(), (int) out.size());
            double d = 0.0, e = 0.0;
            for (auto i = out.size() / 2; i < out.size(); ++i)
            {
                d += std::pow (out[i] - out[i - 1], 2.0);
                e += std::pow (out[i], 2.0);
            }
            return d / std::max (1.0e-12, e);
        };
        check (brightness (1.0f) > 4.0 * brightness (0.0f), "the mod wheel (CC 1) raises the cutoff, as on the hardware");
    }
}

static bool renderToFile (const juce::String& path, const juce::StringArray& options)
{
    constexpr int sampleRate = 48000, block = 480;
    SB64AudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, sampleRate, block);
    processor.prepareToPlay (sampleRate, block);

    double seconds = 8.0;
    int heldNote = -1;
    bool useSequencer = false;

    for (const auto& option : options)
    {
        const auto key = option.upToFirstOccurrenceOf ("=", false, false);
        const auto value = option.fromFirstOccurrenceOf ("=", false, false);

        if (key == "seconds")
            seconds = value.getDoubleValue();
        else if (key == "note")
            heldNote = value.getIntValue();
        else if (option == "seq")
        {
            useSequencer = true;
            sb64::Patterns::writeDemo (processor.getPatterns().selected());
        }
        else if (processor.getState().getParameter (key) != nullptr)
            setParam (processor, key, value.getFloatValue());
        else
            std::printf ("unknown option \"%s\"\n", option.toRawUTF8());
    }

    // note (0 = rest), accent, slide into the next step
    struct Step { int note; bool accent, slide; };
    const Step riff[] {
        { 36, true, false }, { 36, false, false }, { 48, false, true }, { 36, false, false },
        { 0, false, false }, { 39, true, false },  { 36, false, false }, { 46, false, true },
        { 48, false, false }, { 36, false, false }, { 0, false, false }, { 43, true, true },
        { 41, false, false }, { 36, true, false },  { 39, false, true }, { 36, false, false },
    };

    const auto stepSamples = (int) std::lround (sampleRate * 60.0 / 125.0 / 4.0);
    const auto totalSamples = (int) (seconds * sampleRate);
    std::vector<float> out;
    out.reserve ((size_t) totalSamples);
    juce::AudioBuffer<float> buffer (2, block);
    int lastNote = -1;

    for (int start = 0; start < totalSamples; start += block)
    {
        juce::MidiBuffer midi;

        if (useSequencer)
        {
            if (start == 0)
            {
                setParam (processor, id::clock, 0.0f);
                setParam (processor, id::tempo, 125.0f);
                setParam (processor, id::run, 1.0f);
            }
        }
        else if (heldNote >= 0)
        {
            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, heldNote, (juce::uint8) 90), 0);
        }
        else
        {
            for (int i = 0; i < block; ++i)
            {
                const auto t = start + i;
                const auto stepIndex = t / stepSamples;
                const auto& step = riff[stepIndex % 16];
                const auto& previous = riff[(stepIndex + 15) % 16];

                if (t % stepSamples == 0 && step.note > 0)
                {
                    midi.addEvent (juce::MidiMessage::noteOn (1, step.note, (juce::uint8) (step.accent ? 127 : 90)), i);

                    // a slide holds the previous note until this one has started
                    if (previous.slide && lastNote >= 0 && lastNote != step.note)
                        midi.addEvent (juce::MidiMessage::noteOff (1, lastNote), i);

                    lastNote = step.note;
                }

                // gate: half a step, or into the next step for a slide
                if (t % stepSamples == stepSamples / 2 && step.note > 0 && ! step.slide)
                {
                    midi.addEvent (juce::MidiMessage::noteOff (1, step.note), i);
                    lastNote = -1;
                }
            }
        }

        buffer.clear();
        processor.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
    }

    juce::MemoryOutputStream wav;
    const auto dataSize = (uint32_t) out.size() * 2;
    wav.write ("RIFF", 4); wav.writeInt ((int) (36 + dataSize)); wav.write ("WAVE", 4);
    wav.write ("fmt ", 4); wav.writeInt (16); wav.writeShort (1); wav.writeShort (1);
    wav.writeInt (sampleRate); wav.writeInt (sampleRate * 2); wav.writeShort (2); wav.writeShort (16);
    wav.write ("data", 4); wav.writeInt ((int) dataSize);

    float maxLevel = 0.0f;
    for (auto v : out)
    {
        maxLevel = std::max (maxLevel, std::abs (v));
        wav.writeShort ((short) juce::jlimit (-32767, 32767, (int) std::lround (v * 32767.0f)));
    }

    std::printf ("peak %.1f dBFS\n", 20.0 * std::log10 (std::max (1.0e-6f, maxLevel)));
    return juce::File (path).replaceWithData (wav.getData(), wav.getDataSize());
}

static int writePng (const juce::Image& image, const char* path)
{
    juce::File file (juce::String::fromUTF8 (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);
    const auto ok = stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    std::printf ("%s %s\n", ok ? "wrote" : "could not write", path);
    return ok ? 0 : 1;
}


// Harmonic levels of a held C3 through the open filter, in dB relative to the fundamental.
static int harmonics (int wave)
{
    auto s = openSettings ((dsp::Wave) juce::jlimit (0, 2, wave));
    s.cutoff = 1.0f;
    const auto out = renderNote (s, 1.0, 48.0f);
    const auto f0 = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
    const auto from = (size_t) (0.5 * testRate), length = (size_t) (0.4 * testRate);

    auto level = [&] (int h)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < length; ++i)
        {
            const auto w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) length);
            const auto phase = 2.0 * juce::MathConstants<double>::pi * f0 * h * (double) i / testRate;
            re += w * out[from + i] * std::cos (phase);
            im += w * out[from + i] * std::sin (phase);
        }
        return std::sqrt (re * re + im * im);
    };

    const auto fundamental = level (1);
    for (int h = 2; h <= 8; ++h)
        std::printf ("H%d %6.1f dB\n", h, 20.0 * std::log10 (std::max (1.0e-9, level (h) / fundamental)));
    return 0;
}

// Renders the editor to a PNG file at design size (scale 1) or larger.
static int snapshot (const char* path, float scale)
{
    SB64AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (juce::roundToInt (FrontPanel::designWidth * scale), juce::roundToInt (FrontPanel::designHeight * scale));
    return writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), path);
}

static int renderIcon (const char* path, int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    {
        juce::Graphics g (image);
        sb64::ui::drawAppIcon (g, image.getBounds().toFloat());
    }
    return writePng (image, path);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc >= 3 && std::string_view (argv[1]) == "--render")
    {
        juce::StringArray options;
        for (int i = 3; i < argc; ++i)
            options.add (juce::String::fromUTF8 (argv[i]));
        const auto ok = renderToFile (juce::String::fromUTF8 (argv[2]), options);
        std::printf ("%s %s\n", ok ? "wrote" : "could not write", argv[2]);
        return ok ? 0 : 1;
    }

    if (argc >= 3 && std::string_view (argv[1]) == "--snapshot")
        return snapshot (argv[2], argc >= 4 ? (float) std::atof (argv[3]) : 1.0f);

    if (argc == 3 && std::string_view (argv[1]) == "--harmonics")
        return harmonics (std::atoi (argv[2]));

    if (argc == 3 && std::string_view (argv[1]) == "--icon")
        return renderIcon (argv[2], 1024);

    if (argc == 2 && std::string_view (argv[1]) == "--bench")
    {
        SB64AudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, id::detune, 1.0f);
        setParam (processor, id::vcaEnv, 1.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        const auto blocks = 48000 * 20 / 512;
        const auto start = juce::Time::getMillisecondCounterHiRes();

        for (int i = 0; i < blocks; ++i)
        {
            juce::MidiBuffer midi;
            if (i == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), 0);
            processor.processBlock (buffer, midi);
        }

        const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
        std::printf ("20 s audio in %.0f ms (%.2f %% of one core), latency %d samples\n",
                     ms, ms / 200.0, processor.getLatencySamples());
        return 0;
    }

    testOscillator();
    testEnvelopes();
    testFilter();
    testSlideAndPitchEnv();
    testLfo();
    testDetuneAndDrive();
    testProcessor();
    testMenu();
    testEditor();
    testSequencer();
    testPanelSequencing();
    testStepTracks();
    testPhase3Sequencing();
    testUserScalesAndKeyboard();
    testVersion02();
    testTransposeReloadDrift();
    testHardwareFeatures();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
