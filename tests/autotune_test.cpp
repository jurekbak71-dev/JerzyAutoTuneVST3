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
        if(!c->isVisible())continue; // Other pages are checked through the visible-parameter union below.
        if (auto* parameter = processor.parameters.getParameter(c->getComponentID()))
        {
            const auto bounds = editor.getLocalArea(c, c->getLocalBounds());
            require(c->isVisible() && root.isVisible(), ("Parameter control hidden: "+c->getComponentID()).toRawUTF8());
            require(editor.getLocalBounds().contains(bounds), "Parameter control clipped by editor");
            require(bounds.getWidth() >= 18 && bounds.getHeight() >= 17, ("Unusable control size: "+c->getComponentID()+" "+juce::String(bounds.getWidth())+"x"+juce::String(bounds.getHeight())).toRawUTF8());
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
    const auto imageFile=folder.getChildFile(name + ".png");
    imageFile.deleteFile();
    auto stream = imageFile.createOutputStream();
    require(stream != nullptr, "Cannot write GUI preview");
    juce::PNGImageFormat png;
    require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream), "Cannot render GUI");
}

void runStudioTests();

int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI initialise;
        JerzyAutoTuneAudioProcessor processor;
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
        editor->setVisible(true);
        require(editor->getWidth()==1280&&editor->getHeight()==800,"Wrong studio window size");
        const auto original=processor.parameters.copyState();
        auto* studio=dynamic_cast<JerzyAutoTuneAudioProcessorEditor*>(editor.get());
        require(studio!=nullptr,"Missing native studio editor");
        for(const auto size : {juce::Point<int>(960,600),{1280,800},{1920,1200},{1400,700}})
        {
            editor->setSize(size.x,size.y);
            std::set<juce::String> ids;
            for(int page=0;page<7;++page)
            {
                studio->showPage(page,true);
                checkControls(*editor,*editor,ids,processor);
                processor.parameters.replaceState(original.createCopy());
                if(size==juce::Point<int>(1280,800))capture(*editor,"studio-"+juce::String(page)+"-all-controls");
                studio->showPage(page,false);
                if(size==juce::Point<int>(1280,800))capture(*editor,"studio-"+juce::String(page)+"-focus");
            }
            require(ids.size()==static_cast<size_t>(processor.getParameters().size()),"A parameter is missing from studio pages");
        }
        studio->showPage(0,false);
        auto* zoom=dynamic_cast<juce::ComboBox*>(find(*editor,"uiZoom"));
        require(zoom!=nullptr,"No zoom control");zoom->setSelectedId(75,juce::sendNotificationSync);
        require(editor->getWidth()==960&&editor->getHeight()==600,"Studio zoom did not resize");
        studio->showPage(1,true);
        auto* gate=find(*editor,"gateThreshold");require(gate!=nullptr,"Legacy gate not accessible");
        const auto point=editor->getLocalPoint(gate,gate->getLocalBounds().getCentre());
        auto* hit=editor->getComponentAt(point);require(hit==gate||gate->isParentOf(hit),"Transformed mouse hit target moved");
        setParameter(processor,"eqEnabled",0);setParameter(processor,"outputGain",-4);
        juce::MemoryBlock state;processor.getStateInformation(state);editor.reset();
        for(int i=0;i<4;++i){editor.reset(processor.createEditor());require(editor->getWidth()==960&&editor->getHeight()==600,"Reopening lost studio size");editor.reset();}
        JerzyAutoTuneAudioProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));
        for(int i=0;i<processor.getParameters().size();++i)require(std::abs(processor.getParameters()[i]->getValue()-restored.getParameters()[i]->getValue())<1.e-5f,"Parameter state roundtrip failed");
        auto old=original.createXml();juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*old,legacy);
        restored.setStateInformation(legacy.getData(),int(legacy.getSize()));
        require(restored.parameters.getRawParameterValue("engineMode")->load()<.5f,"Old state did not select Legacy engine");
        editor.reset(restored.createEditor());require(editor->getWidth()==1280,"Legacy state did not use safe studio size");editor.reset();
        processor.parameters.replaceState(original.createCopy());
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
        // Integration: measured output tuning, stereo detector, final ceiling and latency.
        for(double fs : {44100.,48000.,96000.,192000.})
        {
            JerzyAutoTuneAudioProcessor p;
            setParameter(p,"engineMode",1); setParameter(p,"scale",0);
            setParameter(p,"vibratoPreserve",0); setParameter(p,"speed",0);
            setParameter(p,"limiterEnabled",0);
            for(auto* id : {"gateEnabled","noiseEnabled","deEsserEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled"}) setParameter(p,id,0);
            p.prepareToPlay(fs,256);
            std::vector<double> crossings;
            float previous=0; int absolute=0;
            for(int block=0;block<static_cast<int>(fs/256)+1;++block)
            {
                for(int i=0;i<256;++i)
                {
                    const float x=.2f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*447.*(absolute+i)/fs));
                    buffer.setSample(0,i,x); buffer.setSample(1,i,-x);
                }
                p.processBlock(buffer,midi);
                for(int i=0;i<256;++i)
                {
                    const float x=buffer.getSample(0,i);
                    require(std::isfinite(x),"Nonfinite advanced output");
                    if(absolute+i>fs*.55 && previous<=0 && x>0)
                        crossings.push_back(absolute+i-1+static_cast<double>(-previous)/(x-previous));
                    previous=x;
                }
                absolute+=256;
            }
            require(p.getDetectedPitch()>69.1f && p.getDetectedPitch()<69.5f,"Advanced opposite-polarity stereo detection failed");
            require(p.getPitchConfidence()>.9f,"Advanced confidence missing");
            require(crossings.size()>100,"Output tuning test produced no periodic signal");
            const double outputHz=fs*(crossings.size()-1)/(crossings.back()-crossings.front());
            require(std::abs(outputHz-440.)<1.,"Advanced pitch correction does not tune 447 Hz to 440 Hz");
            // With pitch mix zero, all other modules off, unity trim and limiter off,
            // the dry impulse must emerge at exactly the host-reported latency.
            setParameter(p,"mix",0);
            p.prepareToPlay(fs,256);
            int found=-1;
            for(int block=0;block<p.getLatencySamples()/256+3;++block)
            {
                buffer.clear(); if(block==0) { buffer.setSample(0,0,.25f); buffer.setSample(1,0,.25f); }
                p.processBlock(buffer,midi);
                for(int i=0;i<256;++i)
                    if(std::abs(buffer.getSample(0,i))>1.e-7f) { require(found<0,"Dry impulse smeared"); found=block*256+i; }
            }
            require(found==p.getLatencySamples(),"Host latency does not match dry path");
            // Limiter remains independent of compressor, EQ and output boosts.
            setParameter(p,"limiterEnabled",1); setParameter(p,"limiterCeiling",-3);
            setParameter(p,"outputGain",12); setParameter(p,"eqEnabled",1);
            setParameter(p,"eqLow",12); setParameter(p,"eqMid",12); setParameter(p,"eqHigh",12);
            p.prepareToPlay(fs,256);
            float maximum=0;
            for(int block=0;block<p.getLatencySamples()/256+80;++block)
            {
                for(int i=0;i<256;++i) { const float x=2.f*static_cast<float>(std::sin((block*256+i)*.13)); buffer.setSample(0,i,x); buffer.setSample(1,i,-x*.5f); }
                p.processBlock(buffer,midi);
                maximum=juce::jmax(maximum,buffer.getMagnitude(0,0,256));
            }
            require(maximum>.1f && maximum<=juce::Decibels::decibelsToGain(-3.f)+1.e-5f,"Final limiter ceiling failed after EQ/output");
            p.releaseResources();
        }
        // Parameter identity/order from 1.5 must remain an exact prefix.
        const char* legacyIds[] = {
"key","scale","noteC","noteCs","noteD","noteDs","noteE","noteF","noteFs","noteG","noteGs","noteA","noteAs","noteB","speed","amount","mix","gateEnabled","gateThreshold","gateRange","gateAttack","gateHold","gateRelease","noiseEnabled","noiseThreshold","noiseReduction","noiseRelease","noiseHighPass","deEsserEnabled","deEsserThreshold","deEsserFreq","deEsserAmount","deEsserRelease","satEnabled","satDrive","satTone","satMix","satOutput","doublerEnabled","doublerAmount","doublerDelay","doublerDetune","doublerWidth","compEnabled","compThreshold","compRatio","compAttack","compRelease","compKnee","compMakeup","limiterCeiling","eqEnabled","eqLowCut","eqLow","eqMidFreq","eqMid","eqMidQ","eqHigh","eqHighCut","outputGain"};
        for(int i=0;i<60;++i)
        {
            auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(processor.getParameters()[i]);
            require(parameter!=nullptr && parameter->paramID==legacyIds[i],"Legacy parameter identity/order changed");
        }
        // Expression is checked at the audio output, rather than by reading controls.
        const auto renderExpression = [&](float preserve, float formantShift, bool vowel)
        {
            JerzyAutoTuneAudioProcessor p;
            for(auto* id : {"gateEnabled","noiseEnabled","deEsserEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled","limiterEnabled"}) setParameter(p,id,0);
            setParameter(p,"engineMode",1); setParameter(p,"scale",0); setParameter(p,"speed",0);
            setParameter(p,"vibratoPreserve",preserve); setParameter(p,"formantShift",formantShift);
            setParameter(p,"formantEnabled",vowel ? 1.f : 0.f);
            p.prepareToPlay(48000,256);
            std::vector<float> audio; audio.reserve(72000);
            double expressionPhase=0;
            for(int block=0;block<282;++block)
            {
                for(int i=0;i<256;++i)
                {
                    float x=0;
                    if(vowel)
                    {
                        const double t=(block*256+i)/48000.;
                        for(int h=1;h<=60;++h)
                        {
                            const double f=110.*h;
                            const double envelope=.01+std::exp(-std::pow((f-700)/190,2))+.6*std::exp(-std::pow((f-1800)/280,2));
                            x+=static_cast<float>(.025*envelope*std::sin(juce::MathConstants<double>::twoPi*f*t));
                        }
                    }
                    else
                    {
                        const double t=(block*256+i)/48000.;
                        const double hz=447.*std::pow(2.,.22*std::sin(juce::MathConstants<double>::twoPi*5*t)/12.);
                        expressionPhase+=juce::MathConstants<double>::twoPi*hz/48000.;
                        x=.2f*static_cast<float>(std::sin(expressionPhase));
                    }
                    buffer.setSample(0,i,x); buffer.setSample(1,i,x);
                }
                p.processBlock(buffer,midi);
                for(int i=0;i<256;++i) audio.push_back(buffer.getSample(0,i));
            }
            return audio;
        };
        const auto modulationDepth = [](const std::vector<float>& audio)
        {
            std::vector<double> crossings;
            for(size_t i=36001;i<audio.size();++i)
                if(audio[i-1]<=0 && audio[i]>0) crossings.push_back(static_cast<double>(i)-1-audio[i-1]/(audio[i]-audio[i-1]));
            require(crossings.size()>200,"Vibrato output is not periodic");
            double sum=0,squares=0;
            for(size_t i=1;i<crossings.size();++i)
            {
                const double cents=1200*std::log2(48000./((crossings[i]-crossings[i-1])*440.));
                sum+=cents; squares+=cents*cents;
            }
            const double n=static_cast<double>(crossings.size()-1);
            return std::sqrt(squares/n-std::pow(sum/n,2));
        };
        const double flatDepth=modulationDepth(renderExpression(0,0,false));
        const double expressiveDepth=modulationDepth(renderExpression(100,0,false));
        std::cout<<"Vibrato output deviation: full correction="<<flatDepth<<" cents, preserve="<<expressiveDepth<<" cents\n";
        require(expressiveDepth>flatDepth*1.1,"Vibrato Preserve does not retain more expression at output");
        const auto spectralCentroid = [](const std::vector<float>& audio)
        {
            constexpr int order=14,size=1<<order;
            juce::dsp::FFT fft(order);
            std::vector<float> spectrum(size*2,0);
            for(int i=0;i<size;++i) spectrum[static_cast<size_t>(i)]=audio[audio.size()-size+static_cast<size_t>(i)]*(.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/(size-1)));
            fft.performFrequencyOnlyForwardTransform(spectrum.data());
            double weighted=0,energy=0;
            for(int i=1;i<size/2;++i) { const double e=spectrum[static_cast<size_t>(i)]*spectrum[static_cast<size_t>(i)]; energy+=e; weighted+=e*i*48000./size; }
            require(energy>1.e-6,"Formant test is silent"); return weighted/energy;
        };
        const double lowFormant=spectralCentroid(renderExpression(100,-5,true));
        const double highFormant=spectralCentroid(renderExpression(100,5,true));
        std::cout<<"Formant spectral centroid: down="<<lowFormant<<" Hz, up="<<highFormant<<" Hz\n";
        require(highFormant>lowFormant*1.2,"Formant Shift does not move the spectral envelope");
        // Bypass ramps must reach the clean signal without an instantaneous gain step.
        for(auto* module : {"gateEnabled","noiseEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled"})
        {
            JerzyAutoTuneAudioProcessor p;
            for(auto* id : {"gateEnabled","noiseEnabled","deEsserEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled","limiterEnabled"}) setParameter(p,id,0);
            setParameter(p,"mix",0); setParameter(p,"engineMode",1); setParameter(p,module,1);
            setParameter(p,"gateThreshold",-10); setParameter(p,"gateRange",80);
            setParameter(p,"satDrive",24); setParameter(p,"satMix",100); setParameter(p,"satOutput",-12);
            setParameter(p,"doublerAmount",100); setParameter(p,"compThreshold",-48); setParameter(p,"compRatio",20);
            p.prepareToPlay(48000,256);
            const auto dcBlock=[&]() { for(int ch=0;ch<2;++ch) for(int i=0;i<256;++i) buffer.setSample(ch,i,.1f); p.processBlock(buffer,midi); };
            for(int block=0;block<160;++block) dcBlock();
            const float before=buffer.getSample(0,255);
            setParameter(p,module,0); dcBlock();
            require(std::abs(buffer.getSample(0,0)-before)<.002f,"Instantaneous module bypass step");
            for(int block=0;block<80;++block) dcBlock();
            require(std::abs(buffer.getSample(0,255)-.1f)<1.e-5f,"Bypass does not reach dry signal");
        }
        for(int channelCount : {1,2})
            for(int blockSize : {1,17,64,1024})
            {
                JerzyAutoTuneAudioProcessor p;
                auto layout=p.getBusesLayout();
                const auto set=channelCount==1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
                layout.inputBuses.set(0,set); layout.outputBuses.set(0,set);
                require(p.setBusesLayout(layout),"Supported channel layout rejected");
                for(auto* id : {"gateEnabled","noiseEnabled","deEsserEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled","limiterEnabled"}) setParameter(p,id,0);
                setParameter(p,"mix",0); setParameter(p,"engineMode",1);
                p.prepareToPlay(48000,64); // The host may exceed its earlier block-size estimate.
                juce::AudioBuffer<float> variable(channelCount,blockSize);
                int found=-1;
                for(int block=0;block<p.getLatencySamples()/blockSize+3;++block)
                {
                    variable.clear(); if(block==0) for(int ch=0;ch<channelCount;++ch) variable.setSample(ch,0,.25f);
                    p.processBlock(variable,midi);
                    for(int i=0;i<blockSize;++i)
                    {
                        require(std::isfinite(variable.getSample(0,i)),"Nonfinite variable-block output");
                        if(std::abs(variable.getSample(0,i))>1.e-7f) { require(found<0,"Variable-block dry impulse smeared"); found=block*blockSize+i; }
                    }
                }
                require(found==p.getLatencySamples(),"Variable-block/mono latency mismatch");
            }
        runStudioTests();
        std::cout << "PASS: studio parameter controls, advanced tuning/ceiling/latency, automation, scaling, hit targets, state recall, editor lifecycle, audio and note mask\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
