#include "PluginEditor.h"

namespace Palette
{
const juce::Colour background { 0xff1c1916 };
const juce::Colour panel      { 0xff2a2420 };
const juce::Colour panelEdge  { 0xff3d342d };
const juce::Colour cream      { 0xffeee3cc };
const juce::Colour creamDark  { 0xffb9ab90 };
const juce::Colour amber      { 0xffe39b3a };
const juce::Colour rust       { 0xffb5532c };
const juce::Colour ink        { 0xff15120f };
const juce::Colour textDim    { 0xff9c8f7c };
} // namespace Palette

//==============================================================================
PatinaLookAndFeel::PatinaLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::cream);
    setColour (juce::ComboBox::textColourId, Palette::cream);
    setColour (juce::ComboBox::arrowColourId, Palette::amber);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink);
    setColour (juce::ToggleButton::textColourId, Palette::cream);
    setColour (juce::ToggleButton::tickColourId, Palette::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::textDim);
    setColour (juce::TextButton::textColourOffId, Palette::creamDark);
    setColour (juce::TextButton::textColourOnId, Palette::ink);
}

void PatinaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    // Scale ticks
    g.setColour (Palette::textDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p1 = centre.getPointOnCircumference (radius - 1.0f, a);
        const auto p2 = centre.getPointOnCircumference (radius - 4.0f, a);
        g.drawLine ({ p1, p2 }, 1.0f);
    }

    // Value arc
    const float arcR = radius - 7.0f;
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (Palette::ink);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (enabled ? Palette::amber : Palette::textDim);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bakelite-cream knob body
    const float knobR = radius - 12.0f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (knob.translated (0.0f, 2.5f));
    g.setGradientFill (juce::ColourGradient (Palette::cream, knob.getX(), knob.getY(),
                                             Palette::creamDark, knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (Palette::ink.withAlpha (0.5f));
    g.drawEllipse (knob, 1.0f);

    // Pointer
    const auto tip = centre.getPointOnCircumference (knobR - 3.0f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.25f, angle);
    g.setColour (enabled ? Palette::rust : Palette::textDim);
    g.drawLine ({ base, tip }, 3.0f);
}

void PatinaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? Palette::amber : (over ? Palette::panelEdge.brighter (0.15f) : Palette::ink));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? Palette::amber.brighter (0.3f) : Palette::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void PatinaLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (1.0f);
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::amber);
    g.fillPath (arrow);
}

juce::Font PatinaLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::FontOptions (14.0f); }

juce::Font PatinaLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::FontOptions (14.0f, juce::Font::bold);
}

juce::Label* PatinaLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (12.5f));
    l->setColour (juce::Label::textColourId, Palette::creamDark);
    l->setColour (juce::Label::outlineWhenEditingColourId, Palette::amber);
    return l;
}

//==============================================================================
void TransferCurve::setState (int type, float driveDb, float blend)
{
    if (type == satType && juce::approximatelyEqual (driveDb, drive) && juce::approximatelyEqual (blend, blendAmount))
        return;
    satType = type;
    drive = driveDb;
    blendAmount = blend;
    repaint();
}

void TransferCurve::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (r, 6.0f);

    const auto plot = r.reduced (10.0f);
    g.setColour (Palette::panelEdge);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());
    for (float f : { 0.25f, 0.75f })
    {
        g.drawHorizontalLine ((int) (plot.getY() + f * plot.getHeight()), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) (plot.getX() + f * plot.getWidth()), plot.getY(), plot.getBottom());
    }

    // Static curve: the oscilloscope view ignores Crush's sample-and-hold.
    const auto type = (patina::SatType) juce::jmax (0, satType);
    const float gain = patina::dbToGain (drive);
    const float amount = drive / 36.0f;
    auto curveAt = [&] (float x)
    {
        float y;
        if (type == patina::SatType::Crush)
        {
            const float levels = std::exp2 (16.0f - 12.0f * std::pow (amount, 0.7f) - 1.0f);
            y = std::round (std::tanh (x * (1.0f + amount)) * levels) / levels;
        }
        else
        {
            patina::ShaperState st;
            y = patina::Shaper::shape (type, x * gain, amount, 1.0f, st);
        }
        return x + blendAmount * (y - x);
    };

    float peak = 0.0f;
    for (int i = 0; i <= 64; ++i)
        peak = juce::jmax (peak, std::abs (curveAt (-1.0f + (float) i / 32.0f)));
    peak = juce::jmax (peak, 0.05f);

    juce::Path p;
    const int steps = (int) plot.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) steps;
        const float y = curveAt (x) / peak;
        const float px = plot.getX() + (float) i;
        const float py = plot.getCentreY() - y * plot.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (Palette::amber.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (Palette::amber);
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

//==============================================================================
PatinaEditor::PatinaEditor (PatinaProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (drive, ParamIDs::drive, "Drive");
    setupKnob (satTone, ParamIDs::satTone, "Tone");
    setupKnob (satBlend, ParamIDs::satBlend, "Blend");
    setupKnob (time, ParamIDs::time, "Time");
    setupKnob (feedback, ParamIDs::feedback, "Feedback");
    setupKnob (preDelay, ParamIDs::preDelay, "Pre-Delay");
    setupKnob (decay, ParamIDs::decay, "Decay");
    setupKnob (tone, ParamIDs::tone, "Tone");
    setupKnob (age, ParamIDs::age, "Age");
    setupKnob (width, ParamIDs::width, "Width");
    setupKnob (duck, ParamIDs::duck, "Duck");
    setupKnob (mix, ParamIDs::mix, "Mix");
    setupKnob (output, ParamIDs::output, "Output");

    setupCombo (satType, satTypeLabel, ParamIDs::satType, "Character", satTypeAtt);
    setupCombo (satPos, satPosLabel, ParamIDs::satPos, "Position", satPosAtt);
    setupCombo (division, divisionLabel, ParamIDs::division, "Division", divisionAtt);

    satPos.setTooltip ("Pre: drive the input. Space: drive the echo loop / reverb tank. Post: drive the final mix.");
    age.slider.setTooltip ("Wow, flutter, wobble, hiss and darkening - everything a worn machine adds.");

    addAndMakeVisible (curve);
    modeInfo.setFont (juce::FontOptions (12.5f).withStyle ("Italic"));
    modeInfo.setColour (juce::Label::textColourId, Palette::textDim);
    modeInfo.setJustificationType (juce::Justification::topLeft);
    modeInfo.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (modeInfo);

    addAndMakeVisible (sync);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::sync, sync);

    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeButtons[i];
        b.setButtonText (PatinaProcessor::modeNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAtt = std::make_unique<juce::ParameterAttachment> (
        *processor.apvts.getParameter (ParamIDs::mode), [this] (float v)
        {
            currentMode = juce::roundToInt (v);
            for (int i = 0; i < 4; ++i)
                modeButtons[i].setToggleState (i == currentMode, juce::dontSendNotification);
            refreshVisibility();
        });
    modeAtt->sendInitialUpdate();

    setSize (900, 470);
    startTimerHz (15);
}

PatinaEditor::~PatinaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PatinaEditor::setupKnob (Knob& k, const char* paramID, const juce::String& name)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);
    k.label.setText (name.toUpperCase(), juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, k.slider);
}

void PatinaEditor::setupCombo (juce::ComboBox& box, juce::Label& label, const char* paramID, const juce::String& name,
                               std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramID)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, paramID, box);
}

void PatinaEditor::timerCallback()
{
    auto& state = processor.apvts;
    curve.setState ((int) state.getRawParameterValue (ParamIDs::satType)->load(),
                    state.getRawParameterValue (ParamIDs::drive)->load(),
                    state.getRawParameterValue (ParamIDs::satBlend)->load());

    const bool s = sync.getToggleState();
    if (s != lastSync)
    {
        lastSync = s;
        refreshVisibility();
    }
}

void PatinaEditor::refreshVisibility()
{
    const bool echo = currentMode <= 1;
    const bool synced = sync.getToggleState();

    time.slider.setVisible (echo);       time.label.setVisible (echo);
    feedback.slider.setVisible (echo);   feedback.label.setVisible (echo);
    preDelay.slider.setVisible (! echo); preDelay.label.setVisible (! echo);
    decay.slider.setVisible (! echo);    decay.label.setVisible (! echo);

    sync.setVisible (echo);
    division.setVisible (echo);
    divisionLabel.setVisible (echo);
    static const char* descriptions[] = {
        "Tape loop echo: motor glide on time changes, wow, flutter and drift, "
        "head-bump and roll-off in the loop, tape compression and hiss.",
        "Bucket-brigade analog delay: bandwidth narrows as the clock slows, "
        "chorus wobble, hard op-amp knee and compander noise.",
        "Spring tank: dispersive allpass springs give the drip and boing; "
        "Age drives the input transformer harder.",
        "Steel plate: dense figure-of-eight tank with no discrete echoes; "
        "Age adds wander and darker damping pads."
    };
    modeInfo.setText (descriptions[juce::jlimit (0, 3, currentMode)], juce::dontSendNotification);
    time.slider.setEnabled (! synced);
    division.setEnabled (synced);
    repaint();
}

void PatinaEditor::layoutKnob (Knob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (16));
    k.slider.setBounds (r);
}

void PatinaEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (54);

    satPanel = area.removeFromLeft (270);
    area.removeFromLeft (12);
    outPanel = area.removeFromRight (110);
    area.removeFromRight (12);
    spacePanel = area;

    constexpr int knobH = 112;

    // Saturation
    {
        auto r = satPanel.reduced (12);
        r.removeFromTop (26);
        auto row = r.removeFromTop (44);
        auto left = row.removeFromLeft (row.getWidth() / 2).reduced (4, 0);
        auto right = row.reduced (4, 0);
        satTypeLabel.setBounds (left.removeFromTop (16));
        satType.setBounds (left.withHeight (26));
        satPosLabel.setBounds (right.removeFromTop (16));
        satPos.setBounds (right.withHeight (26));

        r.removeFromTop (18);
        auto knobs = r.removeFromTop (knobH);
        const int w = knobs.getWidth() / 3;
        layoutKnob (drive, knobs.removeFromLeft (w));
        layoutKnob (satTone, knobs.removeFromLeft (w));
        layoutKnob (satBlend, knobs);

        r.removeFromTop (16);
        curve.setBounds (r.reduced (4, 0));
    }

    // Space
    {
        auto r = spacePanel.reduced (12);
        r.removeFromTop (26);
        auto buttons = r.removeFromTop (30);
        const int bw = buttons.getWidth() / 4;
        for (auto& b : modeButtons)
            b.setBounds (buttons.removeFromLeft (bw).reduced (3, 0));

        r.removeFromTop (22);
        auto knobs = r.removeFromTop (knobH);
        const int w = knobs.getWidth() / 5;
        auto first = knobs.removeFromLeft (w);
        auto second = knobs.removeFromLeft (w);
        layoutKnob (time, first);
        layoutKnob (preDelay, first);
        layoutKnob (feedback, second);
        layoutKnob (decay, second);
        layoutKnob (tone, knobs.removeFromLeft (w));
        layoutKnob (age, knobs.removeFromLeft (w));
        layoutKnob (width, knobs);

        r.removeFromTop (14);
        auto bottom = r.removeFromTop (44);
        sync.setBounds (bottom.removeFromLeft (90).withTrimmedTop (16).withHeight (26));
        auto div = bottom.removeFromLeft (120);
        divisionLabel.setBounds (div.removeFromTop (16));
        division.setBounds (div.withHeight (26));

        r.removeFromTop (16);
        modeInfo.setBounds (r.removeFromTop (40));
    }

    // Output
    {
        auto r = outPanel.reduced (6, 12);
        r.removeFromTop (24);
        const int h = r.getHeight() / 3;
        layoutKnob (duck, r.removeFromTop (h).withTrimmedBottom (8));
        layoutKnob (mix, r.removeFromTop (h).withTrimmedBottom (8));
        layoutKnob (output, r.withTrimmedBottom (8));
    }
}

void PatinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Header
    auto header = getLocalBounds().reduced (14).removeFromTop (46);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("PATINA", header.removeFromLeft (170), juce::Justification::centredLeft);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("tape  /  bucket brigade  /  spring  /  plate  +  saturation", header,
                juce::Justification::centredLeft);

    g.setColour (Palette::amber);
    g.fillRect (14, 62, getWidth() - 28, 2);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Palette::amber);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, r.reduced (14, 10).removeFromTop (16), juce::Justification::centredLeft);
    };

    drawPanel (satPanel, "SATURATION");
    drawPanel (spacePanel, "SPACE");
    drawPanel (outPanel, "OUTPUT");

    // Rack screws
    g.setColour (Palette::panelEdge);
    for (auto pt : { juce::Point<float> (8.0f, 8.0f), juce::Point<float> ((float) getWidth() - 8.0f, 8.0f),
                     juce::Point<float> (8.0f, (float) getHeight() - 8.0f),
                     juce::Point<float> ((float) getWidth() - 8.0f, (float) getHeight() - 8.0f) })
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
}
