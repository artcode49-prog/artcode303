// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// PluginProcessor.h - VST3 wrapper around the TB-303 emulation core.
#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "dsp/Tb303Engine.h"

class Tb303AudioProcessor : public juce::AudioProcessor
{
public:
    Tb303AudioProcessor();
    ~Tb303AudioProcessor() override;

    //========================================================== audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //========================================================== plugin description
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ACID-303"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //========================================================== state
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //========================================================== parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //========================================================== pattern access (message thread)
    tb303::Pattern getPattern() const;
    void setPattern (const tb303::Pattern& p);
    void setStep (int index, const tb303::Step& s);
    tb303::Step getStep (int index) const;

    //========================================================== transport / sequencer
    void setPlaying (bool shouldPlay);
    // Effective transport state (follows the host in Host mode).
    bool isPlaying() const { return transportActive.load(); }
    // The user's internal PLAY button intent (only meaningful in Internal mode).
    bool isInternalPlaying() const { return sequencerRunning.load(); }
    int getCurrentStep() const { return engine.getCurrentStep(); }
    void playNotesFromKeyboard (bool on, int midiNote, float velocity);

    //========================================================== presets
    struct Preset { juce::String name; const char* xml; };
    static int getNumFactoryPresets();
    static juce::String getFactoryPresetName (int index);
    void loadFactoryPreset (int index);
    int getCurrentPreset() const { return currentPreset; }
    void setCurrentPreset (int index) { currentPreset = index; }
    void loadUserPresetFile (const juce::File& f);
    void saveUserPresetFile (const juce::File& f);

    //========================================================== misc
    double getCurrentBpm() const { return currentBpm.load(); }
    float getOutputLevel() const { return outputLevel.load(); }

    juce::MidiKeyboardState keyboardState;

    void handleIncomingMidi (const juce::MidiMessage& m);

private:
    void syncParamsFromState();
    void patternToTree();
    void patternFromTree();
    void applyPresetXml (const juce::String& xml);

    tb303::Engine engine;
    tb303::Params params;

    juce::SpinLock patternLock;
    tb303::Pattern patternCopy;
    std::atomic<bool> patternDirty { true };
    std::atomic<bool> sequencerRunning { false };
    std::atomic<bool> transportActive { false };   // effective playing state (audio thread)
    std::atomic<int>  clockSource { 0 };      // 0 = internal, 1 = host
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<int>  pendingPreset { -1 };
    int currentPreset = 0;

    juce::MidiBuffer incomingMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tb303AudioProcessor)
};
