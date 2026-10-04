#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <set>

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
juce::Component* find(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* c = find(*child, id)) return c;
    return nullptr;
}
void setParameter(JerzyAutoTuneAudioProcessor& p, const char* id, float realValue)
{
    auto* parameter = p.parameters.getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(realValue));
}
void checkControls(juce::Component& root, juce::Component& editor, std::set<juce::String>& ids,
                   JerzyAutoTuneAudioProcessor& processor)
{
    for (auto* c : root.getChildren())
    {
        if (auto* parameter = processor.parameters.getParameter(c->getComponentID()))
        {
            const auto bounds = editor.getLocalArea(c, c->getLocalBounds());
            require(c->isShowing(), "Parameter control hidden");
            require(editor.getLocalBounds().contains(bounds), "Parameter control clipped by editor");
            require(bounds.getWidth() >= 18 && bounds.getHeight() >= 17, "Unusable control size");
            ids.insert(c->getComponentID());
            if (auto* slider = dynamic_cast<juce::Slider*>(c))
            {
                parameter->setValueNotifyingHost(0.37f);
                require(std::abs(slider->getValue() - parameter->convertFrom0to1(parameter->getValue())) < 0.02,
                        "Host automation did not reach slider");
                slider->setValue(parameter->convertFrom0to1(0.73f), juce::sendNotificationSync);
                require(std::abs(slider->getValue() - parameter->convertFrom0to1(parameter->getValue())) < 0.02,
                        "Slider did not update DSP parameter");
            }
            else if (auto* button = dynamic_cast<juce::Button*>(c))
            {
                parameter->setValueNotifyingHost(0.0f);
                require(!button->getToggleState(), "Button did not follow host OFF");
                parameter->setValueNotifyingHost(1.0f);
                require(button->getToggleState(), "Button did not follow host ON");
            }
            else if (auto* combo = dynamic_cast<juce::ComboBox*>(c))
            {
                combo->setSelectedId(2, juce::sendNotificationSync);
                require(std::abs(parameter->convertFrom0to1(parameter->getValue()) - 1.0f) < 0.01f,
                        "Combo did not update parameter");
            }
        }
        checkControls(*c, editor, ids, processor);
    }
}
void capture(juce::Component& editor, const juce::String& name)
{
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile("gui-previews");
    folder.createDirectory();
    auto stream = folder.getChildFile(name + ".png").createOutputStream();
    require(stream != nullptr, "Cannot write GUI preview");
    juce::PNGImageFormat png;
    require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream), "Cannot render GUI");
}

int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI initialise;
        JerzyAutoTuneAudioProcessor processor;
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
        editor->setVisible(true);
        require(editor->getWidth() == 1008 && editor->getHeight() == 702, "Wrong default window size");
        capture(*editor, "default-1008x702");
        const auto original = processor.parameters.copyState();
        for (const auto size : { juce::Point<int>(840, 585), { 1120, 780 }, { 1680, 1170 }, { 1200, 620 } })
        {
            editor->setSize(size.x, size.y);
            std::set<juce::String> ids;
            checkControls(*editor, *editor, ids, processor);
            require(ids.size() == static_cast<size_t>(processor.getParameters().size()), "A DSP parameter is missing from GUI");
            processor.parameters.replaceState(original);
            capture(*editor, juce::String(size.x) + "x" + juce::String(size.y));
        }
        auto* zoom = dynamic_cast<juce::ComboBox*>(find(*editor, "uiZoom"));
        require(zoom != nullptr, "No zoom control");
        zoom->setSelectedId(75, juce::sendNotificationSync);
        require(editor->getWidth() == 840 && editor->getHeight() == 585, "Zoom did not resize editor");
        // Verify host-to-GUI updates, no mouse hit-area drift after transformation.
        auto* gate = find(*editor, "gateThreshold");
        const auto gatePoint = editor->getLocalPoint(gate, gate->getLocalBounds().getCentre());
        auto* hit = editor->getComponentAt(gatePoint);
        require(hit == gate || gate->isParentOf(hit), "Transformed control has a displaced hit target");
        setParameter(processor, "eqEnabled", 0.0f);
        auto* output = dynamic_cast<juce::Slider*>(find(*editor, "outputGain"));
        output->setValue(-4.0, juce::sendNotificationSync);
        require(std::abs(processor.parameters.getRawParameterValue("outputGain")->load() + 4.0f) < 0.01f,
                "Output trim unavailable with EQ bypassed");
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        editor.reset();
        for (int i = 0; i < 8; ++i)
        {
            editor.reset(processor.createEditor());
            require(editor->getWidth() == 840 && editor->getHeight() == 585, "Reopening lost window size");
            editor.reset();
        }
        JerzyAutoTuneAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        editor.reset(restored.createEditor());
        require(editor->getWidth() == 840 && editor->getHeight() == 585, "State recall lost window size");
        editor.reset();
        // Old sessions have no window properties and must still restore parameters.
        auto old = original.createXml();
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary(*old, legacy);
        restored.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
        editor.reset(restored.createEditor());
        require(editor->getWidth() == 1008, "Legacy state did not use safe default size");
        editor.reset();

        // An explicit mask outside C major must bypass correction, not tune to chromatic notes.
        const char* notes[] = { "noteC", "noteCs", "noteD", "noteDs", "noteE", "noteF", "noteFs", "noteG", "noteGs", "noteA", "noteAs", "noteB" };
        setParameter(processor, "key", 0); setParameter(processor, "scale", 1);
        for (auto* note : notes) setParameter(processor, note, juce::String(note) == "noteCs" ? 1.0f : 0.0f);
        for (auto* id : { "gateEnabled", "noiseEnabled", "deEsserEnabled", "satEnabled", "doublerEnabled", "compEnabled", "eqEnabled" })
            setParameter(processor, id, 0.0f);
        setParameter(processor, "outputGain", 0.0f);
        processor.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        double phase = 0.0;
        for (int block = 0; block < 160; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float sample = 0.2f * static_cast<float>(std::sin(phase));
                phase += juce::MathConstants<double>::twoPi * 447.0 / 48000.0;
                buffer.setSample(0, i, sample); buffer.setSample(1, i, sample);
            }
            processor.processBlock(buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i) require(std::isfinite(buffer.getSample(ch, i)), "Non-finite audio");
        }
        require(processor.getDetectedPitch() > 68.0f && processor.getDetectedPitch() < 70.0f, "Pitch detection failed");
        require(std::abs(processor.getCorrectionCents()) < 0.01f, "Empty scale/mask intersection still shifts pitch");
        // The input meter must measure the louder channel, including opposite-polarity stereo.
        for (int i = 0; i < 256; ++i) { buffer.setSample(0, i, 0.5f); buffer.setSample(1, i, -0.5f); }
        processor.processBlock(buffer, midi);
        require(std::abs(processor.getInputPeak() - 0.5f) < 0.001f, "Stereo input meter cancels opposite-polarity channels");
        processor.releaseResources();
        std::cout << "PASS: 60 parameter controls, automation, scaling, hit targets, state recall, editor lifecycle, audio and note mask\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
