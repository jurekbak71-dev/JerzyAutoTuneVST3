#include "PluginEditor.h"

namespace
{
const juce::Colour outer(13, 20, 25);
const juce::Colour metal(134, 151, 160);
const juce::Colour face(57, 74, 88);
const juce::Colour faceDark(32, 44, 53);
const juce::Colour panel(32, 44, 53);
const juce::Colour ink(216, 229, 233);
const juce::Colour cream(216, 229, 233);
const juce::Colour brass(115, 200, 232);
const juce::Colour red(202, 84, 76);

const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const std::array<const char*, 12> noteParameterIds {
    "noteC", "noteCs", "noteD", "noteDs", "noteE", "noteF",
    "noteFs", "noteG", "noteGs", "noteA", "noteAs", "noteB"
};

const std::array<const char*, 36> controlIds {
    "gateThreshold", "gateRange", "gateAttack", "gateHold", "gateRelease",
    "noiseThreshold", "noiseReduction", "noiseRelease", "noiseHighPass",
    "deEsserThreshold", "deEsserFreq", "deEsserAmount", "deEsserRelease",
    "satDrive", "satTone", "satMix", "satOutput",
    "doublerAmount", "doublerDelay", "doublerDetune", "doublerWidth",
    "compThreshold", "compRatio", "compAttack", "compRelease", "compKnee", "compMakeup", "limiterCeiling",
    "eqLowCut", "eqLow", "eqMidFreq", "eqMid", "eqMidQ", "eqHigh", "eqHighCut", "outputGain"
};

const std::array<const char*, 36> controlLabels {
    "THRESH", "RANGE", "ATTACK", "HOLD", "RELEASE",
    "THRESH", "REDUCE", "RELEASE", "HIGH-PASS",
    "THRESH", "FREQ", "RANGE", "RELEASE",
    "DRIVE", "TONE", "MIX", "OUTPUT",
    "MIX", "DELAY", "DETUNE", "WIDTH",
    "THRESH", "RATIO", "ATTACK", "RELEASE", "KNEE", "MAKEUP", "LIMIT",
    "LOW CUT", "BODY", "MID FREQ", "PRESENCE", "MID Q", "AIR", "HIGH CUT", "OUTPUT"
};

const std::array<const char*, 36> suffixes {
    " dB", " dB", " ms", " ms", " ms",
    " dB", " dB", " ms", " Hz",
    " dB", " Hz", " dB", " ms",
    " dB", " %", " %", " dB",
    " %", " ms", " ct", " %",
    " dB", ":1", " ms", " ms", " dB", " dB", " dB",
    " Hz", " dB", " Hz", " dB", "", " dB", " Hz", " dB"
};

const std::array<const char*, 7> moduleIds {
    "gateEnabled", "noiseEnabled", "deEsserEnabled", "satEnabled",
    "doublerEnabled", "compEnabled", "eqEnabled"
};

void addComboItems(juce::ComboBox& box, const juce::StringArray& items)
{
    for (int i = 0; i < items.size(); ++i) box.addItem(items[i], i + 1);
}
}

JerzyAutoTuneAudioProcessorEditor::JerzyAutoTuneAudioProcessorEditor(JerzyAutoTuneAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(1100, 770, 2100, 1470);
    setSize(1500, 1050);

    addComboItems(keyBox, noteNames);
    addComboItems(scaleBox, { "Chromatic", "Major", "Minor" });
    for (auto* c : { &keyBox, &scaleBox })
    {
        c->setColour(juce::ComboBox::backgroundColourId, panel);
        c->setColour(juce::ComboBox::textColourId, cream);
        c->setColour(juce::ComboBox::outlineColourId, brass.withAlpha(0.7f));
        addAndMakeVisible(c);
    }

    configureLabel(keyLabel, "KEY");
    configureLabel(scaleLabel, "SCALE");
    configureLabel(speedLabel, "SPEED");
    configureLabel(amountLabel, "AMOUNT");
    configureLabel(mixLabel, "MIX");

    configureSlider(speedSlider, " ms");
    configureSlider(amountSlider, " %");
    configureSlider(mixSlider, " %");
    addAndMakeVisible(speedSlider);
    addAndMakeVisible(amountSlider);
    addAndMakeVisible(mixSlider);

    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        auto& b = noteButtons[i];
        b.setButtonText(noteNames[static_cast<int>(i)]);
        b.setClickingTogglesState(true);
        b.setColour(juce::TextButton::buttonColourId, panel);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(89, 111, 70));
        b.setColour(juce::TextButton::textColourOffId, cream);
        b.setColour(juce::TextButton::textColourOnId, cream);
        addAndMakeVisible(b);
    }

    for (size_t i = 0; i < vocalControlCount; ++i)
    {
        configureSlider(vocalSliders[i], suffixes[i]);
        configureLabel(vocalLabels[i], controlLabels[i]);
        addAndMakeVisible(vocalSliders[i]);
        vocalSliders[i].setVisible(true);
    }

    for (auto& b : moduleButtons)
    {
        b.setButtonText("ON");
        b.setColour(juce::ToggleButton::textColourId, cream);
        b.setColour(juce::ToggleButton::tickColourId, brass);
        addAndMakeVisible(b);
    }

    auto& state = processor.parameters;
    keyAttachment = std::make_unique<ComboAttachment>(state, "key", keyBox);
    scaleAttachment = std::make_unique<ComboAttachment>(state, "scale", scaleBox);
    speedAttachment = std::make_unique<SliderAttachment>(state, "speed", speedSlider);
    amountAttachment = std::make_unique<SliderAttachment>(state, "amount", amountSlider);
    mixAttachment = std::make_unique<SliderAttachment>(state, "mix", mixSlider);

    for (size_t i = 0; i < noteAttachments.size(); ++i)
        noteAttachments[i] = std::make_unique<ButtonAttachment>(state, noteParameterIds[i], noteButtons[i]);

    for (size_t i = 0; i < vocalControlCount; ++i)
        vocalSliderAttachments[i] = std::make_unique<SliderAttachment>(state, controlIds[i], vocalSliders[i]);

    for (size_t i = 0; i < moduleButtons.size(); ++i)
        moduleButtonAttachments[i] = std::make_unique<ButtonAttachment>(state, moduleIds[i], moduleButtons[i]);

    startTimerHz(30);
}

JerzyAutoTuneAudioProcessorEditor::~JerzyAutoTuneAudioProcessorEditor()
{
    speedSlider.setLookAndFeel(nullptr);
    amountSlider.setLookAndFeel(nullptr);
    mixSlider.setLookAndFeel(nullptr);
    for (auto& s : vocalSliders) s.setLookAndFeel(nullptr);
}

void JerzyAutoTuneAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(1);
    slider.setLookAndFeel(&analogLookAndFeel);
    slider.setColour(juce::Slider::textBoxTextColourId, cream);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void JerzyAutoTuneAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, ink);
    label.setFont(juce::Font(9.5f, juce::Font::bold));
    addAndMakeVisible(label);
}

juce::Rectangle<int> JerzyAutoTuneAudioProcessorEditor::scaledBounds(float x, float y, float w, float h) const
{
    const float sx = static_cast<float>(getWidth()) / 1500.0f;
    const float sy = static_cast<float>(getHeight()) / 1050.0f;
    const float s = juce::jmin(sx, sy);
    const float ox = (static_cast<float>(getWidth()) - 1500.0f * s) * 0.5f;
    const float oy = (static_cast<float>(getHeight()) - 1050.0f * s) * 0.5f;
    return { juce::roundToInt(ox + x * s), juce::roundToInt(oy + y * s),
             juce::roundToInt(w * s), juce::roundToInt(h * s) };
}

void JerzyAutoTuneAudioProcessorEditor::layoutModule(int moduleIndex, int start, int count,
                                                     float x, float y, float w, float h)
{
    moduleButtons[static_cast<size_t>(moduleIndex)].setBounds(scaledBounds(x + w - 58.0f, y + 7.0f, 50.0f, 22.0f));

    const float top = y + 39.0f;
    const float bottomLabels = 25.0f;
    const float cellW = w / static_cast<float>(count);
    for (int i = 0; i < count; ++i)
    {
        const int index = start + i;
        const float cx = x + static_cast<float>(i) * cellW;
        vocalSliders[static_cast<size_t>(index)].setBounds(
            scaledBounds(cx + 4.0f, top, cellW - 8.0f, h - 66.0f));
        vocalLabels[static_cast<size_t>(index)].setBounds(
            scaledBounds(cx + 2.0f, y + h - bottomLabels, cellW - 4.0f, 17.0f));
    }
}

void JerzyAutoTuneAudioProcessorEditor::resized()
{
    keyLabel.setBounds(scaledBounds(90, 188, 110, 18));
    keyBox.setBounds(scaledBounds(90, 208, 110, 34));
    scaleLabel.setBounds(scaledBounds(215, 188, 160, 18));
    scaleBox.setBounds(scaledBounds(215, 208, 160, 34));

    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        const int row = static_cast<int>(i / 6);
        const int col = static_cast<int>(i % 6);
        noteButtons[i].setBounds(scaledBounds(90.0f + col * 48.0f, 267.0f + row * 32.0f, 43.0f, 27.0f));
    }

    speedSlider.setBounds(scaledBounds(430, 190, 130, 135));
    amountSlider.setBounds(scaledBounds(575, 190, 112, 135));
    mixSlider.setBounds(scaledBounds(700, 190, 112, 135));
    speedLabel.setBounds(scaledBounds(440, 323, 110, 17));
    amountLabel.setBounds(scaledBounds(575, 323, 112, 17));
    mixLabel.setBounds(scaledBounds(700, 323, 112, 17));

    layoutModule(0, 0, 5, 55, 415, 345, 220);
    layoutModule(1, 5, 4, 410, 415, 330, 220);
    layoutModule(2, 9, 4, 750, 415, 330, 220);
    layoutModule(3, 13, 4, 1090, 415, 355, 220);

    layoutModule(4, 17, 4, 55, 650, 330, 255);
    layoutModule(5, 21, 7, 395, 650, 545, 255);
    layoutModule(6, 28, 8, 950, 650, 495, 255);

    for (auto* s : { &speedSlider, &amountSlider, &mixSlider }) s->toFront(false);
    for (auto& s : vocalSliders) s.toFront(false);
    for (auto& l : vocalLabels) l.toFront(false);
    for (auto& b : moduleButtons) b.toFront(false);
}

void JerzyAutoTuneAudioProcessorEditor::drawModule(juce::Graphics& g, int moduleIndex, const juce::String& title,
                                                   float x, float y, float w, float h)
{
    const bool enabled = moduleButtons[static_cast<size_t>(moduleIndex)].getToggleState();
    g.setColour(faceDark);
    g.fillRoundedRectangle(x, y, w, h, 7.0f);
    g.setColour(enabled ? brass : juce::Colour(96, 91, 79));
    g.drawRoundedRectangle(x, y, w, h, 7.0f, 2.0f);
    g.setColour(panel);
    g.fillRoundedRectangle(x, y, w, 32.0f, 7.0f);
    g.setColour(enabled ? cream : juce::Colour(148, 144, 130));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(title, static_cast<int>(x + 12.0f), static_cast<int>(y + 6.0f),
               static_cast<int>(w - 78.0f), 20, juce::Justification::centredLeft);
}

void JerzyAutoTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(outer);
    const float sx = static_cast<float>(getWidth()) / 1500.0f;
    const float sy = static_cast<float>(getHeight()) / 1050.0f;
    const float s = juce::jmin(sx, sy);
    const float ox = (static_cast<float>(getWidth()) - 1500.0f * s) * 0.5f;
    const float oy = (static_cast<float>(getHeight()) - 1050.0f * s) * 0.5f;
    g.addTransform(juce::AffineTransform::translation(ox, oy).scaled(s));

    g.setColour(juce::Colour(46, 48, 45));
    g.fillRoundedRectangle(20, 18, 1460, 1014, 18.0f);
    g.setColour(metal);
    g.fillRoundedRectangle(35, 33, 1430, 984, 11.0f);
    g.setColour(juce::Colour(104, 94, 75));
    g.drawRoundedRectangle(35, 33, 1430, 984, 11.0f, 2.0f);

    g.setColour(panel);
    g.fillRoundedRectangle(55, 58, 1390, 90, 6.0f);
    g.setColour(brass);
    g.setFont(juce::Font(16.0f, juce::Font::plain));
    g.drawText("JERZY AUDIO", 85, 79, 180, 24, juce::Justification::centredLeft);
    g.setColour(cream);
    g.setFont(juce::Font(36.0f, juce::Font::plain));
    g.drawText("JERZY AUTO TUNE", 370, 73, 760, 50, juce::Justification::centred);
    g.setColour(juce::Colour(160, 160, 147));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("PITCH + FULL VOCAL CHANNEL", 1160, 82, 245, 18, juce::Justification::centredRight);
    g.drawText("VST3 - WINDOWS x64", 1160, 107, 245, 18, juce::Justification::centredRight);

    g.setColour(face);
    g.fillRoundedRectangle(55, 165, 1390, 205, 8.0f);
    g.setColour(ink);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("01 - PITCH CORRECTION", 82, 173, 300, 20, juce::Justification::centredLeft);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText("NOTE FILTER", 90, 247, 150, 16, juce::Justification::centredLeft);

    g.setColour(panel);
    g.fillRoundedRectangle(850, 190, 550, 142, 6.0f);
    g.setColour(cream);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("OUTPUT LEVEL", 875, 205, 140, 18, juce::Justification::centredLeft);
    const float level = juce::jlimit(0.0f, 1.0f, processor.getOutputPeak());
    g.setColour(juce::Colour(74, 78, 67));
    g.fillRoundedRectangle(875, 240, 495, 25, 4.0f);
    g.setColour(level > 0.9f ? red : brass);
    g.fillRoundedRectangle(875, 240, 495.0f * level, 25, 4.0f);
    const float db = juce::Decibels::gainToDecibels(juce::jmax(processor.getOutputPeak(), 1.0e-6f));
    g.setColour(cream);
    g.drawText(juce::String(db, 1) + " dB", 1210, 283, 160, 22, juce::Justification::centredRight);

    g.setColour(ink);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("02 - VOCAL PROCESSING CHAIN", 55, 389, 420, 20, juce::Justification::centredLeft);

    drawModule(g, 0, "GATE", 55, 415, 345, 220);
    drawModule(g, 1, "NOISE FILTER", 410, 415, 330, 220);
    drawModule(g, 2, "DE-ESSER", 750, 415, 330, 220);
    drawModule(g, 3, "SATURATION", 1090, 415, 355, 220);

    drawModule(g, 4, "DOUBLER", 55, 650, 330, 255);
    drawModule(g, 5, "VOCAL COMPRESSOR + LIMITER", 395, 650, 545, 255);
    drawModule(g, 6, "VOCAL EQ", 950, 650, 495, 255);

    g.setColour(ink);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText("GATE > NOISE FILTER > DE-ESSER > SATURATION > DOUBLER > COMP/LIMITER > VOCAL EQ",
               57, 931, 1050, 18, juce::Justification::centredLeft);
    g.drawText("ALL CONTROLS AUTOMATABLE IN HOST", 1130, 931, 315, 18, juce::Justification::centredRight);
}

void JerzyAutoTuneAudioProcessorEditor::timerCallback()
{
    repaint();
}

void JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float startAngle, float endAngle, juce::Slider& slider)
{
    JerzyAudioUI::HardwareLookAndFeel::drawRotarySlider(g,x,y,width,height,sliderPos,startAngle,endAngle,slider);
}
