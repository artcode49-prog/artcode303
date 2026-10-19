// PluginEditor.cpp - the ACID-303 front panel.
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace tb303;

//==============================================================================
namespace
{
    constexpr float kPanelInset   = 8.0f;   // outer dark frame
    constexpr float kWoodWidth    = 22.0f;  // wooden side cheeks
    constexpr int   kContentInset = 14;     // metal -> content

    juce::Image decodePng (const void* data, int size)
    {
        if (data == nullptr || size <= 0)
            return {};
        return juce::ImageFileFormat::loadFrom (data, (size_t) size);
    }

    const juce::Image& knobStripImage()
    {
        static const juce::Image img = decodePng (BinaryData::knob_png, BinaryData::knob_pngSize);
        return img;
    }

    const juce::Image& logoImage()
    {
        static const juce::Image img = decodePng (BinaryData::logo_png, BinaryData::logo_pngSize);
        return img;
    }

    const juce::Image& sideWoodImage()
    {
        static const juce::Image img = decodePng (BinaryData::sidewood_png, BinaryData::sidewood_pngSize);
        return img;
    }
}

//==============================================================================
const juce::Colour AcidLookAndFeel::panelDark { 0xff141416 };
const juce::Colour AcidLookAndFeel::panelMid  { 0xff26262c };
const juce::Colour AcidLookAndFeel::orange    { 0xffff8a1e };
const juce::Colour AcidLookAndFeel::red       { 0xffe04230 };
const juce::Colour AcidLookAndFeel::silver    { 0xffb9bbc2 };
const juce::Colour AcidLookAndFeel::text      { 0xffdedee2 };
const juce::Colour AcidLookAndFeel::ledOff    { 0xff3a3a44 };

AcidLookAndFeel::AcidLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, orange);
    setColour (juce::Slider::rotarySliderOutlineColourId, ledOff);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1b20));
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a52));
    setColour (juce::ComboBox::arrowColourId, orange);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1b20));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c33));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void AcidLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto& strip = knobStripImage();

    // Hardware-style knob filmstrip: 128 portrait frames, each the same width
    // as the strip. The frame already contains the body, pointer and orange
    // value arc, so we only pick the matching frame.
    if (strip.isValid() && strip.getWidth() > 0)
    {
        constexpr int frames = 128;
        const int frameH = strip.getHeight() / frames;
        if (frameH > 0)
        {
            const int frame = juce::jlimit (0, frames - 1,
                                            juce::roundToInt (sliderPos * (float) (frames - 1)));
            const float side = juce::jmin (bounds.getWidth(), bounds.getHeight());
            const auto dest = juce::Rectangle<float> (side, side).withCentre (bounds.getCentre());

            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (slider.isEnabled() ? 1.0f : 0.45f);
            g.drawImage (strip,
                         dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                         0.0f, (float) (frame * frameH),
                         (float) strip.getWidth(), (float) frameH);
            g.setOpacity (1.0f);
            return;
        }
    }

    // ---- procedural fallback (only used if the asset is missing) ----
    const auto reduced = bounds.reduced (2.0f);
    const auto radius = juce::jmin (reduced.getWidth(), reduced.getHeight()) * 0.5f;
    const auto centre = reduced.getCentre();
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (ledOff);
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (slider.isEnabled() ? orange : orange.withAlpha (0.4f));
        g.strokePath (valueArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto knobR = radius * 0.74f;
    juce::ColourGradient grad (juce::Colour (0xff54545e), centre.x - knobR * 0.4f, centre.y - knobR * 0.5f,
                               juce::Colour (0xff15151a), centre.x + knobR * 0.9f, centre.y + knobR * 0.9f, true);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (juce::Colour (0xff0c0c0f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    const float pw = juce::jmax (2.0f, knobR * 0.13f);
    pointer.addRoundedRectangle (-pw * 0.5f, -knobR * 0.88f, pw, knobR * 0.52f, pw * 0.5f);
    g.setColour (orange);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void AcidLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider&)
{
    auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
    g.setColour (juce::Colour (0xff101014));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (orange);
    g.fillRoundedRectangle (r.withWidth (sliderPos - r.getX()), 3.0f);
    g.setColour (silver.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

juce::Font AcidLookAndFeel::getLabelFont (juce::Label& l)
{
    return juce::Font (juce::FontOptions (juce::jlimit (9.0f, 14.0f, (float) l.getHeight() * 0.72f)));
}

juce::Font AcidLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return juce::Font (juce::FontOptions (juce::jlimit (10.0f, 13.0f, (float) box.getHeight() * 0.5f)));
}

juce::Font AcidLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (13.0f));
}

//==============================================================================
SwitchButton::SwitchButton (const juce::String& name, const juce::String& top, const juce::String& bottom)
    : juce::Button (name), labelTop (top), labelBottom (bottom)
{
    setClickingTogglesState (true);
}

void SwitchButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff101014));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff4a4a52));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    const bool on = getToggleState();
    auto slot = r.reduced (4.0f);

    // the moving "cap"
    auto capH = slot.getHeight() * 0.5f;
    auto cap = on ? slot.removeFromBottom (capH) : slot.removeFromTop (capH);
    auto capColour = on ? AcidLookAndFeel::orange : juce::Colour (0xff33333a);
    if (highlighted) capColour = capColour.brighter (0.15f);
    if (down)        capColour = capColour.darker (0.2f);
    g.setColour (capColour);
    g.fillRoundedRectangle (cap.reduced (1.0f), 3.0f);

    g.setFont (juce::Font (juce::FontOptions (juce::jmax (8.0f, r.getHeight() * 0.19f))));
    g.setColour (on ? juce::Colours::black : AcidLookAndFeel::text.withAlpha (0.8f));
    g.drawText (labelTop, (int) r.getX(), (int) r.getY() + 2, (int) r.getWidth(), (int) (capH - 2),
                juce::Justification::centred, false);
    g.setColour (on ? AcidLookAndFeel::text.withAlpha (0.85f) : juce::Colours::black.withAlpha (0.85f));
    g.drawText (labelBottom, (int) r.getX(), (int) (r.getBottom() - capH + 2), (int) r.getWidth(), (int) (capH - 2),
                juce::Justification::centred, false);
}

//==============================================================================
namespace
{
    const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String noteNameFor (const Step& s)
    {
        int n = juce::jlimit (0, 12, s.note);
        return juce::String (noteNames[n % 12]) + (n >= 12 ? juce::String ("'") : juce::String());
    }
}

StepGrid::StepGrid (Tb303AudioProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

int StepGrid::rowAt (int y) const
{
    const int header = 16;
    const int gridH = getHeight() - header;
    const int rowH = gridH / numRows;
    if (y < header) return -1;
    return juce::jlimit (0, numRows - 1, (y - header) / juce::jmax (1, rowH));
}

int StepGrid::columnAt (int x) const
{
    const int labelW = 46;
    const int gridW = getWidth() - labelW;
    if (x < labelW) return -1;
    return juce::jlimit (0, 15, (x - labelW) * 16 / juce::jmax (1, gridW));
}

juce::Rectangle<int> StepGrid::cellRect (int col, int row) const
{
    const int labelW = 46, header = 16;
    const int gridW = getWidth() - labelW;
    const int gridH = getHeight() - header;
    const int x = labelW + col * gridW / 16;
    const int x2 = labelW + (col + 1) * gridW / 16;
    const int y = header + row * gridH / numRows;
    const int y2 = header + (row + 1) * gridH / numRows;
    return { x, y, x2 - x, y2 - y };
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto pattern = proc.getPattern();
    const int labelW = 46, header = 16;

    g.setColour (juce::Colour (0xff101014));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    // row labels
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    const char* rowNames[numRows] = { "NOTE", "OCT", "ACC", "SLIDE", "REST" };
    for (int r = 0; r < numRows; ++r)
    {
        auto cell = cellRect (0, r);
        g.setColour (AcidLookAndFeel::text.withAlpha (0.55f));
        g.drawText (rowNames[r], juce::Rectangle<int> (2, cell.getY(), labelW - 6, cell.getHeight()),
                    juce::Justification::centredRight, false);
    }

    for (int c = 0; c < 16; ++c)
    {
        const bool active = c < pattern.length;
        const bool current = (c == highlightStep);

        // column background
        auto colArea = cellRect (c, 0).withBottom (cellRect (c, numRows - 1).getBottom());
        auto bg = colArea.reduced (1, 0).toFloat();
        g.setColour (current ? AcidLookAndFeel::orange.withAlpha (0.16f)
                             : (active ? juce::Colour (0xff1b1b21) : juce::Colour (0xff141418)));
        g.fillRoundedRectangle (bg, 3.0f);

        if (current)
        {
            g.setColour (AcidLookAndFeel::orange.withAlpha (0.5f));
            g.drawRoundedRectangle (bg.reduced (0.5f), 3.0f, 1.0f);
        }

        // step number
        g.setColour (active ? (current ? AcidLookAndFeel::orange : AcidLookAndFeel::text.withAlpha (0.65f))
                            : AcidLookAndFeel::text.withAlpha (0.2f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (juce::String (c + 1), juce::Rectangle<int> (colArea.getX(), 0, colArea.getWidth(), header - 1),
                    juce::Justification::centred, false);

        const auto& s = pattern.steps[c];

        // ---- note
        auto noteCell = cellRect (c, rowNote).reduced (2);
        g.setColour (s.rest ? juce::Colour (0xff26262c) : AcidLookAndFeel::orange);
        g.fillRoundedRectangle (noteCell.toFloat(), 3.0f);
        g.setColour (s.rest ? AcidLookAndFeel::text.withAlpha (0.35f) : juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, noteCell.getHeight() * 0.55f), juce::Font::bold)));
        g.drawText (s.rest ? juce::String ("-") : noteNameFor (s), noteCell, juce::Justification::centred, false);

        // ---- octave
        auto octCell = cellRect (c, rowOct).reduced (2);
        g.setColour (s.oct == 0 ? juce::Colour (0xff26262c) : AcidLookAndFeel::silver.withAlpha (0.75f));
        g.fillRoundedRectangle (octCell.toFloat(), 3.0f);
        g.setColour (s.oct == 0 ? AcidLookAndFeel::text.withAlpha (0.3f) : juce::Colours::black);
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (8.0f, octCell.getHeight() * 0.6f), juce::Font::bold)));
        g.drawText (s.oct > 0 ? "+1" : (s.oct < 0 ? "-1" : "0"), octCell, juce::Justification::centred, false);

        // ---- toggles
        struct ToggleSpec { int row; bool on; juce::Colour colour; };
        const ToggleSpec specs[3] = { { rowAccent, s.accent, AcidLookAndFeel::red },
                                      { rowSlide,  s.slide,  AcidLookAndFeel::orange },
                                      { rowRest,   s.rest,   AcidLookAndFeel::silver } };
        for (auto& spec : specs)
        {
            auto cell = cellRect (c, spec.row).reduced (2);
            auto led = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 14), juce::jmin (cell.getHeight(), 12)).toFloat();
            g.setColour (spec.on ? spec.colour : AcidLookAndFeel::ledOff);
            g.fillEllipse (led);
            if (spec.on)
            {
                g.setColour (spec.colour.brighter (0.4f).withAlpha (0.35f));
                g.drawEllipse (led.expanded (1.5f), 1.5f);
            }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragMoved = false;
    dragRow = rowAt (e.y);
    dragCol = columnAt (e.x);
    dragStartY = e.y;

    if (dragRow < 0 || dragCol < 0)
        return;

    auto s = proc.getStep (dragCol);
    dragStartNote = s.note;

    switch (dragRow)
    {
        case rowNote:
            if (s.rest) { s.rest = false; proc.setStep (dragCol, s); }
            break;
        case rowOct:
            s.oct = s.oct >= 1 ? -1 : s.oct + 1;
            proc.setStep (dragCol, s);
            break;
        case rowAccent: s.accent = ! s.accent; proc.setStep (dragCol, s); break;
        case rowSlide:  s.slide  = ! s.slide;  proc.setStep (dragCol, s); break;
        case rowRest:
            s.rest = ! s.rest;
            proc.setStep (dragCol, s);
            break;
        default: break;
    }
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0)
        return;

    const int col = columnAt (e.x);
    if (col < 0)
        return;

    if (dragRow == rowNote)
    {
        const int delta = (dragStartY - e.y) / 7;
        if (delta != 0) dragMoved = true;
        auto s = proc.getStep (dragCol);
        s.note = juce::jlimit (0, 12, dragStartNote + delta);
        s.rest = false;
        if (s.note != proc.getStep (dragCol).note)
            proc.setStep (dragCol, s);
        return;
    }

    // paint toggles across columns
    if (col != dragCol)
    {
        dragCol = col;
        dragMoved = true;
        auto s = proc.getStep (col);
        switch (dragRow)
        {
            case rowAccent: s.accent = true; break;
            case rowSlide:  s.slide = true;  break;
            case rowRest:   s.rest = true;   break;
            case rowOct:    s.oct = (s.oct >= 1 ? -1 : s.oct + 1); break;
            default: break;
        }
        proc.setStep (col, s);
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragRow == rowNote && ! dragMoved && dragCol >= 0)
    {
        // plain click on the note cell steps through the scale
        auto s = proc.getStep (dragCol);
        s.note = (s.note + 1) % 13;
        s.rest = false;
        proc.setStep (dragCol, s);
    }
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAt (e.y), col = columnAt (e.x);
    if (row < 0 || col < 0) return;
    auto s = proc.getStep (col);
    if (row == rowNote) { s.note = 0; s.rest = false; }
    else if (row == rowOct) s.oct = 0;
    proc.setStep (col, s);
}

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (rowAt (e.y) == rowNote ? juce::MouseCursor::UpDownResizeCursor
                                           : juce::MouseCursor::PointingHandCursor);
}

//==============================================================================
Tb303AudioProcessorEditor::Tb303AudioProcessorEditor (Tb303AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      stepGrid (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    auto& state = proc.apvts;

    struct KnobSpec { const char* id; const char* label; bool bipolar; };
    const KnobSpec knobSpecs[] =
    {
        { "tuning",    "TUNE",     true  },
        { "cutoff",    "CUT OFF",  false },
        { "resonance", "RESO",     false },
        { "envmod",    "ENV MOD",  false },
        { "decay",     "DECAY",    false },
        { "accent",    "ACCENT",   false },
        { "drive",     "DRIVE",    false },
        { "volume",    "VOLUME",   false },
    };

    for (auto& spec : knobSpecs)
    {
        auto* slider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag,
                                         juce::Slider::NoTextBox);
        slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextValueSuffix ("");
        addAndMakeVisible (slider);
        knobs.add (slider);
        sliderAttachments[spec.id] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                                        state, spec.id, *slider);

        auto* label = new juce::Label ({}, spec.label);
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, AcidLookAndFeel::text.withAlpha (0.75f));
        addAndMakeVisible (label);
        knobLabels.add (label);
    }

    addAndMakeVisible (waveSwitch);
    waveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "waveform", waveSwitch);

    // ---- transport
    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonOnColourId, AcidLookAndFeel::orange);
    playButton.onClick = [this] { proc.setPlaying (playButton.getToggleState()); };
    addAndMakeVisible (playButton);

    randButton.onClick = [this]
    {
        auto p = proc.getPattern();
        juce::Random r;
        const int scale[5] = { 0, 3, 5, 7, 10 };
        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            p.steps[i].rest   = r.nextFloat() < 0.18f;
            p.steps[i].note   = scale[r.nextInt (5)] + (r.nextFloat() < 0.25f ? 12 : 0);
            p.steps[i].accent = r.nextFloat() < 0.3f;
            p.steps[i].slide  = r.nextFloat() < 0.25f;
            p.steps[i].oct    = 0;
        }
        proc.setPattern (p);
        stepGrid.repaint();
    };
    addAndMakeVisible (randButton);

    clearButton.onClick = [this]
    {
        auto p = proc.getPattern();
        p.clear();
        p.length = proc.getPattern().length;
        p.direction = proc.getPattern().direction;
        p.rootNote = proc.getPattern().rootNote;
        proc.setPattern (p);
        stepGrid.repaint();
    };
    addAndMakeVisible (clearButton);

    copyButton.onClick = [this] { clipboard = proc.getPattern(); hasClipboard = true; };
    pasteButton.onClick = [this]
    {
        if (hasClipboard) { proc.setPattern (clipboard); stepGrid.repaint(); }
    };
    addAndMakeVisible (copyButton);
    addAndMakeVisible (pasteButton);

    // ---- direction / clock / length
    directionBox.addItemList ({ "FWD", "REV", "PING", "RND" }, 1);
    addAndMakeVisible (directionBox);
    directionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "direction", directionBox);

    clockBox.addItemList ({ "INT", "HOST" }, 1);
    addAndMakeVisible (clockBox);
    clockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "clock", clockBox);

    for (int i = 1; i <= 16; ++i)
        lengthBox.addItem (juce::String (i), i);
    addAndMakeVisible (lengthBox);
    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "length", lengthBox);

    auto& tempoState = proc.apvts;
    auto* tempoSlider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    tempoSlider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (tempoSlider);
    knobs.add (tempoSlider);
    sliderAttachments["tempo"] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                                     tempoState, "tempo", *tempoSlider);
    auto* tempoLabel = new juce::Label ({}, "TEMPO");
    tempoLabel->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (tempoLabel);
    knobLabels.add (tempoLabel);

    auto* swingSlider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    swingSlider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (swingSlider);
    knobs.add (swingSlider);
    sliderAttachments["swing"] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                                     tempoState, "swing", *swingSlider);
    auto* swingLabel = new juce::Label ({}, "SWING");
    swingLabel->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (swingLabel);
    knobLabels.add (swingLabel);

    auto* trSlider = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    trSlider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                   juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (trSlider);
    knobs.add (trSlider);
    sliderAttachments["transpose"] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                                         tempoState, "transpose", *trSlider);
    auto* trLabel = new juce::Label ({}, "TRANSP");
    trLabel->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (trLabel);
    knobLabels.add (trLabel);

    // ---- presets
    for (int i = 0; i < Tb303AudioProcessor::getNumFactoryPresets(); ++i)
        presetBox.addItem (Tb303AudioProcessor::getFactoryPresetName (i), i + 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
        {
            proc.loadFactoryPreset (index);
            stepGrid.repaint();
        }
    };
    addAndMakeVisible (presetBox);

    prevPreset.onClick = [this]
    {
        const int n = Tb303AudioProcessor::getNumFactoryPresets();
        const int next = (presetBox.getSelectedId() - 1 + n - 1) % n;
        presetBox.setSelectedId (next + 1, juce::sendNotificationSync);
    };
    nextPreset.onClick = [this]
    {
        const int n = Tb303AudioProcessor::getNumFactoryPresets();
        const int next = (presetBox.getSelectedId() - 1 + 1) % n;
        presetBox.setSelectedId (next + 1, juce::sendNotificationSync);
    };
    savePreset.onClick = [this] { choosePresetFile (true); };
    loadPreset.onClick = [this] { choosePresetFile (false); };
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (loadPreset);

    addAndMakeVisible (stepGrid);

    keyboard.setKeyWidth (20.0f);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setAvailableRange (36, 72);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe8e8ec));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1a1e));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff111114));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, AcidLookAndFeel::orange.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, AcidLookAndFeel::orange);
    addAndMakeVisible (keyboard);

    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::textColourId, AcidLookAndFeel::orange);
    bpmLabel.setText ("130.0\nBPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, AcidLookAndFeel::text.withAlpha (0.5f));
    statusLabel.setFont (juce::Font (juce::FontOptions (10.0f)));
    addAndMakeVisible (statusLabel);

    refreshFromProcessor();

    setResizable (true, false);
    setResizeLimits (760, 470, 1600, 1000);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (900.0 / 560.0);
    setSize (900, 560);

    startTimerHz (30);
}

Tb303AudioProcessorEditor::~Tb303AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void Tb303AudioProcessorEditor::refreshFromProcessor()
{
    playButton.setToggleState (proc.isPlaying(), juce::dontSendNotification);
}

void Tb303AudioProcessorEditor::stepChanged() {}

void Tb303AudioProcessorEditor::timerCallback()
{
    stepGrid.setHighlightStep (proc.isPlaying() ? proc.getCurrentStep() : -1);

    const double bpm = proc.getCurrentBpm();
    bpmLabel.setText (juce::String (bpm, 1) + "\nBPM", juce::dontSendNotification);

    if (playButton.getToggleState() != proc.isPlaying())
        playButton.setToggleState (proc.isPlaying(), juce::dontSendNotification);

    statusLabel.setText ("OUT " + juce::String (juce::Decibels::gainToDecibels (proc.getOutputLevel(), -60.0f), 1) + " dB",
                         juce::dontSendNotification);
}

void Tb303AudioProcessorEditor::choosePresetFile (bool save)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("ACID-303 Patches");
    if (! dir.exists()) dir.createDirectory();

    chooser = std::make_unique<juce::FileChooser> (save ? "Save patch" : "Load patch",
                                                   dir.getChildFile ("my-patch.acid303"),
                                                   "*.acid303");

    const auto flags = save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this, save] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File())
            return;

        if (save)
        {
            proc.saveUserPresetFile (f.withFileExtension ("acid303"));
        }
        else
        {
            proc.loadUserPresetFile (f);
            presetBox.setSelectedId (0, juce::dontSendNotification);
            refreshFromProcessor();
            stepGrid.repaint();
        }
    });
}

void Tb303AudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg (juce::Colour (0xff2a2a30), bounds.getCentreX(), 0.0f,
                             juce::Colour (0xff101013), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (bounds);

    //------------------------------------------------------------------ panel
    auto panel = bounds.reduced (kPanelInset);

    juce::ColourGradient panelGrad (juce::Colour (0xff23232a), 0.0f, panel.getY(),
                                    juce::Colour (0xff17171c), 0.0f, panel.getBottom(), false);
    g.setGradientFill (panelGrad);
    g.fillRoundedRectangle (panel, 8.0f);

    // wooden side cheeks, clipped to the rounded panel outline
    if (const auto& wood = sideWoodImage(); wood.isValid())
    {
        juce::Path shape;
        shape.addRoundedRectangle (panel, 8.0f);

        g.saveState();
        g.reduceClipRegion (shape);
        g.drawImage (wood, panel.getX(), panel.getY(), kWoodWidth, panel.getHeight(),
                     0, 0, wood.getWidth(), wood.getHeight());
        g.drawImage (wood, panel.getRight() - kWoodWidth, panel.getY(), kWoodWidth, panel.getHeight(),
                     0, 0, wood.getWidth(), wood.getHeight());
        g.restoreState();
    }

    g.setColour (juce::Colour (0xff3d3d46));
    g.drawRoundedRectangle (panel.reduced (0.5f), 8.0f, 1.0f);

    // content area (same maths as resized())
    auto content = panel.reduced ((float) kContentInset)
                        .withTrimmedLeft (kWoodWidth)
                        .withTrimmedRight (kWoodWidth);

    //------------------------------------------------------------------ header
    auto header = content.removeFromTop (46.0f);
    auto brand  = header.removeFromLeft (484.0f);

    if (const auto& logo = logoImage(); logo.isValid())
    {
        auto logoRow = brand.removeFromTop (34.0f);
        const float lh = 30.0f;
        const float lw = lh * (float) logo.getWidth() / (float) logo.getHeight();
        auto dest = juce::Rectangle<float> (lw, lh).withX (logoRow.getX()).withY (logoRow.getY() + 1.0f);
        g.drawImage (logo, dest, juce::RectanglePlacement::centred, false);
    }
    else
    {
        g.setColour (AcidLookAndFeel::orange);
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
        g.drawText ("ACID-303", brand.removeFromTop (34.0f), juce::Justification::centredLeft, false);
    }

    g.setColour (AcidLookAndFeel::text.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("TRANSISTOR BASS LINE  /  4 POLE DIODE LADDER  /  16 STEP SEQUENCER",
                brand.withTrimmedBottom (2.0f), juce::Justification::topLeft, false);

    //------------------------------------------------------------------ separators
    g.setColour (juce::Colour (0xff35353d));
    const int y1 = content.getY() + juce::roundToInt (content.getHeight() * 0.32f);
    const int y2 = content.getY() + juce::roundToInt (content.getHeight() * 0.52f);
    g.fillRect (content.getX(), (float) y1, content.getWidth(), 1.0f);
    g.fillRect (content.getX(), (float) y2, content.getWidth(), 1.0f);
}

void Tb303AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (juce::roundToInt (kPanelInset) + kContentInset);
    area = area.withTrimmedLeft (juce::roundToInt (kWoodWidth))
               .withTrimmedRight (juce::roundToInt (kWoodWidth));

    auto header = area.removeFromTop (46);
    header.removeFromLeft (484);
    auto presetArea = header;

    prevPreset.setBounds (presetArea.removeFromLeft (28).reduced (2, 8));
    nextPreset.setBounds (presetArea.removeFromLeft (28).reduced (2, 8));
    loadPreset.setBounds (presetArea.removeFromRight (54).reduced (2, 8));
    savePreset.setBounds (presetArea.removeFromRight (54).reduced (2, 8));
    presetBox.setBounds (presetArea.reduced (6, 8));

    //--------------------------------------------------------------- knob section
    auto knobRow = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.30f));

    const int totalItems = 12; // wave switch + 8 knobs + tempo + swing + transpose
    const int itemW = knobRow.getWidth() / totalItems;
    const int knobH = knobRow.getHeight() - 16;

    auto placeKnob = [&] (int index, juce::Slider* slider, juce::Label* label)
    {
        auto r = knobRow.withX (knobRow.getX() + index * itemW).withWidth (itemW);
        if (slider) slider->setBounds (r.withTrimmedBottom (16).reduced (6, 2));
        if (label)  label->setBounds (r.removeFromBottom (14));
    };

    {
        auto r = knobRow.withX (knobRow.getX()).withWidth (itemW);
        waveSwitch.setBounds (r.withSizeKeepingCentre (juce::jmin (64, itemW - 6), knobH - 4));
    }
    for (int i = 0; i < knobs.size(); ++i)
        placeKnob (i + 1, knobs[i], knobLabels[i]);

    //--------------------------------------------------------------- transport section
    auto transport = area.removeFromTop (juce::roundToInt (area.getHeight() * 0.20f)).reduced (0, 6);

    playButton.setBounds (transport.removeFromLeft (74).reduced (4, 2));

    auto bpmArea = transport.removeFromLeft (66);
    bpmLabel.setBounds (bpmArea.reduced (2, 0));

    auto put = [&transport] (juce::Component& c, int w)
    {
        c.setBounds (transport.removeFromLeft (w).reduced (4, 8));
    };

    put (lengthBox, 62);
    put (directionBox, 74);
    put (clockBox, 76);
    put (randButton, 62);
    put (clearButton, 56);
    put (copyButton, 60);
    put (pasteButton, 66);

    statusLabel.setBounds (transport.reduced (6, 12));

    //--------------------------------------------------------------- pattern + keyboard
    auto keyboardArea = area.removeFromBottom (juce::jmin (92, area.getHeight() / 3)).reduced (0, 6);
    // 22 white keys between C2 and C5, stretched to fill the width
    const float keyW = juce::jlimit (14.0f, 30.0f, (float) keyboardArea.getWidth() / 22.0f);
    keyboard.setKeyWidth (keyW);
    const int kbWidth = juce::roundToInt (keyW * 22.0f);
    keyboard.setBounds (keyboardArea.withSizeKeepingCentre (kbWidth, keyboardArea.getHeight()));

    stepGrid.setBounds (area.reduced (0, 6));
}
