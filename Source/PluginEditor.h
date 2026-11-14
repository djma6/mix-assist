#pragma once
#include "PluginProcessor.h"

class MixAssistantEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MixAssistantEditor (MixAssistantProcessor&);
    ~MixAssistantEditor() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider s; juce::Label l; std::unique_ptr<SA> a; const char* en; const char* fa; };

    void timerCallback() override;
    void applyLanguage();
    juce::String T (const char* en, const char* fa) const { return isFa ? juce::String::fromUTF8 (fa) : juce::String (en); }
    juce::String buildGuidance();
    Knob& addKnob (const char* id, const char* en, const char* fa, juce::Slider::SliderStyle st = juce::Slider::RotaryHorizontalVerticalDrag);

    MixAssistantProcessor& proc;
    bool isFa = false;

    juce::ComboBox modeBox, instBox, targetBox, langBox;
    std::unique_ptr<CA> modeAtt, instAtt, targetAtt, langAtt;
    std::array<juce::Slider, mx::kBands> eq;
    std::array<juce::Label, mx::kBands> eqLbl;
    std::array<std::unique_ptr<SA>, mx::kBands> eqAtt;

    std::vector<std::unique_ptr<Knob>> knobs;   // ترتیب: hpf thr ratio atk rel makeup | width pan bassMono | limGain limCeil limRel | out
    juce::ToggleButton eqOn, compOn, widthOn, limOn;
    std::unique_ptr<BA> eqOnA, compOnA, widthOnA, limOnA;

    juce::TextButton applyBtn, resetLufs;
    juce::Label meters, tuner;
    juce::TextEditor tips;
    juce::String lastTips;
    juce::Rectangle<int> specArea, scopeArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixAssistantEditor)
};
