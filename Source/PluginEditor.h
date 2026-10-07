#pragma once

#include "PluginProcessor.h"

/**
    Minimal look: flat warm off-white, near-black type, one accent colour.
    Knobs are thin arcs, choices are plain words, sections are separated by
    whitespace and hairlines only.
*/
class MinimalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MinimalLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

//==============================================================================
/** Arc knob with a lower-case label above and its value below. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const char* paramID, juce::String label);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A choice parameter shown as a row of words; the selected one is dark and underlined. */
class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const char* paramID, juce::StringArray words);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> wordBounds (int index) const;
    int indexAt (juce::Point<float>) const;

    juce::StringArray words;
    int selected = 0, hovered = -1;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Small on/off switch: a dot and a word. */
class Toggle final : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const char* paramID, juce::String label);
    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class CurvePlot final : public juce::Component
{
public:
    void setState (int type, float driveDb, float blend);
    void paint (juce::Graphics&) override;

private:
    int satType = -1;
    float drive = -1.0f, blendAmount = -1.0f;
};

class LfoPlot final : public juce::Component
{
public:
    void setState (int shape, float phase, float value);
    void paint (juce::Graphics&) override;

private:
    int shape = 0;
    float phase = 0.0f, value = 0.0f;
};

/** Output level as a single hairline bar; turns accent on overs. */
class LevelBar final : public juce::Component
{
public:
    void setLevel (float rms, float peak);
    void paint (juce::Graphics&) override;

private:
    float levelDb = -60.0f, hold = 0.0f;
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

    PatinaProcessor& processor;
    MinimalLookAndFeel lnf;

    // Saturation
    Choice satType, satPos;
    CurvePlot curve;
    Knob drive, satTone, satBlend;

    // Space
    Choice mode;
    Knob time, feedback, preDelay, decay, tone, age, width, division;
    Toggle sync;

    // Output
    LevelBar level;
    Knob duck, mix, output;

    // LFO
    Choice lfoShape;
    LfoPlot lfoPlot;
    Knob lfoRate, lfoDiv;
    Toggle lfoSync;
    juce::ComboBox lfoTarget[ParamIDs::numLfoSlots];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoTargetAtt[ParamIDs::numLfoSlots];
    std::unique_ptr<Knob> lfoDepth[ParamIDs::numLfoSlots];

    std::array<Knob*, kNumModTargets> targetKnobs {};
    juce::String modeText;
    int currentMode = -1;
    bool lastSync = false;

    juce::Rectangle<int> header, satArea, spaceArea, outArea, lfoArea, modeTextArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaEditor)
};
