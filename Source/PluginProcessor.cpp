// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tb303;

namespace
{
    juce::StringArray directionNames { "Forward", "Reverse", "Ping-Pong", "Random" };
    juce::StringArray clockNames    { "Internal", "Host" };

    constexpr const char* patternProperty = "PATTERN";
}

//==============================================================================
static juce::NormalisableRange<float> unitRange() { return { 0.0f, 1.0f }; }

juce::AudioProcessorValueTreeState::ParameterLayout Tb303AudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addUnit = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                           NormalisableRange<float> (0.0f, 1.0f), def,
                                                           AudioParameterFloatAttributes().withLabel ("")));
    };

    addUnit ("waveform",  "Waveform",   1.00f);
    addUnit ("tuning",    "Tuning",     0.50f);
    addUnit ("cutoff",    "Cut Off",    0.30f);
    addUnit ("resonance", "Resonance",  0.45f);
    addUnit ("envmod",    "Env Mod",    0.60f);
    addUnit ("decay",     "Decay",      0.32f);
    addUnit ("accent",    "Accent",     0.70f);
    addUnit ("drive",     "Drive",      0.20f);
    addUnit ("volume",    "Volume",     0.80f);
    addUnit ("tempo",     "Tempo",      0.50f);
    addUnit ("swing",     "Swing",      0.00f);
    addUnit ("transpose", "Transpose",  0.50f);

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "length", 1 }, "Length", 1, 16, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "direction", 1 }, "Direction", directionNames, 0));
    // Default to the host clock so the sequencer follows the DAW transport.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "clock", 1 }, "Clock Source", clockNames, 1));

    return layout;
}

//==============================================================================
Tb303AudioProcessor::Tb303AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TB303STATE", createParameterLayout())
{
    patternCopy = factory::classicAcid();
    patternToTree();
}

Tb303AudioProcessor::~Tb303AudioProcessor() = default;

//==============================================================================
void Tb303AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    syncParamsFromState();
    patternDirty.store (true);
}

void Tb303AudioProcessor::releaseResources() {}

bool Tb303AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Tb303AudioProcessor::syncParamsFromState()
{
    auto get = [this] (const char* id) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return 0.0f;
    };

    params.waveform  = get ("waveform");
    params.tuning    = get ("tuning");
    params.cutoff    = get ("cutoff");
    params.resonance = get ("resonance");
    params.envMod    = get ("envmod");
    params.decay     = get ("decay");
    params.accent    = get ("accent");
    params.drive     = get ("drive");
    params.volume    = get ("volume");
    params.tempo     = get ("tempo");
    params.swing     = get ("swing");
    params.transpose = get ("transpose");

    if (auto* c = apvts.getRawParameterValue ("clock"))
        clockSource.store ((int) c->load());
}

//==============================================================================
void Tb303AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChans   = buffer.getNumChannels();

    //--------------------------------------------------------------- parameters
    syncParamsFromState();

    params.hostSync = clockSource.load() == 1;

    //--------------------------------------------------------------- pattern
    if (patternDirty.exchange (false))
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        engine.setPattern (patternCopy);
    }

    engine.setParams (params);
    engine.setTranspose (transposeKnobToSemis (params.transpose));

    //--------------------------------------------------------------- transport
    bool playing = false;

    if (params.hostSync)
    {
        double bpm = 0.0;
        bool hostPlaying = false;

        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (pos->getBpm())        bpm = *pos->getBpm();
                hostPlaying = pos->getIsPlaying();
            }

        if (bpm > 1.0)
        {
            engine.setHostTempo (bpm);
            currentBpm.store (bpm);
        }

        playing = transport.update (true, hostPlaying);
    }
    else
    {
        engine.clearHostTempo();
        currentBpm.store (tempoKnobToBpm (params.tempo));

        playing = transport.update (false, false);
    }

    engine.setPlaying (playing);
    transportActive.store (playing);

    //--------------------------------------------------------------- midi
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    for (const auto meta : midi)
        handleIncomingMidi (meta.getMessage());

    //--------------------------------------------------------------- render
    buffer.clear();
    engine.processBlock (buffer.getArrayOfWritePointers(), numChans, numSamples);

    //--------------------------------------------------------------- metering
    float peak = 0.0f;
    for (int c = 0; c < numChans; ++c)
        peak = juce::jmax (peak, buffer.getMagnitude (c, 0, numSamples));

    const float previous = outputLevel.load();
    outputLevel.store (peak > previous ? peak : previous * 0.85f);
}

void Tb303AudioProcessor::handleIncomingMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity() >= 0.8f);
    }
    else if (m.isNoteOff())
    {
        // While the sequencer/transport is running it owns the voice (the
        // hardware behaviour), so incoming note-offs must not cut its notes.
        if (! transportActive.load())
            engine.noteOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff();
    }
}

void Tb303AudioProcessor::playNotesFromKeyboard (bool on, int midiNote, float velocity)
{
    if (on) engine.noteOn (midiNote, velocity >= 0.8f);
    else    engine.noteOff();
}

//==============================================================================
void Tb303AudioProcessor::setPlaying (bool shouldPlay)
{
    transport.requestPlay (shouldPlay);
}

//==============================================================================
tb303::Pattern Tb303AudioProcessor::getPattern() const
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    return patternCopy;
}

void Tb303AudioProcessor::setPattern (const tb303::Pattern& p)
{
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        patternCopy = p;
    }
    patternDirty.store (true);
    patternToTree();
}

void Tb303AudioProcessor::setStep (int index, const tb303::Step& s)
{
    if (index < 0 || index >= tb303::Pattern::maxSteps)
        return;

    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        patternCopy.steps[index] = s;
    }
    patternDirty.store (true);
    patternToTree();
}

tb303::Step Tb303AudioProcessor::getStep (int index) const
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    if (index < 0 || index >= tb303::Pattern::maxSteps)
        return {};
    return patternCopy.steps[index];
}

//==============================================================================
void Tb303AudioProcessor::patternToTree()
{
    tb303::Pattern p;
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        p = patternCopy;
    }

    auto child = apvts.state.getOrCreateChildWithName (patternProperty, nullptr);

    child.setProperty ("length", p.length, nullptr);
    child.setProperty ("rootNote", p.rootNote, nullptr);
    child.setProperty ("transpose", p.transpose, nullptr);
    child.setProperty ("direction", (int) p.direction, nullptr);

    for (int i = 0; i < tb303::Pattern::maxSteps; ++i)
    {
        auto s = child.getChild (i);
        if (! s.isValid())
            s = juce::ValueTree ("STEP");

        s.setProperty ("note",   p.steps[i].note, nullptr);
        s.setProperty ("oct",    p.steps[i].oct, nullptr);
        s.setProperty ("rest",   p.steps[i].rest, nullptr);
        s.setProperty ("accent", p.steps[i].accent, nullptr);
        s.setProperty ("slide",  p.steps[i].slide, nullptr);

        if (s.getParent() != child)
            child.appendChild (s, nullptr);
    }
}

void Tb303AudioProcessor::patternFromTree()
{
    auto child = apvts.state.getChildWithName (patternProperty);
    if (! child.isValid())
        return;

    tb303::Pattern p;
    p.length    = juce::jlimit (1, tb303::Pattern::maxSteps, (int) child.getProperty ("length", 16));
    p.rootNote  = (int) child.getProperty ("rootNote", 36);
    p.transpose = (int) child.getProperty ("transpose", 0);
    p.direction = (tb303::Direction) juce::jlimit (0, 3, (int) child.getProperty ("direction", 0));

    for (int i = 0; i < tb303::Pattern::maxSteps; ++i)
    {
        auto s = child.getChild (i);
        if (! s.isValid())
            continue;
        p.steps[i].note   = (int) s.getProperty ("note", 0);
        p.steps[i].oct    = (int) s.getProperty ("oct", 0);
        p.steps[i].rest   = (bool) s.getProperty ("rest", false);
        p.steps[i].accent = (bool) s.getProperty ("accent", false);
        p.steps[i].slide  = (bool) s.getProperty ("slide", false);
    }

    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        patternCopy = p;
    }
    patternDirty.store (true);
}

//==============================================================================
void Tb303AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    patternToTree();
    auto state = apvts.copyState();
    state.setProperty ("playing", transport.isRequested(), nullptr);
    state.setProperty ("currentPreset", currentPreset, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Tb303AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto newState = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (newState);

    patternFromTree();

    transport.requestPlay ((bool) newState.getProperty ("playing", false));
    currentPreset = (int) newState.getProperty ("currentPreset", 0);

    syncParamsFromState();
    engine.setParams (params);
}

//==============================================================================
// Factory presets
//==============================================================================
namespace
{
    struct FactoryPreset
    {
        const char* name;
        float waveform, tuning, cutoff, resonance, envmod, decay, accent, drive, volume, tempo, swing;
        int length, direction;
        int pattern;   // 0 acid, 1 rave, 2 funk, 3 hi-energy, 4 empty
        int rootNote;
    };

    const FactoryPreset factoryPresets[] =
    {
        { "01 Acid Classic",  1.00f, 0.5f, 0.30f, 0.55f, 0.70f, 0.30f, 0.75f, 0.20f, 0.80f, 0.50f, 0.0f, 16, 0, 0, 36 },
        { "02 Screamer",  0.00f, 0.5f, 0.20f, 0.95f, 0.85f, 0.22f, 0.90f, 0.30f, 0.75f, 0.52f, 0.0f, 16, 0, 0, 36 },
        { "03 Deep Sub",  0.00f, 0.5f, 0.16f, 0.15f, 0.35f, 0.55f, 0.30f, 0.05f, 0.85f, 0.44f, 0.0f, 16, 0, 2, 33 },
        { "04 Squelch It",  1.00f, 0.5f, 0.22f, 0.90f, 0.95f, 0.18f, 0.85f, 0.25f, 0.72f, 0.55f, 0.25f, 16, 0, 0, 36 },
        { "05 Rave Stab",  0.00f, 0.5f, 0.55f, 0.45f, 0.30f, 0.40f, 0.50f, 0.15f, 0.80f, 0.60f, 0.0f, 16, 0, 1, 36 },
        { "06 Funk Bass",  0.00f, 0.5f, 0.36f, 0.42f, 0.50f, 0.35f, 0.60f, 0.15f, 0.82f, 0.46f, 0.0f, 16, 0, 2, 33 },
        { "07 Hard Acid",  1.00f, 0.5f, 0.26f, 0.70f, 0.75f, 0.26f, 0.85f, 0.85f, 0.70f, 0.62f, 0.0f, 16, 0, 3, 36 },
        { "08 Hi-Energy",  1.00f, 0.5f, 0.45f, 0.62f, 0.55f, 0.28f, 0.80f, 0.35f, 0.78f, 0.64f, 0.0f, 16, 0, 3, 36 },
        { "09 Ping Pong",  1.00f, 0.5f, 0.32f, 0.65f, 0.65f, 0.30f, 0.70f, 0.25f, 0.80f, 0.50f, 0.0f, 12, 2, 1, 36 },
        { "10 Empty Pattern",  1.00f, 0.5f, 0.35f, 0.50f, 0.60f, 0.32f, 0.70f, 0.20f, 0.80f, 0.50f, 0.0f, 16, 0, 4, 36 },
    };

    constexpr int numFactoryPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));

    tb303::Pattern patternForIndex (int which)
    {
        switch (which)
        {
            case 1: return factory::raveStab();
            case 2: return factory::funkBass();
            case 3: return factory::hiEnergy();
            case 4: { tb303::Pattern p; p.clear(); return p; }
            default: return factory::classicAcid();
        }
    }
}

int Tb303AudioProcessor::getNumFactoryPresets() { return numFactoryPresets; }

juce::String Tb303AudioProcessor::getFactoryPresetName (int index)
{
    if (index < 0 || index >= numFactoryPresets) return {};
    return factoryPresets[index].name;
}

void Tb303AudioProcessor::loadFactoryPreset (int index)
{
    if (index < 0 || index >= numFactoryPresets)
        return;

    const auto& f = factoryPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
    };

    set ("waveform", f.waveform);
    set ("tuning", f.tuning);
    set ("cutoff", f.cutoff);
    set ("resonance", f.resonance);
    set ("envmod", f.envmod);
    set ("decay", f.decay);
    set ("accent", f.accent);
    set ("drive", f.drive);
    set ("volume", f.volume);
    set ("tempo", f.tempo);
    set ("swing", f.swing);
    set ("transpose", 0.5f);

    if (auto* p = apvts.getParameter ("length"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) f.length));
    if (auto* p = apvts.getParameter ("direction"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) f.direction));

    auto pat = patternForIndex (f.pattern);
    pat.length = f.length;
    pat.direction = (tb303::Direction) f.direction;
    pat.rootNote = f.rootNote;
    setPattern (pat);

    currentPreset = index;
    syncParamsFromState();
}

void Tb303AudioProcessor::applyPresetXml (const juce::String& xml)
{
    if (auto parsed = juce::XmlDocument::parse (xml))
    {
        const void* data = parsed->getText().toRawUTF8();
        setStateInformation (data, (int) std::strlen ((const char*) data));
    }
}

void Tb303AudioProcessor::saveUserPresetFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    getStateInformation (mb);
    f.replaceWithData (mb.getData(), mb.getSize());
}

void Tb303AudioProcessor::loadUserPresetFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (f.loadFileAsData (mb))
    {
        setStateInformation (mb.getData(), (int) mb.getSize());
        currentPreset = -1;
    }
}

//==============================================================================
juce::AudioProcessorEditor* Tb303AudioProcessor::createEditor()
{
    return new Tb303AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Tb303AudioProcessor();
}
