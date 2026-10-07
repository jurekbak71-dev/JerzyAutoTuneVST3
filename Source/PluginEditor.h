#pragma once

#include "PluginProcessor.h"

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

    class AnalogLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        AnalogLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
        juce::Font getLabelFont(juce::Label&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Font getTextButtonFont(juce::TextButton&, int) override;
    };

    class Surface final : public juce::Component
    {
    public:
        explicit Surface(JerzyAutoTuneAudioProcessorEditor& e) : owner(e) {}
        void paint(juce::Graphics& g) override { owner.paintSurface(g); }
    private:
        JerzyAutoTuneAudioProcessorEditor& owner;
    };

    void paintSurface(juce::Graphics&);
    void timerCallback() override;
    void configureSlider(juce::Slider&, const juce::String&, const juce::String&);
    void configureLabel(juce::Label&, const juce::String&);
    void layoutControls();
    void setAllNotes(bool);
    void updateControlStates();

    JerzyAutoTuneAudioProcessor& processor;
    // Declared before components; it must outlive their destruction.
    AnalogLookAndFeel analogLookAndFeel;
    Surface surface { *this };
    juce::TooltipWindow tooltips { this, 650 };
    juce::ComboBox keyBox, scaleBox, zoomBox;
    std::array<juce::TextButton, 12> noteButtons;
    juce::TextButton allNotes { "ALL" }, noNotes { "NONE" };
    juce::Slider speedSlider, amountSlider, mixSlider;
    juce::Label keyLabel, scaleLabel, speedLabel, amountLabel, mixLabel, noteStatus;
    static constexpr size_t vocalControlCount = 36;
    std::array<juce::Slider, vocalControlCount> vocalSliders;
    std::array<juce::Label, vocalControlCount> vocalLabels;
    std::array<juce::TextButton, 7> moduleButtons;

    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment;
    std::array<std::unique_ptr<ButtonAttachment>, 12> noteAttachments;
    std::unique_ptr<SliderAttachment> speedAttachment, amountAttachment, mixAttachment;
    std::array<std::unique_ptr<SliderAttachment>, vocalControlCount> vocalSliderAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, 7> moduleButtonAttachments;
    float displayedInput = -60.0f, displayedOutput = -60.0f, displayedReduction = 0.0f;
    int inputClipTicks = 0, outputClipTicks = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAutoTuneAudioProcessorEditor)
};
