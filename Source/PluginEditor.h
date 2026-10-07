#pragma once

#include "PluginProcessor.h"
#include "JerzyVSTGuiKit.h"

class JerzyAutoTuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit JerzyAutoTuneAudioProcessorEditor(JerzyAutoTuneAudioProcessor&);
    ~JerzyAutoTuneAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class AnalogLookAndFeel final : public JerzyAudioUI::HardwareLookAndFeel
    {
    public:
        AnalogLookAndFeel() : JerzyAudioUI::HardwareLookAndFeel(JerzyAudioUI::steel()) {}
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
    };

    void timerCallback() override;
    void configureSlider(juce::Slider&, const juce::String&);
    void configureLabel(juce::Label&, const juce::String&);
    juce::Rectangle<int> scaledBounds(float, float, float, float) const;
    void layoutModule(int moduleIndex, int start, int count, float x, float y, float w, float h);
    void drawModule(juce::Graphics&, int moduleIndex, const juce::String&, float, float, float, float);

    JerzyAutoTuneAudioProcessor& processor;
    AnalogLookAndFeel analogLookAndFeel;

    juce::ComboBox keyBox, scaleBox;
    std::array<juce::TextButton, 12> noteButtons;
    juce::Slider speedSlider, amountSlider, mixSlider;
    juce::Label keyLabel, scaleLabel, speedLabel, amountLabel, mixLabel;

    static constexpr size_t vocalControlCount = 36;
    std::array<juce::Slider, vocalControlCount> vocalSliders;
    std::array<juce::Label, vocalControlCount> vocalLabels;
    std::array<juce::ToggleButton, 7> moduleButtons;

    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment;
    std::array<std::unique_ptr<ButtonAttachment>, 12> noteAttachments;
    std::unique_ptr<SliderAttachment> speedAttachment, amountAttachment, mixAttachment;
    std::array<std::unique_ptr<SliderAttachment>, vocalControlCount> vocalSliderAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, 7> moduleButtonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAutoTuneAudioProcessorEditor)
};
