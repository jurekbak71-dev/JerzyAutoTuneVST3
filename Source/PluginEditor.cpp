#include "PluginEditor.h"

namespace
{
constexpr int designWidth = 1120, designHeight = 780;
const juce::Colour outer(22, 24, 24), panel(34, 38, 38), face(66, 68, 62);
const juce::Colour cream(239, 233, 213), muted(179, 183, 172), brass(223, 179, 90);
const juce::Colour green(116, 203, 166), red(242, 108, 96);
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

struct ModuleLayout
{
    const char* title;
    int first, count, columns;
    juce::Rectangle<int> bounds;
};
const std::array<ModuleLayout, 7> modules {{
    { "GATE", 0, 5, 5, { 16, 300, 300, 176 } },
    { "NOISE FILTER", 5, 4, 4, { 324, 300, 252, 176 } },
    { "DE-ESSER", 9, 4, 4, { 584, 300, 252, 176 } },
    { "SATURATION", 13, 4, 4, { 844, 300, 260, 176 } },
    { "DOUBLER", 17, 4, 2, { 16, 488, 244, 248 } },
    { "COMPRESSOR / LIMITER", 21, 7, 4, { 268, 488, 414, 248 } },
    { "VOCAL EQ", 28, 7, 4, { 690, 488, 414, 248 } }
}};
const std::array<const char*, 36> hints {
    "Level above which the gate opens.", "Attenuation while the gate is closed.",
    "Time for the gate to open.", "Time to stay open after the signal falls below threshold.", "Time for the gate to close.",
    "Level below which noise attenuation increases.", "Maximum noise attenuation (downward expansion, not spectral denoising).",
    "Recovery time for the noise envelope.", "Removes low-frequency rumble.",
    "Sibilance level at which attenuation starts.", "Frequency separating the sibilance band.",
    "Maximum high-band reduction.", "Recovery time after sibilance.",
    "Level driven into the saturation stage.", "Tilt the saturation towards dark or bright.",
    "Blend saturated and clean signal.", "Output trim of the saturation stage.",
    "Blend the delayed voice.", "Base delay of the doubled voice.", "Depth of delay modulation (nominal cents).", "Stereo width of the doubled voice.",
    "Level above which compression begins.", "Compression ratio above threshold.", "Compressor attack time.",
    "Compressor recovery time.", "Width of the soft transition around threshold.", "Gain after compression.",
    "Soft-clip ceiling before EQ and output trim; later gain can exceed it.",
    "High-pass cutoff.", "Low shelf at 160 Hz.", "Centre frequency of the presence band.",
    "Presence-band gain.", "Presence bandwidth: larger Q is narrower.", "High shelf at 10 kHz.", "Low-pass cutoff.",
    "Final output trim. Remains active when EQ is bypassed."
};

bool inScale(int note, int key, int scale)
{
    static constexpr std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7> minor { 0, 2, 3, 5, 7, 8, 10 };
    const int relative = (note - key + 12) % 12;
    const auto& intervals = scale == 1 ? major : minor;
    return scale == 0 || std::find(intervals.begin(), intervals.end(), relative) != intervals.end();
}

void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> r,
          float size, juce::Colour colour, int alignment = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, juce::Font::bold));
    g.drawText(value, r, alignment);
}

void meter(juce::Graphics& g, const juce::String& title, float db, bool clip, int y)
{
    text(g, title, { 735, y, 36, 20 }, 11.0f, muted);
    const juce::Rectangle<float> track(775.0f, static_cast<float>(y + 5), 206.0f, 9.0f);
    g.setColour(outer); g.fillRoundedRectangle(track, 3.0f);
    g.setColour(clip ? red : green);
    g.fillRoundedRectangle(track.withWidth(track.getWidth() * juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f)), 3.0f);
    text(g, db <= -59.9f ? "-inf" : juce::String(db, 1), { 983, y, 45, 20 }, 11.0f, clip ? red : cream,
         juce::Justification::centredRight);
}
}

JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::AnalogLookAndFeel()
{
    setColour(juce::Label::textColourId, cream);
    setColour(juce::ComboBox::backgroundColourId, outer);
    setColour(juce::ComboBox::textColourId, cream);
    setColour(juce::ComboBox::outlineColourId, face);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, cream);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, brass);
    setColour(juce::PopupMenu::highlightedTextColourId, outer);
    setColour(juce::TextButton::buttonColourId, outer);
    setColour(juce::TextButton::buttonOnColourId, brass);
    setColour(juce::TextButton::textColourOffId, cream);
    setColour(juce::TextButton::textColourOnId, outer);
}
juce::Font JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::getLabelFont(juce::Label& l)
{ return dynamic_cast<juce::Slider*>(l.getParentComponent()) != nullptr ? juce::Font(juce::FontOptions(11.0f)) : l.getFont(); }
juce::Font JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::getComboBoxFont(juce::ComboBox&)
{ return juce::FontOptions(13.0f); }
juce::Font JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{ return juce::FontOptions(12.0f, juce::Font::bold); }

JerzyAutoTuneAudioProcessorEditor::JerzyAutoTuneAudioProcessorEditor(JerzyAutoTuneAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    const int savedWidth = processor.editorWidth.load();
    const int savedHeight = processor.editorHeight.load();
    setLookAndFeel(&analogLookAndFeel);
    setOpaque(true);
    addAndMakeVisible(surface);
    surface.setSize(designWidth, designHeight);
    for (int i = 0; i < noteNames.size(); ++i) keyBox.addItem(noteNames[i], i + 1);
    scaleBox.addItemList({ "Chromatic", "Major", "Minor" }, 1);
    for (auto* c : { &keyBox, &scaleBox, &zoomBox }) surface.addAndMakeVisible(c);
    keyBox.setComponentID("key"); scaleBox.setComponentID("scale"); zoomBox.setComponentID("uiZoom");
    keyBox.setTooltip("Root note of the selected scale.");
    scaleBox.setTooltip("Allowed notes are the intersection of this scale and NOTE FILTER.");
    zoomBox.setTooltip("Resize the entire vector interface. You can also drag the lower-right corner.");
    for (const int percent : { 75, 90, 100, 125, 150 }) zoomBox.addItem(juce::String(percent) + "%", percent);
    zoomBox.onChange = [this] {
        const int percent = zoomBox.getSelectedId();
        if (percent > 0) setSize(designWidth * percent / 100, designHeight * percent / 100);
    };
    configureLabel(keyLabel, "KEY"); configureLabel(scaleLabel, "SCALE");
    configureLabel(speedLabel, "SPEED"); configureLabel(amountLabel, "AMOUNT"); configureLabel(mixLabel, "PITCH MIX");
    configureLabel(noteStatus, {});
    noteStatus.setJustificationType(juce::Justification::centredLeft);
    configureSlider(speedSlider, "speed", "Correction response time. Lower values give faster pitch transitions.");
    configureSlider(amountSlider, "amount", "How far the voice is moved towards a permitted note.");
    configureSlider(mixSlider, "mix", "Pitch-corrected / latency-aligned dry blend. The vocal chain still processes both.");
    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        auto& b = noteButtons[i];
        b.setButtonText(noteNames[static_cast<int>(i)]);
        b.setComponentID(noteParameterIds[i]);
        b.setClickingTogglesState(true);
        b.onClick = [this] { updateControlStates(); };
        surface.addAndMakeVisible(b);
    }
    for (auto* b : { &allNotes, &noNotes }) surface.addAndMakeVisible(b);
    allNotes.setComponentID("allNotes"); noNotes.setComponentID("noNotes");
    allNotes.setTooltip("Enable all 12 note switches; the scale still applies.");
    noNotes.setTooltip("Disable pitch correction by clearing the note mask. Vocal effects remain active.");
    allNotes.onClick = [this] { setAllNotes(true); };
    noNotes.onClick = [this] { setAllNotes(false); };
    for (size_t i = 0; i < vocalControlCount; ++i)
    {
        configureSlider(vocalSliders[i], controlIds[i], hints[i]);
        configureLabel(vocalLabels[i], controlLabels[i]);
    }
    for (size_t i = 0; i < moduleButtons.size(); ++i)
    {
        auto& b = moduleButtons[i];
        b.setClickingTogglesState(true);
        b.setComponentID(moduleIds[i]);
        b.setTooltip(juce::String("Enable / bypass ") + modules[i].title);
        b.onClick = [this] { updateControlStates(); };
        surface.addAndMakeVisible(b);
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

    // Attachments provide parameter conversion; append units afterwards and keep editable numeric values.
    const auto format = [](juce::Slider& s, const juce::String& suffix, int places) {
        s.textFromValueFunction = [suffix, places](double v) { return juce::String(v, places) + suffix; };
        s.valueFromTextFunction = [](const juce::String& t) { return t.getDoubleValue(); };
        s.updateText();
    };
    format(speedSlider, " ms", 1); format(amountSlider, " %", 1); format(mixSlider, " %", 1);
    for (size_t i = 0; i < vocalControlCount; ++i)
    {
        const auto interval = vocalSliders[i].getInterval();
        format(vocalSliders[i], suffixes[i], interval >= 1.0 ? 0 : (interval < 0.1 ? 2 : 1));
    }
    keyBox.onChange = [this] { updateControlStates(); };
    scaleBox.onChange = [this] { updateControlStates(); };
    layoutControls();
    updateControlStates();
    setResizable(true, true);
    setResizeLimits(840, 585, 1680, 1170);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
    setSize(savedWidth, savedHeight);
    startTimerHz(30);
}

JerzyAutoTuneAudioProcessorEditor::~JerzyAutoTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void JerzyAutoTuneAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String& id,
                                                       const juce::String& hint)
{
    slider.setComponentID(id);
    slider.setName(processor.parameters.getParameter(id)->getName(80));
    slider.setTooltip(hint + " Double-click: default. Shift-drag: fine adjustment. Click value: type a number.");
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 21);
    slider.setColour(juce::Slider::textBoxTextColourId, cream);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, outer);
    slider.setColour(juce::Slider::textBoxOutlineColourId, face);
    slider.setScrollWheelEnabled(false);
    slider.setMouseDragSensitivity(180);
    const auto* p = processor.parameters.getParameter(id);
    slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
    surface.addAndMakeVisible(slider);
}

void JerzyAutoTuneAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& value)
{
    label.setText(value, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setBorderSize(juce::BorderSize<int>(0));
    surface.addAndMakeVisible(label);
}

void JerzyAutoTuneAudioProcessorEditor::layoutControls()
{
    zoomBox.setBounds(998, 22, 90, 28);
    keyLabel.setBounds(32, 121, 62, 18); keyBox.setBounds(32, 142, 82, 29);
    scaleLabel.setBounds(128, 121, 96, 18); scaleBox.setBounds(128, 142, 146, 29);
    allNotes.setBounds(280, 142, 48, 29); noNotes.setBounds(332, 142, 48, 29);
    for (size_t i = 0; i < noteButtons.size(); ++i)
        noteButtons[i].setBounds(32 + static_cast<int>(i) * 29, 193, 26, 32);
    noteStatus.setBounds(32, 235, 350, 18);
    speedSlider.setBounds(404, 126, 90, 111); speedLabel.setBounds(404, 239, 90, 18);
    amountSlider.setBounds(508, 126, 90, 111); amountLabel.setBounds(508, 239, 90, 18);
    mixSlider.setBounds(612, 126, 90, 111); mixLabel.setBounds(612, 239, 90, 18);
    vocalSliders[35].setBounds(1036, 139, 60, 93); vocalLabels[35].setBounds(1030, 236, 72, 18);
    for (size_t m = 0; m < modules.size(); ++m)
    {
        const auto& module = modules[m];
        const auto r = module.bounds;
        moduleButtons[m].setBounds(r.getRight() - 59, r.getY() + 7, 49, 23);
        const int rows = (module.count + module.columns - 1) / module.columns;
        const int cellWidth = (r.getWidth() - 12) / module.columns;
        const int cellHeight = (r.getHeight() - 42) / rows;
        for (int j = 0; j < module.count; ++j)
        {
            const size_t index = static_cast<size_t>(module.first + j);
            const int x = r.getX() + 6 + (j % module.columns) * cellWidth;
            const int y = r.getY() + 38 + (j / module.columns) * cellHeight;
            vocalLabels[index].setBounds(x, y, cellWidth, 17);
            vocalSliders[index].setBounds(x + 2, y + 18, cellWidth - 4, cellHeight - 22);
            vocalSliders[index].setTextBoxStyle(juce::Slider::TextBoxBelow, false, cellWidth - 6, 21);
        }
    }
}

void JerzyAutoTuneAudioProcessorEditor::resized()
{
    const float scale = juce::jmin(static_cast<float>(getWidth()) / designWidth,
                                  static_cast<float>(getHeight()) / designHeight);
    const float x = (getWidth() - designWidth * scale) * 0.5f;
    const float y = (getHeight() - designHeight * scale) * 0.5f;
    // One transform scales paint, fonts, text editors and hit targets together.
    surface.setTransform(juce::AffineTransform::scale(scale).translated(x, y));
    const int percent = juce::roundToInt(scale * 100.0f);
    zoomBox.setSelectedId(0, juce::dontSendNotification);
    zoomBox.setText(juce::String(percent) + "%", juce::dontSendNotification);
    if (getWidth() >= 840 && getHeight() >= 585)
    {
        processor.editorWidth.store(getWidth()); processor.editorHeight.store(getHeight());
    }
}

void JerzyAutoTuneAudioProcessorEditor::setAllNotes(bool enabled)
{
    for (const auto* id : noteParameterIds)
    {
        auto* p = processor.parameters.getParameter(id);
        p->beginChangeGesture(); p->setValueNotifyingHost(enabled ? 1.0f : 0.0f); p->endChangeGesture();
    }
    updateControlStates();
}

void JerzyAutoTuneAudioProcessorEditor::updateControlStates()
{
    const int key = static_cast<int>(processor.parameters.getRawParameterValue("key")->load());
    const int scale = static_cast<int>(processor.parameters.getRawParameterValue("scale")->load());
    int active = 0;
    for (size_t i = 0; i < noteButtons.size(); ++i)
    {
        const bool member = inScale(static_cast<int>(i), key, scale);
        const bool selected = processor.parameters.getRawParameterValue(noteParameterIds[i])->load() >= 0.5f;
        if (member && selected) ++active;
        noteButtons[i].setColour(juce::TextButton::buttonOnColourId, member ? brass : face);
        noteButtons[i].setColour(juce::TextButton::textColourOnId, member ? outer : muted);
        noteButtons[i].setTooltip(noteNames[static_cast<int>(i)] + (member ? ": in the selected scale." : ": outside this scale; switch is remembered."));
    }
    noteStatus.setText(active == 0 ? "No target notes - pitch correction bypassed" :
                       juce::String(active) + " active notes  /  gold = allowed by scale", juce::dontSendNotification);
    noteStatus.setColour(juce::Label::textColourId, active == 0 ? red : muted);
    for (size_t m = 0; m < modules.size(); ++m)
    {
        const bool on = processor.parameters.getRawParameterValue(moduleIds[m])->load() >= 0.5f;
        moduleButtons[m].setButtonText(on ? "ON" : "BYP");
        for (int j = 0; j < modules[m].count; ++j)
        {
            const size_t i = static_cast<size_t>(modules[m].first + j);
            // Remain editable while bypassed so settings can be prepared before enabling.
            vocalSliders[i].setAlpha(on ? 1.0f : 0.48f);
            vocalLabels[i].setAlpha(on ? 1.0f : 0.48f);
        }
    }
    surface.repaint();
}

void JerzyAutoTuneAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(outer); }

void JerzyAutoTuneAudioProcessorEditor::paintSurface(juce::Graphics& g)
{
    g.fillAll(outer);
    g.setColour(brass); g.fillRect(16, 16, 4, 47);
    text(g, "JERZY AUTO TUNE", { 32, 16, 430, 34 }, 27.0f, cream);
    text(g, "PITCH CORRECTION  /  VOCAL CHANNEL", { 33, 50, 440, 18 }, 11.0f, muted);
    text(g, "1.5", { 850, 25, 55, 24 }, 14.0f, brass, juce::Justification::centredRight);
    text(g, "UI SIZE", { 923, 25, 68, 24 }, 11.0f, muted);
    g.setColour(panel); g.fillRoundedRectangle(16.0f, 84.0f, 1088.0f, 185.0f, 9.0f);
    text(g, "01  PITCH CORRECTION", { 32, 93, 350, 22 }, 13.0f, brass);
    text(g, "NOTE FILTER", { 32, 173, 300, 18 }, 11.0f, muted);
    g.setColour(face); g.drawVerticalLine(720, 101.0f, 252.0f);
    text(g, "MONITOR", { 735, 99, 144, 22 }, 12.0f, brass);
    const float pitch = processor.getDetectedPitch();
    const int nearest = juce::roundToInt(pitch);
    const juce::String detected = pitch < 0.0f ? "--" : noteNames[(nearest % 12 + 12) % 12] + juce::String(nearest / 12 - 1);
    text(g, "NOTE  " + detected, { 735, 124, 130, 25 }, 15.0f, cream);
    const float cents = processor.getCorrectionCents();
    text(g, "SHIFT  " + juce::String(cents > 0.0f ? "+" : "") + juce::String(cents, 0) + " ct",
         { 868, 124, 160, 25 }, 12.0f, muted, juce::Justification::centredRight);
    meter(g, "IN", displayedInput, inputClipTicks > 0, 155);
    meter(g, "OUT", displayedOutput, outputClipTicks > 0, 181);
    text(g, "COMP GR", { 735, 213, 90, 22 }, 11.0f, muted);
    const juce::Rectangle<float> gr(823.0f, 220.0f, 135.0f, 7.0f);
    g.setColour(outer); g.fillRoundedRectangle(gr, 3.0f);
    g.setColour(brass); g.fillRoundedRectangle(gr.withWidth(gr.getWidth() * juce::jlimit(0.0f, 1.0f, displayedReduction / 24.0f)), 3.0f);
    text(g, juce::String(displayedReduction, 1) + " dB", { 960, 212, 68, 22 }, 11.0f, brass, juce::Justification::centredRight);
    text(g, "02  VOCAL PROCESSING", { 16, 275, 420, 22 }, 12.0f, muted);
    for (size_t i = 0; i < modules.size(); ++i)
    {
        const auto r = modules[i].bounds;
        const bool enabled = moduleButtons[i].getToggleState();
        g.setColour(panel); g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(enabled ? face : face.withAlpha(0.45f)); g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);
        g.setColour(enabled ? brass : muted); g.fillRoundedRectangle(static_cast<float>(r.getX() + 10), static_cast<float>(r.getY() + 14), 3.0f, 10.0f, 1.0f);
        text(g, modules[i].title, { r.getX() + 21, r.getY() + 7, r.getWidth() - 82, 23 }, 12.0f, enabled ? cream : muted);
    }
    text(g, "PITCH > GATE > NOISE > DE-ESSER > SATURATION > DOUBLER > COMP / LIMITER > EQ > OUT",
         { 18, 744, 850, 20 }, 10.5f, muted);
    text(g, "Double-click: reset", { 883, 744, 220, 20 }, 11.0f, muted, juce::Justification::centredRight);
}

void JerzyAutoTuneAudioProcessorEditor::timerCallback()
{
    const float input = processor.getInputPeak(), output = processor.getOutputPeak();
    displayedInput = juce::jmax(juce::Decibels::gainToDecibels(input, -60.0f), displayedInput - 1.3f);
    displayedOutput = juce::jmax(juce::Decibels::gainToDecibels(output, -60.0f), displayedOutput - 1.3f);
    displayedReduction = juce::jmax(processor.getCompressorReduction(), displayedReduction - 0.5f);
    inputClipTicks = input >= 1.0f ? 45 : juce::jmax(0, inputClipTicks - 1);
    outputClipTicks = output >= 1.0f ? 45 : juce::jmax(0, outputClipTicks - 1);
    updateControlStates();
}

void JerzyAutoTuneAudioProcessorEditor::AnalogLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float startAngle, float endAngle, juce::Slider&)
{
    const float diameter = juce::jmax(10.0f, static_cast<float>(juce::jmin(width, height)) - 10.0f);
    const float radius = diameter * 0.5f;
    const float cx = x + width * 0.5f, cy = y + height * 0.5f;
    const float angle = startAngle + position * (endAngle - startAngle);
    juce::Path track, value;
    track.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour(face); g.strokePath(track, juce::PathStrokeType(2.0f));
    g.setColour(brass); g.strokePath(value, juce::PathStrokeType(2.5f));
    const float r = radius - 5.0f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(88, 91, 84), cx, cy - r, outer, cx, cy + r, false));
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(cream);
    g.drawLine(cx + std::sin(angle) * r * 0.4f, cy - std::cos(angle) * r * 0.4f,
               cx + std::sin(angle) * r * 0.85f, cy - std::cos(angle) * r * 0.85f, 2.1f);
}
