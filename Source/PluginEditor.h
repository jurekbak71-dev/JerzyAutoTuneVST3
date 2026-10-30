#pragma once
#include "PluginProcessor.h"

class JerzyAutoTuneAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JerzyAutoTuneAudioProcessorEditor(JerzyAutoTuneAudioProcessor&);
    ~JerzyAutoTuneAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void showPage(int index, bool allControls = false);
private:
    struct StudioLookAndFeel final : juce::LookAndFeel_V4
    {
        StudioLookAndFeel();
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
        void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
        void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
        juce::Font getTextButtonFont(juce::TextButton&,int) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Font getLabelFont(juce::Label&) override;
    };
    struct ParameterControl;
    struct Surface final : juce::Component
    {
        explicit Surface(JerzyAutoTuneAudioProcessorEditor& e) : owner(e) {}
        void paint(juce::Graphics& g) override { owner.paintSurface(g); }
        void mouseDown(const juce::MouseEvent& e) override { owner.chartMouseDown(e); }
        void mouseDrag(const juce::MouseEvent& e) override { owner.chartMouseDrag(e); }
        void mouseUp(const juce::MouseEvent& e) override { owner.chartMouseUp(e); }
        void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
        JerzyAutoTuneAudioProcessorEditor& owner;
    };
    void paintSurface(juce::Graphics&);
    void drawSpectrum(juce::Graphics&,juce::Rectangle<float>,bool showOutput);
    void timerCallback() override;
    void layoutControls();
    void chartMouseDown(const juce::MouseEvent&);
    void chartMouseDrag(const juce::MouseEvent&);
    void chartMouseUp(const juce::MouseEvent&);
    juce::Rectangle<float> plotBounds() const;
    ParameterControl* control(const juce::String&) const;
    void setReal(const char*,float);
    JerzyAutoTuneAudioProcessor& processor;
    StudioLookAndFeel look;
    Surface surface { *this };
    juce::TooltipWindow tips { this,650 };
    juce::Image steel;
    std::array<juce::TextButton,7> navigation;
    juce::ComboBox presets,zoom;
    juce::TextButton compare { "A / B" },undo { "UNDO" },advancedButton { "ALL CONTROLS" },learnNoise { "LEARN NOISE" },learnKey { "ANALYSE KEY" },applyKey { "APPLY KEY" },resetPeak { "PEAK RESET" },allNotes { "ALL NOTES" },noNotes { "CLEAR NOTES" };
    juce::OwnedArray<ParameterControl> parameters,master;
    juce::ValueTree compareState;
    juce::dsp::FFT fft {11};
    std::array<float,2048> input {},output {};
    std::array<float,1025> spectrum {},processedSpectrum {};
    std::array<float,260> pitchHistory {},targetHistory {},reductionHistory {},stageBHistory {};
    size_t historyWrite=0;
    float inputDb=-60,outputDb=-60;
    int page=0,dragBand=-1;
    bool expanded=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyAutoTuneAudioProcessorEditor)
};
