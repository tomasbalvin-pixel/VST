#pragma once

#include "PluginProcessor.h"

class PatinaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PatinaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

/** Draws the current saturation transfer curve, like a scope in X/Y mode. */
class TransferCurve final : public juce::Component
{
public:
    void setState (int type, float driveDb, float blend);
    void paint (juce::Graphics&) override;

private:
    int satType = -1;
    float drive = -1.0f, blendAmount = -1.0f;
};

class PatinaEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PatinaEditor (PatinaProcessor&);
    ~PatinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramID, const juce::String& name);
    void setupCombo (juce::ComboBox&, juce::Label&, const char* paramID, const juce::String& name,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);
    void layoutKnob (Knob&, juce::Rectangle<int>);
    void timerCallback() override;
    void refreshVisibility();

    PatinaProcessor& processor;
    PatinaLookAndFeel lnf;

    Knob drive, satTone, satBlend;
    Knob time, feedback, preDelay, decay, tone, age, width;
    Knob duck, mix, output;

    juce::ComboBox satType, satPos, division;
    juce::Label satTypeLabel, satPosLabel, divisionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satTypeAtt, satPosAtt, divisionAtt;

    juce::ToggleButton sync { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;

    juce::TextButton modeButtons[4];
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    int currentMode = -1;
    bool lastSync = false;

    TransferCurve curve;
    juce::Label modeInfo;

    juce::Rectangle<int> satPanel, spacePanel, outPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatinaEditor)
};
