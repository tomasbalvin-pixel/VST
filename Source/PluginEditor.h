#pragma once

#include "PluginProcessor.h"

/**
    Look and feel modelled on Soviet laboratory test equipment (oscilloscopes,
    signal generators, level meters of the 1970s): hammertone enamel panels,
    black bakelite knobs with fluted skirts, chicken-head range selectors,
    bat-lever toggles, VFD read-outs and white silkscreened legends.
*/
class SovietLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SovietLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

private:
    void drawBakeliteKnob (juce::Graphics&, juce::Point<float> c, float r, float angle, bool enabled);
    void drawChickenHead (juce::Graphics&, juce::Point<float> c, float r, float angle);
};

//==============================================================================
/** Knob with a Cyrillic legend, English sub-legend and a VFD read-out window. */
class LabelledKnob final : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState&, const char* paramID, juce::String ru, juce::String en);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::String ru, en;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Detented rotary range switch with its positions engraved around it. Clicking a legend selects it. */
class Selector final : public juce::Component
{
public:
    Selector (juce::AudioProcessorValueTreeState&, const char* paramID, juce::String ru, juce::String en,
              juce::StringArray positionsRu, juce::StringArray positionsEn);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::Point<float> labelCentre (int index) const;

    juce::String ru, en;
    juce::StringArray posRu, posEn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Bat-lever toggle switch with ВКЛ / ВЫКЛ legends. */
class LabelledToggle final : public juce::Component
{
public:
    LabelledToggle (juce::AudioProcessorValueTreeState&, const char* paramID, juce::String ru, juce::String en);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ToggleButton button;

private:
    juce::String ru, en;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** Jewel indicator lamp. */
class Lamp final : public juce::Component
{
public:
    Lamp (juce::Colour c, juce::String legend) : colour (c), text (std::move (legend)) {}
    void setLevel (float newLevel);
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    juce::String text;
    float level = 0.0f;
};

/** Green-phosphor CRT. Subclasses draw the trace. */
class Scope : public juce::Component
{
public:
    void paint (juce::Graphics&) override;

protected:
    virtual void drawTrace (juce::Graphics&, juce::Rectangle<float> screen) = 0;
    static void strokeTrace (juce::Graphics&, const juce::Path&);
};

class TransferScope final : public Scope
{
public:
    void setState (int type, float driveDb, float blend);

private:
    void drawTrace (juce::Graphics&, juce::Rectangle<float>) override;
    int satType = -1;
    float drive = -1.0f, blendAmount = -1.0f;
};

class LfoScope final : public Scope
{
public:
    void setState (int shape, float phase, float value);

private:
    void drawTrace (juce::Graphics&, juce::Rectangle<float>) override;
    int shape = 0;
    float phase = 0.0f, value = 0.0f;
};

/** Moving-coil level meter with VU ballistics. */
class NeedleMeter final : public juce::Component
{
public:
    void setLevel (float rms);
    void paint (juce::Graphics&) override;

private:
    float needleDb = -30.0f;
};

//==============================================================================
class PatinaEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PatinaEditor (PatinaProcessor&);
    ~PatinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshVisibility();
    void updateModulationDisplay();
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& ru, const juce::String& en);

    PatinaProcessor& processor;
    SovietLookAndFeel lnf;
    juce::Image panelTexture;

    // Saturation
    TransferScope transferScope;
    Selector satType, satPos;
    LabelledKnob drive, satTone, satBlend;

    // Space
    Selector mode;
    LabelledKnob time, feedback, preDelay, decay, tone, age, width, division;
    LabelledToggle sync;

    // Output
    NeedleMeter meter;
    LabelledKnob duck, mix, output;
    Lamp powerLamp { juce::Colour (0xff59ff7a), juce::String::fromUTF8 ("СЕТЬ") };
    Lamp overloadLamp { juce::Colour (0xffff3b2a), juce::String::fromUTF8 ("ПЕРЕГР.") };

    // LFO
    LfoScope lfoScope;
    Lamp lfoLamp { juce::Colour (0xffff5a2a), juce::String::fromUTF8 ("НЧГ") };
    Selector lfoShape;
    LabelledKnob lfoRate, lfoDiv;
    LabelledToggle lfoSync;
    juce::ComboBox lfoTarget[ParamIDs::numLfoSlots];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoTargetAtt[ParamIDs::numLfoSlots];
    std::unique_ptr<LabelledKnob> lfoDepth[ParamIDs::numLfoSlots];

    std::array<LabelledKnob*, kNumModTargets> targetKnobs {};
    juce::String modeText;
    int currentMode = -1;
    bool lastSync = false;
    float overloadHold = 0.0f;

    juce::Rectangle<int> nameplate, satPanel, spacePanel, outPanel, lfoPanel, modePlate, slotArea[ParamIDs::numLfoSlots];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaEditor)
};
