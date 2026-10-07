#include "PluginEditor.h"

namespace
{
juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace Colours
{
const juce::Colour enamel     { 0xff6f7d74 };  // hammertone grey-green
const juce::Colour enamelDark { 0xff58655d };
const juce::Colour frame      { 0xff2f3531 };
const juce::Colour silk       { 0xfff1eee4 };  // silkscreen white
const juce::Colour silkDim    { 0xffc9cdc4 };
const juce::Colour bakelite   { 0xff141414 };
const juce::Colour vfd        { 0xff74f7cf };  // vacuum-fluorescent blue-green
const juce::Colour vfdDim     { 0xff1d3a33 };
const juce::Colour phosphor   { 0xff8dff9e };
const juce::Colour screen     { 0xff07140b };
const juce::Colour alu        { 0xffc7c9c4 };
const juce::Colour signal     { 0xffff4a2e };  // modulation red
} // namespace Colours

juce::Font legendFont (float h) { return juce::FontOptions (h, juce::Font::bold); }
juce::Font subFont (float h) { return juce::FontOptions (h); }
juce::Font monoFont (float h) { return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold); }

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dbd6), c.x - r, c.y - r,
                                             juce::Colour (0xff6c706b), c.x + r, c.y + r, false));
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c), 0.8f);
    const auto d = juce::Point<float> (std::cos (slotAngle), std::sin (slotAngle)) * (r * 0.8f);
    g.setColour (juce::Colour (0xff3a3d3a));
    g.drawLine ({ c - d, c + d }, 1.6f);
}

void drawReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool enabled)
{
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Colours::frame.darker());
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setFont (monoFont (11.0f));
    if (enabled)
    {
        g.setColour (Colours::vfd.withAlpha (0.25f));
        g.drawText (text, r.translated (0.0f, 0.5f), juce::Justification::centred);
    }
    g.setColour (enabled ? Colours::vfd : Colours::vfdDim);
    g.drawText (text, r, juce::Justification::centred);
}
} // namespace

//==============================================================================
SovietLookAndFeel::SovietLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Colours::vfd);
    setColour (juce::ComboBox::arrowColourId, Colours::vfd);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::black);
    setColour (juce::PopupMenu::textColourId, Colours::vfd);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::vfdDim);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::vfd.brighter (0.3f));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colours::black);
    setColour (juce::TooltipWindow::textColourId, Colours::vfd);
}

void SovietLookAndFeel::drawBakeliteKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle, bool enabled)
{
    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c.translated (1.5f, 3.0f)));

    // Fluted skirt
    const auto skirt = juce::Rectangle<float> (2 * r, 2 * r).withCentre (c);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), skirt.getX(), skirt.getY(),
                                             juce::Colour (0xff050505), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse (skirt);
    constexpr int flutes = 24;
    for (int i = 0; i < flutes; ++i)
    {
        const float a = (float) i / flutes * juce::MathConstants<float>::twoPi + angle;
        const auto p1 = c.getPointOnCircumference (r * 0.80f, a);
        const auto p2 = c.getPointOnCircumference (r * 0.98f, a);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // Cap
    const float capR = r * 0.72f;
    const auto cap = juce::Rectangle<float> (2 * capR, 2 * capR).withCentre (c);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e2e), cap.getX(), cap.getY(),
                                             Colours::bakelite, cap.getCentreX(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (cap.reduced (1.0f), 1.0f);

    // Specular highlight
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), cap.getX() + capR * 0.5f, cap.getY(),
                                             juce::Colours::transparentWhite, cap.getCentreX(), cap.getCentreY(), true));
    g.fillEllipse (cap.reduced (capR * 0.15f).translated (-capR * 0.15f, -capR * 0.2f));

    // Index line through cap and skirt
    g.setColour (enabled ? Colours::silk : Colours::silkDim.withAlpha (0.35f));
    g.drawLine ({ c.getPointOnCircumference (r * 0.25f, angle), c.getPointOnCircumference (r * 0.97f, angle) }, 2.2f);
}

void SovietLookAndFeel::drawChickenHead (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    juce::Path head;
    head.addRoundedRectangle (-r * 0.30f, -r * 1.0f, r * 0.60f, r * 2.0f, r * 0.28f);
    head.addEllipse (-r * 0.62f, -r * 0.62f, r * 1.24f, r * 1.24f);
    // Pointed end towards the selected position
    head.addTriangle (-r * 0.30f, -r * 0.85f, r * 0.30f, -r * 0.85f, 0.0f, -r * 1.22f);
    const auto xf = juce::AffineTransform::rotation (angle).translated (c);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (head, xf.translated (1.5f, 3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), c.x - r, c.y - r,
                                             juce::Colour (0xff050505), c.x + r, c.y + r, false));
    g.fillPath (head, xf);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.strokePath (head, juce::PathStrokeType (1.0f), xf);

    g.setColour (Colours::silk);
    g.drawLine ({ c.getPointOnCircumference (r * 0.1f, angle), c.getPointOnCircumference (r * 1.1f, angle) }, 2.0f);
}

void SovietLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (s.getProperties().contains ("selector"))
    {
        drawChickenHead (g, c, r * 0.8f, angle);
        return;
    }

    // Silkscreened scale on the panel
    const bool bipolar = s.getProperties().contains ("bipolar");
    g.setColour (Colours::silk);
    for (int i = 0; i <= 20; ++i)
    {
        const float a = startAngle + (float) i / 20.0f * (endAngle - startAngle);
        const bool major = i % 2 == 0;
        g.drawLine ({ c.getPointOnCircumference (r * (major ? 0.70f : 0.74f), a),
                      c.getPointOnCircumference (r * 0.80f, a) }, major ? 1.3f : 0.8f);
        if (major && r > 30.0f)
        {
            const int label = bipolar ? i / 2 - 5 : i / 2;
            const auto p = c.getPointOnCircumference (r * 0.92f, a);
            g.setFont (subFont (8.5f));
            g.drawText (juce::String (label), juce::Rectangle<float> (16.0f, 10.0f).withCentre (p),
                        juce::Justification::centred);
        }
    }

    // LFO modulation: range arc and live position dot
    const float modDepth = (float) s.getProperties().getWithDefault ("modDepth", 0.0f);
    if (modDepth > 0.0f && s.isEnabled())
    {
        const float modOffset = (float) s.getProperties().getWithDefault ("modOffset", 0.0f);
        const float lo = juce::jlimit (0.0f, 1.0f, pos - modDepth * 0.5f);
        const float hi = juce::jlimit (0.0f, 1.0f, pos + modDepth * 0.5f);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r * 0.75f, r * 0.75f, 0.0f,
                           startAngle + lo * (endAngle - startAngle), startAngle + hi * (endAngle - startAngle), true);
        g.setColour (Colours::signal.withAlpha (0.65f));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float live = juce::jlimit (0.0f, 1.0f, pos + modOffset);
        const auto dot = c.getPointOnCircumference (r * 0.75f, startAngle + live * (endAngle - startAngle));
        g.setColour (Colours::signal.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (dot));
        g.setColour (Colours::signal.brighter (0.4f));
        g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre (dot));
    }

    drawBakeliteKnob (g, c, r * 0.60f, angle, s.isEnabled());
}

void SovietLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const auto c = bounds.getCentre();
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const bool on = b.getToggleState();

    // Hex nut
    juce::Path nut;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = c.getPointOnCircumference (r * 0.55f, (float) i / 6.0f * juce::MathConstants<float>::twoPi);
        if (i == 0) nut.startNewSubPath (p); else nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e7e3), c.x - r, c.y - r,
                                             juce::Colour (0xff6d716c), c.x + r, c.y + r, false));
    g.fillPath (nut);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));
    g.setColour (juce::Colour (0xff2b2d2b));
    g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (c));

    // Bat lever, foreshortened: points up when on, down when off
    const float dir = on ? -1.0f : 1.0f;
    const auto tip = c.translated (0.0f, dir * r * 0.95f);
    juce::Path lever;
    lever.startNewSubPath (c.translated (-r * 0.12f, 0.0f));
    lever.lineTo (tip.translated (-r * 0.2f, 0.0f));
    lever.lineTo (tip.translated (r * 0.2f, 0.0f));
    lever.lineTo (c.translated (r * 0.12f, 0.0f));
    lever.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (lever, juce::AffineTransform::translation (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.x - r * 0.2f, tip.y,
                                             juce::Colour (0xff8a8e89), tip.x + r * 0.2f, tip.y, false));
    g.fillPath (lever);
    g.fillEllipse (juce::Rectangle<float> (r * 0.46f, r * 0.46f).withCentre (tip));
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (juce::Rectangle<float> (r * 0.46f, r * 0.46f).withCentre (tip), 0.8f);
}

void SovietLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat();
    g.setColour (Colours::frame.darker (0.6f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.reduced (2.0f), 2.0f);

    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void SovietLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font SovietLookAndFeel::getComboBoxFont (juce::ComboBox&) { return monoFont (12.0f); }
juce::Font SovietLookAndFeel::getPopupMenuFont() { return monoFont (13.0f); }

void SovietLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colours::black);
    g.setColour (Colours::vfdDim);
    g.drawRect (0, 0, w, h, 1);
}

//==============================================================================
LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String r, juce::String e)
    : ru (std::move (r)), en (std::move (e))
{
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void LabelledKnob::resized()
{
    auto r = getLocalBounds();
    if (ru.isNotEmpty())
        r.removeFromTop (26);
    r.removeFromBottom (20);
    const int side = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void LabelledKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (ru.isNotEmpty())
    {
        g.setColour (Colours::silk);
        g.setFont (legendFont (11.0f));
        g.drawText (ru, r.removeFromTop (14.0f), juce::Justification::centred);
        g.setColour (Colours::silkDim);
        g.setFont (subFont (9.0f));
        g.drawText (en.toUpperCase(), r.removeFromTop (11.0f), juce::Justification::centred);
    }

    const auto window = r.removeFromBottom (17.0f).withSizeKeepingCentre (juce::jmin (74.0f, r.getWidth() - 4.0f), 16.0f);
    drawReadout (g, window, slider.getTextFromValue (slider.getValue()), isEnabled() && slider.isEnabled());
}

//==============================================================================
Selector::Selector (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String r, juce::String e,
                    juce::StringArray positionsRu, juce::StringArray positionsEn)
    : ru (std::move (r)), en (std::move (e)), posRu (std::move (positionsRu)), posEn (std::move (positionsEn))
{
    slider.getProperties().set ("selector", true);
    slider.setMouseDragSensitivity (120);
    const int n = posRu.size();
    const float step = juce::degreesToRadians (n <= 4 ? 50.0f : 36.0f);
    const float span = step * (float) (n - 1);
    const float twoPi = juce::MathConstants<float>::twoPi;
    slider.setRotaryParameters (twoPi - span * 0.5f, twoPi + span * 0.5f, true);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    slider.onValueChange = [this] { repaint(); };
}

void Selector::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (26);
    r.removeFromBottom (20);
    const int side = juce::jmin ((int) (r.getWidth() * 0.46f), r.getHeight() - 30, 84);
    slider.setBounds (juce::Rectangle<int> (side, side).withCentre ({ r.getCentreX(), r.getBottom() - side / 2 - 2 }));
}

juce::Point<float> Selector::labelCentre (int index) const
{
    const auto params = slider.getRotaryParameters();
    const int n = posRu.size();
    const float a = params.startAngleRadians
                  + (float) index / (float) juce::jmax (1, n - 1) * (params.endAngleRadians - params.startAngleRadians);
    const auto c = slider.getBounds().toFloat().getCentre();
    const float radius = (float) slider.getWidth() * 0.5f + 11.0f;
    // Anchor the legend's near edge at the radius so it grows outwards, never over the knob.
    const float halfWidth = 0.5f * juce::GlyphArrangement::getStringWidth (legendFont (9.0f), posRu[index]);
    const auto p = c.getPointOnCircumference (radius, a);
    return p + juce::Point<float> (std::sin (a) * halfWidth, -std::cos (a) * 5.0f);
}

void Selector::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::silk);
    g.setFont (legendFont (11.0f));
    g.drawText (ru, r.removeFromTop (14.0f), juce::Justification::centred);
    g.setColour (Colours::silkDim);
    g.setFont (subFont (9.0f));
    g.drawText (en.toUpperCase(), r.removeFromTop (11.0f), juce::Justification::centred);

    const int current = juce::roundToInt (slider.getValue() - slider.getMinimum());
    const auto params = slider.getRotaryParameters();
    const auto centre = slider.getBounds().toFloat().getCentre();
    const int n = posRu.size();
    for (int i = 0; i < n; ++i)
    {
        const float a = params.startAngleRadians
                      + (float) i / (float) juce::jmax (1, n - 1) * (params.endAngleRadians - params.startAngleRadians);
        g.setColour (Colours::silk);
        g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f)
                           .withCentre (centre.getPointOnCircumference ((float) slider.getWidth() * 0.5f + 4.0f, a)));

        const bool selected = i == current;
        g.setColour (selected ? Colours::silk : Colours::silkDim.withAlpha (0.85f));
        g.setFont (legendFont (9.0f));
        g.drawText (posRu[i], juce::Rectangle<float> (60.0f, 12.0f).withCentre (labelCentre (i)), juce::Justification::centred);
    }

    const auto window = getLocalBounds().toFloat().removeFromBottom (17.0f).withSizeKeepingCentre (74.0f, 16.0f);
    drawReadout (g, window, posEn[current], true);
}

void Selector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < posRu.size(); ++i)
        if (juce::Rectangle<float> (56.0f, 16.0f).withCentre (labelCentre (i)).contains (e.position))
        {
            slider.setValue (slider.getMinimum() + i, juce::sendNotificationSync);
            return;
        }
}

//==============================================================================
LabelledToggle::LabelledToggle (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String r, juce::String e)
    : ru (std::move (r)), en (std::move (e))
{
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, button);
    button.onStateChange = [this] { repaint(); };
}

void LabelledToggle::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (40);
    button.setBounds (r.removeFromTop (34).withSizeKeepingCentre (34, 34));
}

void LabelledToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::silk);
    g.setFont (legendFont (11.0f));
    g.drawText (ru, r.removeFromTop (14.0f), juce::Justification::centred);
    g.setColour (Colours::silkDim);
    g.setFont (subFont (9.0f));
    g.drawText (en.toUpperCase(), r.removeFromTop (11.0f), juce::Justification::centred);

    const auto b = button.getBounds().toFloat();
    g.setFont (legendFont (9.0f));
    g.setColour (button.getToggleState() ? Colours::silk : Colours::silkDim);
    g.drawText (U ("ВКЛ"), b.withY (b.getY() - 15.0f).withHeight (13.0f).expanded (20.0f, 0.0f), juce::Justification::centred);
    g.setColour (button.getToggleState() ? Colours::silkDim : Colours::silk);
    g.drawText (U ("ВЫКЛ"), b.withY (b.getBottom() + 2.0f).withHeight (13.0f).expanded (20.0f, 0.0f), juce::Justification::centred);
}

//==============================================================================
void Lamp::setLevel (float newLevel)
{
    newLevel = juce::jlimit (0.0f, 1.0f, newLevel);
    if (std::abs (newLevel - level) > 0.01f)
    {
        level = newLevel;
        repaint();
    }
}

void Lamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto legend = r.removeFromBottom (13.0f);
    const float d = std::min (r.getWidth(), r.getHeight()) - 4.0f;
    const auto bezel = juce::Rectangle<float> (d, d).withCentre (r.getCentre());

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e9e5), bezel.getX(), bezel.getY(),
                                             juce::Colour (0xff5e625d), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);
    const auto jewel = bezel.reduced (d * 0.18f);
    const auto lit = colour.darker (2.2f).interpolatedWith (colour.brighter (0.4f), level);
    g.setGradientFill (juce::ColourGradient (lit.brighter (0.6f), jewel.getCentreX() - jewel.getWidth() * 0.2f, jewel.getY(),
                                             lit.darker (0.5f), jewel.getCentreX(), jewel.getBottom(), true));
    g.fillEllipse (jewel);
    if (level > 0.05f)
    {
        g.setColour (colour.withAlpha (0.25f * level));
        g.fillEllipse (jewel.expanded (d * 0.18f));
    }
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (jewel.reduced (jewel.getWidth() * 0.32f).translated (-jewel.getWidth() * 0.14f, -jewel.getHeight() * 0.16f));

    g.setColour (Colours::silk);
    g.setFont (legendFont (9.0f));
    g.drawText (text, legend, juce::Justification::centred);
}

//==============================================================================
void Scope::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3f3c), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff121412), bounds.getRight(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const auto tube = bounds.reduced (7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10261a), tube.getCentreX(), tube.getCentreY(),
                                             Colours::screen, tube.getX(), tube.getY(), true));
    g.fillRoundedRectangle (tube, 14.0f);

    // Graticule: 10 x 8 divisions with minor ticks on the centre lines
    const auto screen = tube.reduced (8.0f, 7.0f);
    g.setColour (Colours::phosphor.withAlpha (0.13f));
    for (int i = 0; i <= 10; ++i)
        g.drawVerticalLine ((int) (screen.getX() + screen.getWidth() * (float) i / 10.0f), screen.getY(), screen.getBottom());
    for (int i = 0; i <= 8; ++i)
        g.drawHorizontalLine ((int) (screen.getY() + screen.getHeight() * (float) i / 8.0f), screen.getX(), screen.getRight());
    g.setColour (Colours::phosphor.withAlpha (0.22f));
    for (int i = 0; i <= 50; ++i)
    {
        const float x = screen.getX() + screen.getWidth() * (float) i / 50.0f;
        g.drawVerticalLine ((int) x, screen.getCentreY() - 2.0f, screen.getCentreY() + 2.0f);
    }
    for (int i = 0; i <= 40; ++i)
    {
        const float y = screen.getY() + screen.getHeight() * (float) i / 40.0f;
        g.drawHorizontalLine ((int) y, screen.getCentreX() - 2.0f, screen.getCentreX() + 2.0f);
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tube.toNearestInt());
        drawTrace (g, screen);
    }

    // Glass reflection
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), tube.getX(), tube.getY(),
                                             juce::Colours::transparentWhite, tube.getX(), tube.getCentreY(), false));
    g.fillRoundedRectangle (tube.withHeight (tube.getHeight() * 0.5f), 14.0f);
}

void Scope::strokeTrace (juce::Graphics& g, const juce::Path& p)
{
    g.setColour (Colours::phosphor.withAlpha (0.12f));
    g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::phosphor.withAlpha (0.3f));
    g.strokePath (p, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::phosphor.brighter (0.5f));
    g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void TransferScope::setState (int type, float driveDb, float blend)
{
    if (type == satType && juce::approximatelyEqual (driveDb, drive) && juce::approximatelyEqual (blend, blendAmount))
        return;
    satType = type;
    drive = driveDb;
    blendAmount = blend;
    repaint();
}

void TransferScope::drawTrace (juce::Graphics& g, juce::Rectangle<float> screen)
{
    // Static X/Y transfer curve: the scope view ignores Crush's sample-and-hold.
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
    const int steps = (int) screen.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) steps;
        const float px = screen.getX() + (float) i;
        const float py = screen.getCentreY() - curveAt (x) / peak * screen.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    strokeTrace (g, p);
}

void LfoScope::setState (int newShape, float newPhase, float newValue)
{
    shape = newShape;
    phase = newPhase;
    value = newValue;
    repaint();
}

void LfoScope::drawTrace (juce::Graphics& g, juce::Rectangle<float> screen)
{
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
    const int steps = (int) screen.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float cyc = cyclesShown * (float) i / (float) steps;
        const float px = screen.getX() + (float) i;
        const float py = screen.getCentreY() - shapeAt (std::min (cyc, cyclesShown - 0.0001f)) * screen.getHeight() * 0.4f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    strokeTrace (g, p);

    // Beam spot at the live position
    const float x = screen.getX() + screen.getWidth() * phase / cyclesShown;
    const float y = screen.getCentreY() - value * screen.getHeight() * 0.4f;
    g.setColour (Colours::phosphor.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ x, y }));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x, y }));
}

//==============================================================================
void NeedleMeter::setLevel (float rms)
{
    // 0 VU = -14 dBFS RMS. One-pole approximation of 300 ms VU ballistics at the 30 Hz UI rate.
    const float db = juce::Decibels::gainToDecibels (rms, -60.0f) + 14.0f;
    const float before = needleDb;
    needleDb += 0.25f * (juce::jlimit (-30.0f, 6.0f, db) - needleDb);
    if (std::abs (needleDb - before) > 0.01f)
        repaint();
}

void NeedleMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (bounds, 4.0f);
    const auto face = bounds.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3ecd6), face.getCentreX(), face.getY(),
                                             juce::Colour (0xffd8ceb1), face.getCentreX(), face.getBottom(), false));
    g.fillRect (face);

    // Arc of +-48 degrees sized to the face width, pivot hidden below the face.
    const float radius = (face.getWidth() * 0.5f - 16.0f) / std::sin (juce::degreesToRadians (48.0f));
    const auto pivot = juce::Point<float> (face.getCentreX(), face.getY() + 24.0f + radius);
    static constexpr float minDb = -20.0f, maxDb = 3.0f;
    auto angleFor = [] (float db)
    {
        // VU-style non-linear spacing: position follows the linear voltage.
        const float v = std::pow (10.0f, db / 20.0f);
        const float lo = std::pow (10.0f, minDb / 20.0f), hi = std::pow (10.0f, maxDb / 20.0f);
        return juce::degreesToRadians (-48.0f + 96.0f * (v - lo) / (hi - lo));
    };

    juce::Path redZone;
    redZone.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (0.0f), angleFor (maxDb), true);
    g.setColour (juce::Colour (0xffc2271b));
    g.strokePath (redZone, juce::PathStrokeType (5.0f));
    juce::Path scale;
    scale.addCentredArc (pivot.x, pivot.y, radius - 3.0f, radius - 3.0f, 0.0f, angleFor (minDb), angleFor (maxDb), true);
    g.setColour (juce::Colours::black);
    g.strokePath (scale, juce::PathStrokeType (1.0f));

    const float marks[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    g.setFont (subFont (8.5f));
    for (float m : marks)
    {
        const float a = angleFor (m);
        g.setColour (m > 0.0f ? juce::Colour (0xffc2271b) : juce::Colours::black);
        g.drawLine ({ pivot.getPointOnCircumference (radius - 3.0f, a), pivot.getPointOnCircumference (radius + 4.0f, a) }, 1.0f);
        const auto label = juce::String (m > 0 ? "+" : "") + juce::String ((int) m);
        g.drawText (label, juce::Rectangle<float> (22.0f, 10.0f).withCentre (pivot.getPointOnCircumference (radius + 11.0f, a)),
                    juce::Justification::centred);
    }

    g.setColour (juce::Colours::black);
    g.setFont (legendFont (11.0f));
    g.drawText (U ("дБ"), face.withTrimmedTop (face.getHeight() * 0.5f).withTrimmedBottom (14.0f),
                juce::Justification::centredBottom);
    g.setFont (subFont (7.5f));
    g.drawText (U ("М4200"), face.reduced (4.0f, 3.0f), juce::Justification::bottomLeft);
    g.drawText (U ("ГОСТ 8711"), face.reduced (4.0f, 3.0f), juce::Justification::bottomRight);

    // Needle
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (face.toNearestInt());
        const float a = angleFor (juce::jlimit (minDb - 3.0f, maxDb + 1.5f, needleDb));
        const auto tip = pivot.getPointOnCircumference (radius + 6.0f, a);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine ({ pivot.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, 1.4f);
        g.setColour (juce::Colours::black);
        g.drawLine ({ pivot, tip }, 1.4f);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), face.getX(), face.getY(),
                                             juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
    g.fillRect (face.withHeight (face.getHeight() * 0.45f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRect (face, 1.0f);
}

//==============================================================================
PatinaEditor::PatinaEditor (PatinaProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      satType (p.apvts, ParamIDs::satType, U ("ХАРАКТЕР"), "Character",
               { U ("ЛЕНТА"), U ("ЛАМПА"), U ("ФУЗЗ"), U ("ЦИФРА") }, { "TAPE", "TUBE", "FUZZ", "CRUSH" }),
      satPos (p.apvts, ParamIDs::satPos, U ("ПОЛОЖЕНИЕ"), "Position",
              { U ("ДО"), U ("ПЕТЛЯ"), U ("ПОСЛЕ") }, { "PRE", "SPACE", "POST" }),
      drive (p.apvts, ParamIDs::drive, U ("УСИЛЕНИЕ"), "Drive"),
      satTone (p.apvts, ParamIDs::satTone, U ("ТЕМБР"), "Tone"),
      satBlend (p.apvts, ParamIDs::satBlend, U ("СМЕСЬ"), "Blend"),
      mode (p.apvts, ParamIDs::mode, U ("РЕЖИМ"), "Mode",
            { U ("ЛЕНТА"), U ("ПЗС"), U ("ПРУЖИНА"), U ("ПЛАСТИНА") }, { "TAPE", "BBD", "SPRING", "PLATE" }),
      time (p.apvts, ParamIDs::time, U ("ВРЕМЯ"), "Time"),
      feedback (p.apvts, ParamIDs::feedback, U ("ОБР. СВЯЗЬ"), "Feedback"),
      preDelay (p.apvts, ParamIDs::preDelay, U ("ПРЕДЗАДЕРЖКА"), "Pre-Delay"),
      decay (p.apvts, ParamIDs::decay, U ("ЗАТУХАНИЕ"), "Decay"),
      tone (p.apvts, ParamIDs::tone, U ("ТЕМБР"), "Tone"),
      age (p.apvts, ParamIDs::age, U ("ИЗНОС"), "Age"),
      width (p.apvts, ParamIDs::width, U ("ШИРИНА"), "Width"),
      division (p.apvts, ParamIDs::division, U ("ДЕЛЕНИЕ"), "Division"),
      sync (p.apvts, ParamIDs::sync, U ("СИНХР."), "Sync"),
      duck (p.apvts, ParamIDs::duck, U ("ПРИГЛУШ."), "Duck"),
      mix (p.apvts, ParamIDs::mix, U ("БАЛАНС"), "Mix"),
      output (p.apvts, ParamIDs::output, U ("ВЫХОД"), "Output"),
      lfoShape (p.apvts, ParamIDs::lfoShape, U ("ФОРМА"), "Shape",
                { U ("СИНУС"), U ("ТРЕУГ."), U ("МЕАНДР"), U ("ПИЛА"), U ("СЛУЧ."), U ("ДРЕЙФ") },
                { "SINE", "TRI", "SQUARE", "SAW", "S&H", "DRIFT" }),
      lfoRate (p.apvts, ParamIDs::lfoRate, U ("ЧАСТОТА"), "Rate"),
      lfoDiv (p.apvts, ParamIDs::lfoDiv, U ("ДЕЛЕНИЕ"), "Division"),
      lfoSync (p.apvts, ParamIDs::lfoSync, U ("СИНХР."), "Sync")
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> {
             &transferScope, &satType, &satPos, &drive, &satTone, &satBlend, &mode, &time, &feedback, &preDelay,
             &decay, &tone, &age, &width, &division, &sync, &meter, &duck, &mix, &output, &powerLamp,
             &overloadLamp, &lfoScope, &lfoLamp, &lfoShape, &lfoRate, &lfoDiv, &lfoSync })
        addAndMakeVisible (c);

    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        auto& box = lfoTarget[i];
        box.addItemList (PatinaProcessor::lfoTargetNames, 1);
        addAndMakeVisible (box);
        lfoTargetAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            p.apvts, ParamIDs::lfoTarget[i], box);
        lfoDepth[i] = std::make_unique<LabelledKnob> (p.apvts, ParamIDs::lfoDepth[i], juce::String(), juce::String());
        lfoDepth[i]->slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (*lfoDepth[i]);
    }

    targetKnobs = { nullptr, &time, &feedback, &preDelay, &decay, &tone, &age, &width, &mix, &drive, &satTone, &satBlend };

    powerLamp.setLevel (1.0f);
    age.slider.setTooltip ("Wow, flutter, wobble, hiss and darkening - everything a worn machine adds.");
    satPos.slider.setTooltip ("Pre: drive the input. Space: drive the echo loop / reverb tank. Post: drive the final mix.");

    // Hammertone enamel texture, generated once.
    panelTexture = juce::Image (juce::Image::ARGB, 256, 256, true);
    {
        juce::Graphics tg (panelTexture);
        juce::Random rng (1979);
        for (int i = 0; i < 2600; ++i)
        {
            const float x = rng.nextFloat() * 256.0f, y = rng.nextFloat() * 256.0f;
            const float sz = 1.0f + rng.nextFloat() * 3.5f;
            tg.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f + rng.nextFloat() * 0.03f));
            tg.fillEllipse (x, y, sz, sz);
        }
    }

    setSize (1060, 720);
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

    transferScope.setState ((int) raw (ParamIDs::satType), raw (ParamIDs::drive), raw (ParamIDs::satBlend));

    const float lfoValue = processor.lfoValueUI.load();
    lfoScope.setState ((int) raw (ParamIDs::lfoShape), processor.lfoPhaseUI.load(), lfoValue);
    lfoLamp.setLevel (0.5f + 0.5f * lfoValue);

    meter.setLevel (processor.outputLevelUI.load());
    const float peak = processor.outputPeakUI.exchange (0.0f);
    overloadHold = peak >= 0.98f ? 1.0f : overloadHold * 0.88f;
    overloadLamp.setLevel (overloadHold);

    lfoDiv.setEnabled (raw (ParamIDs::lfoSync) > 0.5f);
    lfoRate.setEnabled (raw (ParamIDs::lfoSync) < 0.5f);

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
        auto* knob = targetKnobs[(size_t) t];
        auto& props = knob->slider.getProperties();
        const float d = depth[(size_t) t];
        const float wasDepth = (float) props.getWithDefault ("modDepth", 0.0f);
        if (d > 0.0f || wasDepth > 0.0f)
        {
            props.set ("modDepth", d);
            props.set ("modOffset", processor.modOffsetUI[(size_t) t].load());
            knob->slider.repaint();
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
        "Tape loop echo. Motor glide on time changes, wow, flutter and drift, tape compression and hiss.",
        "Bucket-brigade delay. Bandwidth narrows as the clock slows; chorus wobble and compander noise.",
        "Spring tank. Dispersive springs give the drip; Age drives the input transformer into boing.",
        "Steel plate. Dense figure-of-eight tank, no discrete echoes; Age tires the damping pads."
    };
    modeText = descriptions[juce::jlimit (0, 3, currentMode)];
    repaint (modePlate);
}

void PatinaEditor::resized()
{
    constexpr int margin = 14;
    auto area = getLocalBounds().reduced (margin);

    nameplate = area.removeFromTop (52);
    area.removeFromTop (10);

    auto top = area.removeFromTop (404);
    area.removeFromTop (10);
    lfoPanel = area;

    satPanel = top.removeFromLeft (300);
    top.removeFromLeft (10);
    outPanel = top.removeFromRight (250);
    top.removeFromRight (10);
    spacePanel = top;

    // --- Saturation
    {
        auto r = satPanel.reduced (14);
        r.removeFromTop (20);
        transferScope.setBounds (r.removeFromTop (96));
        r.removeFromTop (6);
        auto sel = r.removeFromTop (124);
        satType.setBounds (sel.removeFromLeft (sel.getWidth() / 2));
        satPos.setBounds (sel);
        const int w = r.getWidth() / 3;
        drive.setBounds (r.removeFromLeft (w));
        satTone.setBounds (r.removeFromLeft (w));
        satBlend.setBounds (r);
    }

    // --- Space
    {
        auto r = spacePanel.reduced (14);
        r.removeFromTop (20);
        auto left = r.removeFromLeft (196);
        mode.setBounds (left.removeFromTop (150));
        left.removeFromTop (6);
        modePlate = left.removeFromTop (78).reduced (2, 0);
        left.removeFromTop (8);
        sync.setBounds (left.removeFromTop (92).withSizeKeepingCentre (90, 92));

        r.removeFromLeft (6);
        const int w = r.getWidth() / 3;
        auto row1 = r.removeFromTop (r.getHeight() / 2);
        auto row2 = r;
        auto slot = [&] (juce::Rectangle<int>& row) { return row.removeFromLeft (w); };
        auto s1 = slot (row1);
        time.setBounds (s1);
        preDelay.setBounds (s1);
        auto s2 = slot (row1);
        feedback.setBounds (s2);
        decay.setBounds (s2);
        tone.setBounds (row1);
        age.setBounds (slot (row2));
        width.setBounds (slot (row2));
        division.setBounds (row2);
    }

    // --- Output
    {
        auto r = outPanel.reduced (14);
        r.removeFromTop (20);
        meter.setBounds (r.removeFromTop (118));
        r.removeFromTop (8);
        auto lamps = r.removeFromBottom (44);
        powerLamp.setBounds (lamps.removeFromLeft (lamps.getWidth() / 2).withSizeKeepingCentre (60, 44));
        overloadLamp.setBounds (lamps.withSizeKeepingCentre (60, 44));
        const int w = r.getWidth() / 3;
        duck.setBounds (r.removeFromLeft (w));
        mix.setBounds (r.removeFromLeft (w));
        output.setBounds (r);
    }

    // --- LFO
    {
        auto r = lfoPanel.reduced (14);
        r.removeFromTop (20);
        auto scopeArea = r.removeFromLeft (176);
        lfoScope.setBounds (scopeArea.removeFromTop (scopeArea.getHeight() - 6).withTrimmedRight (40).reduced (0, 4));
        lfoLamp.setBounds (lfoScope.getRight() + 4, lfoScope.getY() + 20, 36, 44);
        r.removeFromLeft (8);
        lfoShape.setBounds (r.removeFromLeft (190));
        lfoRate.setBounds (r.removeFromLeft (92));
        lfoSync.setBounds (r.removeFromLeft (76));
        lfoDiv.setBounds (r.removeFromLeft (92));
        r.removeFromLeft (10);
        const int w = r.getWidth() / ParamIDs::numLfoSlots;
        for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
        {
            auto slot = r.removeFromLeft (w).reduced (5, 0);
            slotArea[i] = slot;
            slot.removeFromTop (26);
            lfoTarget[i].setBounds (slot.removeFromTop (24));
            slot.removeFromTop (2);
            lfoDepth[i]->setBounds (slot);
        }
    }
}

void PatinaEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& ru, const juce::String& en)
{
    const auto rf = r.toFloat();
    g.setColour (Colours::enamelDark.withAlpha (0.45f));
    g.fillRoundedRectangle (rf, 4.0f);
    g.setColour (Colours::silk.withAlpha (0.85f));

    // Silkscreened frame with the title breaking the top rule
    const juce::String title = ru + "  " + en.toUpperCase();
    const auto font = legendFont (12.0f);
    g.setFont (font);
    const float tw = juce::GlyphArrangement::getStringWidth (font, title) + 16.0f;
    const float top = rf.getY() + 8.0f;
    const float x0 = rf.getX() + 4.0f, x1 = rf.getRight() - 4.0f, bottom = rf.getBottom() - 4.0f;
    const float tx = x0 + 14.0f;
    juce::Path frame;
    frame.startNewSubPath (tx, top);
    frame.lineTo (x0, top);
    frame.lineTo (x0, bottom);
    frame.lineTo (x1, bottom);
    frame.lineTo (x1, top);
    frame.lineTo (tx + tw, top);
    g.strokePath (frame, juce::PathStrokeType (1.2f));
    g.drawText (title, juce::Rectangle<float> (tx, top - 8.0f, tw, 16.0f), juce::Justification::centred);
}

void PatinaEditor::paint (juce::Graphics& g)
{
    // Enamel panel
    g.fillAll (Colours::enamel);
    g.setTiledImageFill (panelTexture, 0, 0, 1.0f);
    g.fillAll();
    g.setColour (Colours::frame);
    g.drawRect (getLocalBounds(), 4);

    // Corner screws
    const float inset = 7.0f;
    drawScrew (g, { inset, inset }, 4.5f, 0.6f);
    drawScrew (g, { (float) getWidth() - inset, inset }, 4.5f, 2.1f);
    drawScrew (g, { inset, (float) getHeight() - inset }, 4.5f, 1.2f);
    drawScrew (g, { (float) getWidth() - inset, (float) getHeight() - inset }, 4.5f, 0.2f);

    // Black anodised nameplate
    {
        const auto r = nameplate.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262826), r.getX(), r.getY(),
                                                 juce::Colour (0xff0d0e0d), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);

        auto inner = r.reduced (16.0f, 4.0f);
        g.setColour (Colours::alu);
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
        g.drawText (U ("ПАТИНА"), inner.removeFromLeft (170.0f), juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        g.setColour (Colours::signal);
        g.drawText (U ("ЭХ-1"), inner.removeFromLeft (80.0f), juce::Justification::centredLeft);

        auto right = inner.removeFromRight (190.0f);
        g.setColour (Colours::alu.withAlpha (0.8f));
        g.setFont (subFont (10.0f));
        g.drawText (U ("ЗАВ. № 000117"), right.removeFromTop (right.getHeight() * 0.5f), juce::Justification::centredRight);
        g.drawText (U ("ИЗГОТОВЛЕНО 1979 г."), right, juce::Justification::centredRight);

        g.setColour (Colours::alu);
        g.setFont (legendFont (11.5f));
        g.drawText (U ("ГЕНЕРАТОР ЭХА  ·  РЕВЕРБЕРАТОР  ·  БЛОК ИСКАЖЕНИЙ"),
                    inner.removeFromTop (inner.getHeight() * 0.55f), juce::Justification::bottomLeft);
        g.setColour (Colours::alu.withAlpha (0.6f));
        g.setFont (subFont (9.5f));
        g.drawText ("TAPE / BBD ECHO  -  SPRING / PLATE REVERB  -  SATURATION  -  LFO MODULATOR",
                    inner, juce::Justification::topLeft);

        drawScrew (g, { r.getX() + 7.0f, r.getCentreY() }, 3.5f, 0.3f);
        drawScrew (g, { r.getRight() - 7.0f, r.getCentreY() }, 3.5f, 1.9f);
    }

    drawSection (g, satPanel, U ("НАСЫЩЕНИЕ"), "Saturation");
    drawSection (g, spacePanel, U ("ПРОСТРАНСТВО"), "Space");
    drawSection (g, outPanel, U ("ВЫХОД"), "Output");
    drawSection (g, lfoPanel, U ("НЧ ГЕНЕРАТОР"), "LFO Modulator");

    // Riveted aluminium instruction plate for the current mode
    {
        const auto r = modePlate.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dbd6), r.getX(), r.getY(),
                                                 juce::Colour (0xffa9aca6), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (r, 2.0f, 0.8f);
        for (auto pt : { r.getTopLeft().translated (5, 5), r.getTopRight().translated (-5, 5),
                         r.getBottomLeft().translated (5, -5), r.getBottomRight().translated (-5, -5) })
        {
            g.setColour (juce::Colour (0xff8c8f8a));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (pt));
        }
        g.setColour (juce::Colour (0xff1b1d1b));
        g.setFont (subFont (10.5f));
        g.drawFittedText (modeText, r.reduced (10.0f, 9.0f).toNearestInt(), juce::Justification::topLeft, 6, 1.0f);
    }

    // Slot legends for the LFO assignments
    for (int i = 0; i < ParamIDs::numLfoSlots; ++i)
    {
        auto r = slotArea[i].toFloat();
        g.setColour (Colours::silk);
        g.setFont (legendFont (11.0f));
        g.drawText (U ("ЦЕЛЬ ") + juce::String (i + 1) + U (" · ГЛУБИНА"), r.removeFromTop (14.0f), juce::Justification::centred);
        g.setColour (Colours::silkDim);
        g.setFont (subFont (9.0f));
        g.drawText ("TARGET " + juce::String (i + 1) + " / DEPTH", r.removeFromTop (11.0f), juce::Justification::centred);
    }
}
