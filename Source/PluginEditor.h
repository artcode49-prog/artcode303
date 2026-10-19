// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// PluginEditor.h
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// 303 flavoured look and feel: silver knobs, orange accents, dark panel.
class AcidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcidLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static const juce::Colour panelDark, panelMid, orange, red, silver, text, ledOff;
};

//==============================================================================
// A two position hardware style switch (used for the waveform selector).
class SwitchButton : public juce::Button
{
public:
    SwitchButton (const juce::String& name, const juce::String& top, const juce::String& bottom);
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    juce::String labelTop, labelBottom;
};

//==============================================================================
// The 16 step pattern programmer (single canvas, hit tested).
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (Tb303AudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    void setHighlightStep (int s) { if (s != highlightStep) { highlightStep = s; repaint(); } }

private:
    enum Row { rowNote = 0, rowOct, rowAccent, rowSlide, rowRest, numRows };

    int rowAt (int y) const;
    int columnAt (int x) const;
    juce::Rectangle<int> cellRect (int col, int row) const;

    Tb303AudioProcessor& proc;
    int highlightStep = -1;
    int dragRow = -1, dragCol = -1;
    int dragStartY = 0, dragStartNote = 0;
    bool dragMoved = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};

//==============================================================================
class Tb303AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit Tb303AudioProcessorEditor (Tb303AudioProcessor&);
    ~Tb303AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFromProcessor();
    void stepChanged();
    void choosePresetFile (bool save);

    Tb303AudioProcessor& proc;
    AcidLookAndFeel lnf;

    StepGrid stepGrid;
    juce::MidiKeyboardComponent keyboard;

    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::Label> knobLabels;
    std::map<juce::String, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;

    SwitchButton waveSwitch { "Wave", "SQR", "SAW" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> waveAttachment;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton randButton { "RAND" };
    juce::TextButton clearButton { "CLR" };
    juce::TextButton copyButton { "COPY" };
    juce::TextButton pasteButton { "PASTE" };

    juce::ComboBox directionBox, clockBox, lengthBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> directionAttachment, clockAttachment, lengthAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };
    juce::TextButton savePreset { "SAVE" }, loadPreset { "LOAD" };

    juce::Label bpmLabel;
    juce::Label statusLabel;

    tb303::Pattern clipboard;
    bool hasClipboard = false;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tb303AudioProcessorEditor)
};
