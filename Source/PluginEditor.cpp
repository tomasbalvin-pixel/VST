#include "PluginEditor.h"

namespace
{
namespace Ink
{
const juce::Colour paper  { 0xffeeede9 };
const juce::Colour ink    { 0xff1c1c1b };
const juce::Colour mid    { 0xff8d8b86 };
const juce::Colour faint  { 0xffd4d2cc };
const juce::Colour accent { 0xffff5a1f };
} // namespace Ink

juce::Font uiFont (float height, bool medium = false)
{
#if JUCE_WINDOWS
    const juce::String face = "Segoe UI";
#elif JUCE_MAC
    const juce::String face = "Helvetica Neue";
#else
    const juce::String face = juce::Font::getDefaultSansSerifFontName();
#endif
    return juce::FontOptions (face, height, medium ? juce::Font::bold : juce::Font::plain);
}

juce::Font headingFont() { return uiFont (10.5f, true).withExtraKerningFactor (0.18f); }

void drawHeading (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text)
{
    g.setColour (Ink::mid);
    g.setFont (headingFont());
    g.drawText (text.toUpperCase(), r, juce::Justification::topLeft);
}
} // namespace

//==============================================================================
MinimalLookAndFeel::MinimalLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Ink::ink);
    setColour (juce::ComboBox::arrowColourId, Ink::mid);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour (juce::PopupMenu::textColourId, Ink::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Ink::paper);
    setColour (juce::PopupMenu::highlightedTextColourId, Ink::ink);
    setColour (juce::TooltipWindow::backgroundColourId, Ink::ink);
    setColour (juce::TooltipWindow::textColourId, Ink::paper);
}

void MinimalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 7.0f;
    const auto c = bounds.getCentre();
    const bool enabled = s.isEnabled();
    const bool bipolar = s.getProperties().contains ("bipolar");
    const float angle = startAngle + pos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (Ink::faint);
    g.strokePath (track, stroke);

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (enabled ? Ink::ink : Ink::faint.darker (0.15f));
        g.strokePath (value, stroke);
    }

    g.setColour (enabled ? Ink::ink : Ink::faint.darker (0.15f));
    g.drawLine ({ c.getPointOnCircumference (r * 0.35f, angle), c.getPointOnCircumference (r - 5.0f, angle) }, 2.0f);

    // LFO modulation: accent range on an outer ring and a live dot
    const float modDepth = (float) s.getProperties().getWithDefault ("modDepth", 0.0f);
    if (modDepth > 0.0f && enabled)
    {
        const float modOffset = (float) s.getProperties().getWithDefault ("modOffset", 0.0f);
        const float lo = juce::jlimit (0.0f, 1.0f, pos - modDepth * 0.5f);
        const float hi = juce::jlimit (0.0f, 1.0f, pos + modDepth * 0.5f);
        const float ro = r + 5.0f;
        juce::Path range;
        range.addCentredArc (c.x, c.y, ro, ro, 0.0f,
                             startAngle + lo * (endAngle - startAngle), startAngle + hi * (endAngle - startAngle), true);
        g.setColour (Ink::accent.withAlpha (0.45f));
        g.strokePath (range, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float live = juce::jlimit (0.0f, 1.0f, pos + modOffset);
        g.setColour (Ink::accent);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f)
                           .withCentre (c.getPointOnCircumference (ro, startAngle + live * (endAngle - startAngle))));
    }
}

void MinimalLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 5.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (Ink::accent);
        g.fillEllipse (dot);
    }
    else
    {
        g.setColour (over ? Ink::ink : Ink::mid);
        g.drawEllipse (dot.reduced (0.5f), 1.0f);
    }
    g.setColour (on || over ? Ink::ink : Ink::mid);
    g.setFont (uiFont (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
}

void MinimalLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    g.setColour (box.isMouseOver (true) ? Ink::mid : Ink::faint);
    g.fillRect (0.0f, (float) h - 1.0f, (float) w, 1.0f);

    juce::Path chevron;
    const float cx = (float) w - 7.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (Ink::mid);
    g.strokePath (chevron, juce::PathStrokeType (1.2f));
}

void MinimalLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (0, 0, box.getWidth() - 16, box.getHeight() - 1);
    label.setFont (getComboBoxFont (box));
    label.setBorderSize ({});
}

juce::Font MinimalLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }
juce::Font MinimalLookAndFeel::getPopupMenuFont() { return uiFont (13.0f); }

void MinimalLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colours::white);
    g.setColour (Ink::faint);
    g.drawRect (0, 0, w, h, 1);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String l)
    : label (std::move (l))
{
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (label.isNotEmpty())
        r.removeFromTop (18);
    r.removeFromBottom (18);
    const int side = juce::jmin (r.getWidth(), r.getHeight(), 58);
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool enabled = isEnabled() && slider.isEnabled();
    if (label.isNotEmpty())
    {
        g.setColour (Ink::mid);
        g.setFont (uiFont (12.0f));
        g.drawText (label, r.removeFromTop (18.0f), juce::Justification::centred);
    }
    g.setColour (enabled ? Ink::ink : Ink::faint.darker (0.2f));
    g.setFont (uiFont (12.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (18.0f), juce::Justification::centred);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::StringArray w)
    : words (std::move (w))
{
    attachment = std::make_unique<juce::ParameterAttachment> (*state.getParameter (paramID), [this] (float v)
    {
        selected = juce::roundToInt (v);
        repaint();
    });
    attachment->sendInitialUpdate();
}

juce::Rectangle<float> Choice::wordBounds (int index) const
{
    const auto font = uiFont (13.0f);
    float x = 0.0f;
    for (int i = 0; i < index; ++i)
        x += juce::GlyphArrangement::getStringWidth (font, words[i]) + 16.0f;
    return { x, 0.0f, juce::GlyphArrangement::getStringWidth (font, words[index]), (float) getHeight() };
}

int Choice::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < words.size(); ++i)
        if (wordBounds (i).expanded (8.0f, 0.0f).contains (p))
            return i;
    return -1;
}

void Choice::paint (juce::Graphics& g)
{
    g.setFont (uiFont (13.0f));
    for (int i = 0; i < words.size(); ++i)
    {
        const auto b = wordBounds (i);
        const bool sel = i == selected;
        g.setColour (sel ? Ink::ink : (i == hovered ? Ink::ink.withAlpha (0.6f) : Ink::mid));
        g.drawText (words[i], b, juce::Justification::centredLeft);
        if (sel)
        {
            g.setColour (Ink::accent);
            g.fillRect (b.getX(), b.getBottom() - 2.0f, b.getWidth(), 1.5f);
        }
    }
}

void Choice::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0)
        attachment->setValueAsCompleteGesture ((float) i);
}

void Choice::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hovered)
    {
        hovered = i;
        setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void Choice::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
Toggle::Toggle (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String label)
{
    button.setButtonText (label);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, button);
}

//==============================================================================
void CurvePlot::setState (int type, float driveDb, float blend)
{
    if (type == satType && juce::approximatelyEqual (driveDb, drive) && juce::approximatelyEqual (blend, blendAmount))
        return;
    satType = type;
    drive = driveDb;
    blendAmount = blend;
    repaint();
}

void CurvePlot::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f, 3.0f);
    g.setColour (Ink::faint);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());

    // Static transfer curve; the plot ignores Crush's sample-and-hold.
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

    float peak = 0.05f;
    for (int i = 0; i <= 64; ++i)
        peak = juce::jmax (peak, std::abs (curveAt (-1.0f + (float) i / 32.0f)));

    juce::Path p;
    const int steps = (int) r.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) steps;
        const float py = r.getCentreY() - curveAt (x) / peak * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (r.getX(), py); else p.lineTo (r.getX() + (float) i, py);
    }
    g.setColour (Ink::ink);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LfoPlot::setState (int newShape, float newPhase, float newValue)
{
    shape = newShape;
    phase = newPhase;
    value = newValue;
    repaint();
}

void LfoPlot::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
    g.setColour (Ink::faint);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    // Representative random sequence for the S&H and drift shapes.
    static const float randoms[] = { 0.3f, -0.7f, 0.9f, -0.2f, 0.55f, -0.85f, 0.1f, 0.7f, -0.45f };
    const auto s = (patina::Lfo::Shape) shape;
    auto shapeAt = [&] (float cycles)
    {
        const int seg = (int) cycles;
        const float ph = cycles - (float) seg;
        switch (s)
        {
            case patina::Lfo::Shape::Sine:       return std::sin (patina::kTwoPi * ph);
            case patina::Lfo::Shape::Triangle:   return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case patina::Lfo::Shape::Square:     return ph < 0.5f ? 1.0f : -1.0f;
            case patina::Lfo::Shape::Saw:        return 1.0f - 2.0f * ph;
            case patina::Lfo::Shape::SampleHold: return randoms[seg % 9];
            case patina::Lfo::Shape::Drift:
            {
                const float t = 0.5f - 0.5f * std::cos (patina::kPi * ph);
                return randoms[seg % 9] + t * (randoms[(seg + 1) % 9] - randoms[seg % 9]);
            }
        }
        return 0.0f;
    };

    constexpr float cyclesShown = 2.0f;
    juce::Path p;
    const int steps = (int) r.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float cyc = std::min (cyclesShown * (float) i / (float) steps, cyclesShown - 0.0001f);
        const float py = r.getCentreY() - shapeAt (cyc) * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (r.getX(), py); else p.lineTo (r.getX() + (float) i, py);
    }
    g.setColour (Ink::mid);
    g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const juce::Point<float> dot { r.getX() + r.getWidth() * phase / cyclesShown, r.getCentreY() - value * r.getHeight() * 0.5f };
    g.setColour (Ink::accent);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
}

void LevelBar::setLevel (float rms, float peak)
{
    // Simple ballistics at the 30 Hz UI rate; peaks at or above -0.2 dBFS light the bar for ~1 s.
    const float db = juce::Decibels::gainToDecibels (rms, -60.0f);
    levelDb += (db > levelDb ? 0.5f : 0.15f) * (db - levelDb);
    hold = peak >= 0.977f ? 1.0f : std::max (0.0f, hold - 0.035f);
    repaint();
}

void LevelBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto text = r.removeFromRight (52.0f);
    const auto bar = r.withSizeKeepingCentre (r.getWidth(), 2.0f);
    g.setColour (Ink::faint);
    g.fillRect (bar);
    const float norm = juce::jlimit (0.0f, 1.0f, (levelDb + 48.0f) / 48.0f);
    g.setColour (hold > 0.0f ? Ink::accent : Ink::ink);
    g.fillRect (bar.withWidth (bar.getWidth() * norm));

    g.setColour (hold > 0.0f ? Ink::accent : Ink::mid);
    g.setFont (uiFont (12.0f));
    g.drawText (levelDb <= -59.0f ? juce::String::fromUTF8 ("-\xe2\x88\x9e dB") : juce::String (juce::roundToInt (levelDb)) + " dB",
                text, juce::Justification::centredRight);
}

//==============================================================================
PatinaEditor::PatinaEditor (PatinaProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      satType (p.apvts, ParamIDs::satType, { "tape", "tube", "fuzz", "crush" }),
      satPos (p.apvts, ParamIDs::satPos, { "pre", "loop", "post" }),
      drive (p.apvts, ParamIDs::drive, "drive"),
      satTone (p.apvts, ParamIDs::satTone, "tone"),
      satBlend (p.apvts, ParamIDs::satBlend, "blend"),
      mode (p.apvts, ParamIDs::mode, { "tape", "bbd", "spring", "plate" }),
      time (p.apvts, ParamIDs::time, "time"),
      feedback (p.apvts, ParamIDs::feedback, "feedback"),
      preDelay (p.apvts, ParamIDs::preDelay, "pre-delay"),
      decay (p.apvts, ParamIDs::decay, "decay"),
      tone (p.apvts, ParamIDs::tone, "tone"),
      age (p.apvts, ParamIDs::age, "age"),
      width (p.apvts, ParamIDs::width, "width"),
      division (p.apvts, ParamIDs::division, "note"),
      sync (p.apvts, ParamIDs::sync, "sync"),
      duck (p.apvts, ParamIDs::duck, "duck"),
      mix (p.apvts, ParamIDs::mix, "mix"),
      output (p.apvts, ParamIDs::output, "output"),
      lfoShape (p.apvts, ParamIDs::lfoShape, { "sine", "tri", "square", "saw", "s&h", "drift" }),
      lfoRate (p.apvts, ParamIDs::lfoRate, "rate"),
      lfoDiv (p.apvts, ParamIDs::lfoDiv, "note"),
      lfoSync (p.apvts, ParamIDs::lfoSync, "sync")
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> {
             &satType, &satPos, &curve, &drive, &satTone, &satBlend, &mode, &time, &feedback, &preDelay, &decay,
             &tone, &age, &width, &division, &sync, &level, &duck, &mix, &output, &lfoShape, &lfoPlot, &lfoRate,
             &lfoDiv, &lfoSync })
        addAndMakeVisible (c);

    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        auto& box = lfoTarget[i];
        for (int t = 0; t < PatinaProcessor::lfoTargetNames.size(); ++t)
            box.addItem (t == 0 ? juce::String ("no target") : PatinaProcessor::lfoTargetNames[t].toLowerCase(), t + 1);
        addAndMakeVisible (box);
        lfoTargetAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            p.apvts, ParamIDs::lfoTarget[i], box);
        lfoDepth[i] = std::make_unique<Knob> (p.apvts, ParamIDs::lfoDepth[i], juce::String());
        lfoDepth[i]->slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (*lfoDepth[i]);
    }

    targetKnobs = { nullptr, &time, &feedback, &preDelay, &decay, &tone, &age, &width, &mix, &drive, &satTone, &satBlend };


    setSize (1000, 600);
    refreshVisibility();
    startTimerHz (30);
}

PatinaEditor::~PatinaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PatinaEditor::timerCallback()
{
    auto& s = processor.apvts;
    auto raw = [&s] (const char* id) { return s.getRawParameterValue (id)->load(); };

    const int m = (int) raw (ParamIDs::mode);
    const bool sy = raw (ParamIDs::sync) > 0.5f;
    if (m != currentMode || sy != lastSync)
    {
        currentMode = m;
        lastSync = sy;
        refreshVisibility();
    }

    curve.setState ((int) raw (ParamIDs::satType), raw (ParamIDs::drive), raw (ParamIDs::satBlend));
    lfoPlot.setState ((int) raw (ParamIDs::lfoShape), processor.lfoPhaseUI.load(), processor.lfoValueUI.load());
    level.setLevel (processor.outputLevelUI.load(), processor.outputPeakUI.exchange (0.0f));

    const bool lfoSynced = raw (ParamIDs::lfoSync) > 0.5f;
    lfoDiv.slider.setEnabled (lfoSynced);
    lfoRate.slider.setEnabled (! lfoSynced);

    updateModulationDisplay();
}

void PatinaEditor::updateModulationDisplay()
{
    std::array<float, kNumModTargets> depth {};
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        const int t = (int) processor.apvts.getRawParameterValue (ParamIDs::lfoTarget[i])->load();
        if (t > 0 && t < kNumModTargets)
            depth[(size_t) t] += std::abs (processor.apvts.getRawParameterValue (ParamIDs::lfoDepth[i])->load());
    }

    for (int t = 1; t < kNumModTargets; ++t)
    {
        auto& props = targetKnobs[(size_t) t]->slider.getProperties();
        const float d = depth[(size_t) t];
        if (d > 0.0f || (float) props.getWithDefault ("modDepth", 0.0f) > 0.0f)
        {
            props.set ("modDepth", d);
            props.set ("modOffset", processor.modOffsetUI[(size_t) t].load());
            targetKnobs[(size_t) t]->slider.repaint();
        }
    }
}

void PatinaEditor::refreshVisibility()
{
    const bool echo = currentMode <= 1;
    time.setVisible (echo);
    feedback.setVisible (echo);
    division.setVisible (echo);
    sync.setVisible (echo);
    preDelay.setVisible (! echo);
    decay.setVisible (! echo);
    time.slider.setEnabled (! lastSync);
    division.slider.setEnabled (lastSync);
    time.repaint();
    division.repaint();

    static const char* descriptions[] = {
        "Tape loop echo with motor glide, wow, flutter, tape compression and hiss.",
        "Bucket-brigade delay. Darker as it gets longer, with chorus wobble.",
        "Spring tank with dispersive drip. Age drives the input harder.",
        "Dense steel plate, no discrete echoes. Age dulls the damping."
    };
    modeText = descriptions[juce::jlimit (0, 3, currentMode)];
    repaint (modeTextArea);
}

void PatinaEditor::resized()
{
    auto area = getLocalBounds().reduced (36, 28);

    header = area.removeFromTop (34);
    area.removeFromTop (26);

    auto top = area.removeFromTop (272);
    area.removeFromTop (30);
    lfoArea = area;

    satArea = top.removeFromLeft (250);
    top.removeFromLeft (44);
    outArea = top.removeFromRight (200);
    top.removeFromRight (44);
    spaceArea = top;

    constexpr int headingH = 26, choiceH = 22, knobH = 104;

    // Saturation
    {
        auto r = satArea;
        r.removeFromTop (headingH);
        satType.setBounds (r.removeFromTop (choiceH));
        r.removeFromTop (6);
        satPos.setBounds (r.removeFromTop (choiceH));
        // Knob row shares its baseline with the Space and Output knobs.
        r.setTop (satArea.getY() + headingH + choiceH + 34);
        auto knobs = r.removeFromTop (knobH);
        const int w = knobs.getWidth() / 3;
        drive.setBounds (knobs.removeFromLeft (w));
        satTone.setBounds (knobs.removeFromLeft (w));
        satBlend.setBounds (knobs);
        r.removeFromTop (18);
        curve.setBounds (r.removeFromTop (juce::jmin (r.getHeight(), 60)));
    }

    // Space
    {
        auto r = spaceArea;
        r.removeFromTop (headingH);
        auto row = r.removeFromTop (choiceH);
        sync.setBounds (row.removeFromRight (64));
        mode.setBounds (row);
        r.removeFromTop (34);
        auto knobs = r.removeFromTop (knobH);
        const int w = knobs.getWidth() / 6;
        auto s1 = knobs.removeFromLeft (w);
        time.setBounds (s1);
        preDelay.setBounds (s1);
        auto s2 = knobs.removeFromLeft (w);
        feedback.setBounds (s2);
        decay.setBounds (s2);
        tone.setBounds (knobs.removeFromLeft (w));
        age.setBounds (knobs.removeFromLeft (w));
        width.setBounds (knobs.removeFromLeft (w));
        division.setBounds (knobs);
        r.removeFromTop (22);
        modeTextArea = r.removeFromTop (40);
    }

    // Output
    {
        auto r = outArea;
        r.removeFromTop (headingH);
        level.setBounds (r.removeFromTop (choiceH));
        r.removeFromTop (34);
        auto knobs = r.removeFromTop (knobH);
        const int w = knobs.getWidth() / 3;
        duck.setBounds (knobs.removeFromLeft (w));
        mix.setBounds (knobs.removeFromLeft (w));
        output.setBounds (knobs);
    }

    // LFO: words and selectors on the first line, plot and knobs below, aligned to the columns above
    {
        auto r = lfoArea;
        r.removeFromTop (headingH);
        auto line = r.removeFromTop (choiceH);
        r.removeFromTop (14);
        auto body = r.removeFromTop (knobH);

        lfoShape.setBounds (line.removeFromLeft (satArea.getWidth()));
        lfoPlot.setBounds (body.removeFromLeft (satArea.getWidth()).withTrimmedTop (18).withTrimmedBottom (18));
        line.removeFromLeft (44);
        body.removeFromLeft (44);

        const int kw = spaceArea.getWidth() / 6;
        auto rateLine = line.removeFromLeft (kw * 2);
        lfoSync.setBounds (rateLine.withWidth (64));
        lfoRate.setBounds (body.removeFromLeft (kw));
        lfoDiv.setBounds (body.removeFromLeft (kw));

        line.removeFromLeft (kw);
        body.removeFromLeft (kw);
        const int sw = line.getWidth() / ParamIDs::numLfoSlots;
        for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
        {
            lfoTarget[i].setBounds (line.removeFromLeft (sw).reduced (8, 0));
            lfoDepth[i]->setBounds (body.removeFromLeft (sw));
        }
    }
}

void PatinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (Ink::paper);

    // Wordmark
    g.setColour (Ink::ink);
    g.setFont (uiFont (24.0f, true).withExtraKerningFactor (-0.02f));
    g.drawText ("patina", header, juce::Justification::centredLeft);
    g.setColour (Ink::mid);
    g.setFont (uiFont (12.5f));
    g.drawText (juce::String::fromUTF8 ("echo \xc2\xb7 reverb \xc2\xb7 saturation \xc2\xb7 lfo"), header,
                juce::Justification::centredRight);

    g.setColour (Ink::faint);
    g.fillRect ((float) header.getX(), (float) header.getBottom() + 12.0f, (float) header.getWidth(), 1.0f);
    g.fillRect ((float) lfoArea.getX(), (float) lfoArea.getY() - 16.0f, (float) lfoArea.getWidth(), 1.0f);

    drawHeading (g, satArea, "Saturation");
    drawHeading (g, spaceArea, "Space");
    drawHeading (g, outArea, "Output");
    drawHeading (g, lfoArea, "LFO");

    // Slot captions sit with the target selectors
    g.setColour (Ink::mid);
    g.setFont (headingFont());
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        const auto b = lfoTarget[i].getBounds();
        g.drawText (juce::String (i + 1), b.withX (b.getX() - 16).withWidth (12), juce::Justification::centredRight);
    }

    g.setColour (Ink::mid);
    g.setFont (uiFont (12.5f));
    g.drawFittedText (modeText, modeTextArea, juce::Justification::topLeft, 2, 1.0f);
}
