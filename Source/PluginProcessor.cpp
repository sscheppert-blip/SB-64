#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    namespace id = sb64::params::id;
    constexpr const char* editorWidthProperty = "editorWidth";

    // the step after the given one within the pattern's first..last
    int nextInPattern (const sb64::seq::Pattern& p, int step)
    {
        return step < p.first() || step >= p.last() ? p.first() : step + 1;
    }
}

SB64AudioProcessor::SB64AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SB64", sb64::params::createLayout (wrapperType == wrapperType_Standalone))
{
    auto raw = [this] (const char* paramId)
    {
        auto* value = state.getRawParameterValue (paramId);
        jassert (value != nullptr);
        return value;
    };

    refs.drive = raw (id::drive);           refs.detune = raw (id::detune);
    refs.vcaEnv = raw (id::vcaEnv);         refs.vcfEnv = raw (id::vcfEnv);
    refs.resonance = raw (id::resonance);   refs.cutoff = raw (id::cutoff);
    refs.fmLevel = raw (id::fmLevel);       refs.subLevel = raw (id::subLevel);
    refs.noiseLevel = raw (id::noiseLevel); refs.lfoRate = raw (id::lfoRate);
    refs.lfoLevel = raw (id::lfoLevel);     refs.envAmount = raw (id::envAmount);
    refs.wave = raw (id::wave);             refs.filterMode = raw (id::filterMode);
    refs.lfoShape = raw (id::lfoShape);     refs.lfoSync = raw (id::lfoSync);
    refs.lfoRange = raw (id::lfoRange);     refs.lfoReset = raw (id::lfoReset);
    refs.lfoPhase = raw (id::lfoPhase);     refs.slideTime = raw (id::slideTime);
    refs.accentVel = raw (id::accentVel);   refs.pitchEnvDepth = raw (id::pitchEnvDepth);
    refs.pitchEnvDecay = raw (id::pitchEnvDecay);
    refs.bendRange = raw (id::bendRange);   refs.tune = raw (id::tune);
    refs.volume = raw (id::volume);
    refs.run = raw (id::run);               refs.tempo = raw (id::tempo);
    refs.clock = raw (id::clock);           refs.swing = raw (id::swing);

    heldNotes.reserve (128);
}

void SB64AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The voice runs oversampled: 4x up to 48 kHz, 2x up to 96 kHz.
    const auto factorExponent = sampleRate <= 50000.0 ? 2 : (sampleRate <= 100000.0 ? 1 : 0);
    oversamplingFactor = 1 << factorExponent;
    maxChunk = juce::jmax (samplesPerBlock, 32);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, (size_t) factorExponent, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxChunk);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    zeroBuffer.setSize (1, maxChunk);
    zeroBuffer.clear();
    oversampledBuffer.setSize (1, maxChunk * oversamplingFactor);

    voice.setSettings (readSettings ((double) display.bpm.load()));
    voice.prepare (sampleRate * oversamplingFactor);
    sequencer.prepare (sampleRate);
    wasRunning = false;
    heldNotes.clear();
    hostWasPlaying = false;
}

bool SB64AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
sb64::dsp::Settings SB64AudioProcessor::readSettings (double bpm) const noexcept
{
    using namespace sb64;
    auto on = [] (const std::atomic<float>* p) { return p->load() > 0.5f; };
    auto index = [] (const std::atomic<float>* p, int max) { return juce::jlimit (0, max, juce::roundToInt (p->load())); };

    dsp::Settings s;
    s.drive = refs.drive->load();           s.detune = refs.detune->load();
    s.vcaEnv = refs.vcaEnv->load();         s.vcfEnv = refs.vcfEnv->load();
    s.resonance = refs.resonance->load();   s.cutoff = refs.cutoff->load();
    s.fmLevel = refs.fmLevel->load();       s.subLevel = refs.subLevel->load();
    s.noiseLevel = refs.noiseLevel->load(); s.lfoLevel = refs.lfoLevel->load();
    s.envAmount = refs.envAmount->load();

    s.wave = (dsp::Wave) index (refs.wave, 2);
    s.filter = (dsp::FilterMode) index (refs.filterMode, 1);

    s.lfoShape = (dsp::LfoShape) index (refs.lfoShape, dsp::numLfoShapes - 1);
    s.lfoHz = on (refs.lfoSync) ? (float) (bpm / 60.0 * params::lfoCyclesPerBeat (refs.lfoRate->load(), on (refs.lfoRange)))
                                : dsp::lfoHz (refs.lfoRate->load());
    s.lfoReset = on (refs.lfoReset);
    s.lfoStartPhase = (float) index (refs.lfoPhase, 7) / 8.0f;

    s.slideSeconds = refs.slideTime->load() * 0.001f;
    s.pitchEnvDepth = index (refs.pitchEnvDepth, 9);
    s.pitchEnvDecay = refs.pitchEnvDecay->load();
    s.tuneHz = refs.tune->load();
    s.volume = refs.volume->load();
    return s;
}

void SB64AudioProcessor::playNote (int note, int velocity, bool slide) noexcept
{
    const auto accent = velocity >= juce::roundToInt (refs.accentVel->load());
    voice.noteOn ((float) note, accent, slide ? sb64::dsp::Transition::slide : sb64::dsp::Transition::retrigger);
    display.note = note;
}

void SB64AudioProcessor::record (int note, int velocity) noexcept
{
    auto& pattern = patterns.selected();
    int target;

    if (sequencer.isRunning() && sequencer.getPosition() >= 0)
    {
        // quantised to the nearest step
        target = sequencer.getPosition();
        if (sequencer.getProgress() > 0.5f)
            target = nextInPattern (pattern, target);
    }
    else
    {
        target = juce::jlimit (pattern.first(), pattern.last(), recordStep.load());
        recordStep = nextInPattern (pattern, target);
    }

    auto step = pattern.get (target);
    step.on = true;
    step.note = juce::jlimit (sb64::seq::lowestNote, sb64::seq::highestNote, note);
    step.accent = velocity >= juce::roundToInt (refs.accentVel->load());
    step.slide = false;
    pattern.set (target, step);
}

void SB64AudioProcessor::handleMidi (const juce::MidiMessage& message) noexcept
{
    if (message.isNoteOn())
    {
        const auto note = message.getNoteNumber();

        if (recording.load())
            record (note, message.getVelocity());

        const auto legato = ! heldNotes.empty();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        // playing legato slides to the new note
        playNote (note, message.getVelocity(), legato);
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        const auto wasSounding = ! heldNotes.empty() && heldNotes.back() == note;
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());

        if (heldNotes.empty())
            voice.noteOff();
        else if (wasSounding)
            playNote (heldNotes.back(), 0, true);   // back to the note still held
    }
    else if (message.isPitchWheel())
    {
        pitchWheel = message.getPitchWheelValue();
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        heldNotes.clear();

        if (message.isAllSoundOff())
            voice.panic();
        else
            voice.noteOff();
    }
}

void SB64AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    juce::Optional<double> hostBpm, hostPpq;
    bool hostPlaying = false;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            hostBpm = position->getBpm();
            hostPpq = position->getPpqPosition();
            hostPlaying = position->getIsPlaying();
        }
    }

    const auto bpm = hostBpm.hasValue() ? juce::jlimit (20.0, 999.0, *hostBpm) : 120.0;
    display.bpm = (float) bpm;

    // the step keys on the panel
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    voice.setSettings (readSettings (bpm));
    voice.setBend ((float) (pitchWheel - 8192) / 8192.0f * refs.bendRange->load());

    // a synced LFO follows the host's beat position while it plays
    if (refs.lfoSync->load() > 0.5f && hostPlaying && hostPpq.hasValue())
    {
        const auto cycles = *hostPpq * sb64::params::lfoCyclesPerBeat (refs.lfoRate->load(), refs.lfoRange->load() > 0.5f);
        voice.setLfoPhase ((float) (cycles - std::floor (cycles)));
    }

    // stopping the host lets go of everything (a drone keeps going until a new note)
    if (hostWasPlaying && ! hostPlaying)
    {
        heldNotes.clear();
        voice.noteOff();
    }

    hostWasPlaying = hostPlaying;

    if (oversampledBuffer.getNumSamples() < numSamples * oversamplingFactor)
        oversampledBuffer.setSize (1, numSamples * oversamplingFactor, false, false, true);

    // sequencer clock: its own tempo, or the host's beat position
    sb64::seq::Clock clock;
    clock.run = refs.run->load() > 0.5f;
    clock.host = refs.clock->load() > 0.5f;
    clock.hostPlaying = hostPlaying && hostPpq.hasValue();
    clock.bpm = clock.host ? bpm : (double) refs.tempo->load();
    clock.swing = refs.swing->load() * 0.01f;
    const auto ppqPerSample = bpm / 60.0 / getSampleRate();
    const auto ppqStart = hostPpq.hasValue() ? *hostPpq : 0.0;

    if (! clock.host)
        display.bpm = (float) clock.bpm;

    // one host sample at a time: MIDI, sequencer, voice
    auto* oversampled = oversampledBuffer.getWritePointer (0);
    auto event = midi.cbegin();

    for (int i = 0; i < numSamples; ++i)
    {
        for (; event != midi.cend() && juce::jlimit (0, juce::jmax (0, numSamples - 1), (*event).samplePosition) <= i; ++event)
            handleMidi ((*event).getMessage());

        clock.ppq = ppqStart + ppqPerSample * i;
        sequencer.process (voice, patterns, clock);
        voice.render (oversampled + i * oversamplingFactor, oversamplingFactor);
    }

    midi.clear();

    // a stopped sequence starts again from its first step
    if (wasRunning && ! sequencer.isRunning())
        sequencer.reset();

    wasRunning = sequencer.isRunning();
    display.running = sequencer.isRunning();
    display.step = sequencer.isRunning() ? sequencer.getPosition() : -1;

    // back to the host rate
    auto output = getBusBuffer (buffer, false, 0);
    auto* mono = output.getWritePointer (0);

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const auto length = juce::jmin (maxChunk, numSamples - start);
        const auto zeros = juce::dsp::AudioBlock<float> (zeroBuffer).getSubBlock (0, (size_t) length);
        auto up = oversampler->processSamplesUp (zeros);
        juce::FloatVectorOperations::copy (up.getChannelPointer (0), oversampled + start * oversamplingFactor,
                                           length * oversamplingFactor);

        float* channels[] = { mono + start };
        juce::dsp::AudioBlock<float> down (channels, 1, (size_t) length);
        oversampler->processSamplesDown (down);
    }

    const auto blockPeak = output.getMagnitude (0, 0, numSamples);

    if (blockPeak > display.peak.load())
        display.peak = blockPeak;

    display.gate = voice.isGateOn();
    display.accent = voice.isAccent() && voice.getAmpEnvelope() > 0.05f;
    display.lfoHigh = voice.getLfoValue() > 0.0f;

    for (int c = 1; c < output.getNumChannels(); ++c)
        output.copyFrom (c, 0, output, 0, 0, numSamples);

    for (int c = output.getNumChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

//==============================================================================
void SB64AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty (editorWidthProperty, editorWidth.load(), nullptr);
    tree.appendChild (patterns.toTree(), nullptr);

    if (const auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void SB64AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            editorWidth = (int) tree.getProperty (editorWidthProperty, 0);
            tree.removeProperty (editorWidthProperty, nullptr);

            const auto savedPatterns = tree.getChildWithName ("PATTERNS");
            tree.removeChild (savedPatterns, nullptr);
            patterns.fromTree (savedPatterns);
            state.replaceState (tree);

            // CLAP hosts expect a value rescan after a state load (the wrapper doesn't send one itself)
            if (is_clap)
                updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
        }
    }
}

juce::AudioProcessorEditor* SB64AudioProcessor::createEditor()
{
    return new SB64AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SB64AudioProcessor();
}
