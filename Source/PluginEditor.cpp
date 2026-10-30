#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
const juce::Colour black(17,18,19),panel(25,26,27),raised(40,41,42),silver(166,163,155),white(230,226,217),muted(145,144,139),red(236,48,43),green(54,213,94);
const char* pages[]={"TUNE","CLEAN","DYNAMICS","TONE","SPACE","HARMONY","VOICE"};
const char* pagePower[]={"tuneEnabled","cleanEnabled","compEnabled","eqEnabled","reverbEnabled","harmonyEnabled","voiceEnabled"};
const char* noteIds[]={"noteC","noteCs","noteD","noteDs","noteE","noteF","noteFs","noteG","noteGs","noteA","noteAs","noteB"};
const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float size=14,juce::Colour c=white,juce::Justification align=juce::Justification::centredLeft)
{
    g.setColour(c);g.setFont(juce::FontOptions(size));g.drawText(value,r.toNearestInt(),align,true);
}
void led(juce::Graphics& g,juce::Point<float> p,bool on)
{
    g.setColour((on?green:red).withAlpha(.13f));g.fillEllipse(p.x-7,p.y-7,14,14);
    g.setColour(on?green:red.darker(.12f));g.fillEllipse(p.x-4,p.y-4,8,8);
    g.setColour(juce::Colours::white.withAlpha(.45f));g.fillEllipse(p.x-2,p.y-2,2,2);
}
float frequencyX(float hz,juce::Rectangle<float> r){return r.getX()+r.getWidth()*std::log(std::clamp(hz,20.f,20000.f)/20.f)/std::log(1000.f);}
float xFrequency(float x,juce::Rectangle<float> r){return 20.f*std::pow(1000.f,juce::jlimit(0.f,1.f,(x-r.getX())/r.getWidth()));}
float dbY(float value,juce::Rectangle<float> r,float low=-72,float high=0){return r.getBottom()-(value-low)/(high-low)*r.getHeight();}
void grid(juce::Graphics& g,juce::Rectangle<float> r)
{
    g.setColour(black);g.fillRoundedRectangle(r,4);
    for(float hz:{20.f,50.f,100.f,200.f,500.f,1000.f,2000.f,5000.f,10000.f,20000.f})
    {
        const float x=frequencyX(hz,r);g.setColour(raised);g.drawVerticalLine(int(x),r.getY(),r.getBottom());
        text(g,hz>=1000?juce::String(hz/1000,0)+"k":juce::String(hz,0),{x-19,r.getBottom()+3,38,18},11,muted,juce::Justification::centred);
    }
    for(int i=0;i<5;++i){g.setColour(raised);g.drawHorizontalLine(int(r.getY()+r.getHeight()*i/4),r.getX(),r.getRight());}
}
std::vector<juce::String> originalsForPage(int page)
{
    std::vector<juce::String> result;
    const char* lists[]={
        "key scale noteC noteCs noteD noteDs noteE noteF noteFs noteG noteGs noteA noteAs noteB speed amount engineMode vibratoPreserve noteStability referenceHz formantEnabled formantShift transpose",
        "gateEnabled gateThreshold gateRange gateAttack gateHold gateRelease noiseEnabled noiseThreshold noiseReduction noiseRelease noiseHighPass deEsserEnabled deEsserThreshold deEsserFreq deEsserAmount deEsserRelease",
        "compEnabled compThreshold compRatio compAttack compRelease compKnee compMakeup",
        "eqEnabled eqLowCut eqLow eqMidFreq eqMid eqMidQ eqHigh eqHighCut satEnabled satDrive satTone satMix satOutput doublerEnabled doublerAmount doublerDelay doublerDetune doublerWidth",
        "","","formantEnabled formantShift transpose"
    };
    const auto tokens=juce::StringArray::fromTokens(lists[page]," ","");for(const auto& id:tokens)result.push_back(id);return result;
}
std::vector<juce::String> primaryForPage(int page)
{
    const char* lists[]={
        "tuneEnabled speed amount vibratoPreserve formantShift engineMode key scale referenceHz transpose noteStability formantEnabled",
        "cleanEnabled cleanReduction cleanSmooth cleanMix cleanAudition gateEnabled gateThreshold deEsserEnabled deEsserAmount deEsserHard noiseEnabled noiseReduction",
        "compEnabled compStyle compThreshold compRatio compAttack compRelease comp2Enabled comp2Style comp2Threshold comp2Ratio comp2Attack comp2Release",
        "eqEnabled eqMidFreq eqMid eqMidQ eqLowCut eqHighCut eqLow eqHigh satEnabled satDrive satMix satModel",
        "reverbEnabled reverbType reverbDecay reverbPreDelay reverbDamping reverbWidth reverbMix spaceDucking delayEnabled delayDivision delayFeedback delayMix",
        "harmonyEnabled harmonyMode harmonyMix harmonyHumanize",
        "voiceEnabled formantShift transpose voiceAir voiceColour voiceCharacter voiceMix formantEnabled"
    };
    std::vector<juce::String> result;for(const auto& id:juce::StringArray::fromTokens(lists[page]," ",""))result.push_back(id);return result;
}
}

JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::StudioLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId,black);setColour(juce::ComboBox::textColourId,white);setColour(juce::ComboBox::outlineColourId,silver.withAlpha(.4f));
    setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,white);setColour(juce::PopupMenu::highlightedBackgroundColourId,raised);
    setColour(juce::Slider::textBoxTextColourId,white);setColour(juce::Slider::textBoxBackgroundColourId,black);setColour(juce::Slider::textBoxOutlineColourId,raised);
    setColour(juce::Label::textColourId,white);setColour(juce::TextButton::textColourOffId,white);setColour(juce::TextButton::textColourOnId,white);
}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getTextButtonFont(juce::TextButton&,int){return juce::FontOptions(13.f);}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getComboBoxFont(juce::ComboBox&){return juce::FontOptions(14.f);}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getLabelFont(juce::Label& l){return l.getFont();}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down)
{
    const auto r=b.getLocalBounds().toFloat().reduced(1);g.setColour(b.getToggleState()?raised:panel);g.fillRoundedRectangle(r,4);
    g.setColour((over||b.getToggleState()?silver:silver.withAlpha(.3f)));g.drawRoundedRectangle(r,4,down?2.f:1.f);
    if(b.getProperties().contains("moduleState"))led(g,{10,r.getCentreY()},bool(b.getProperties()["moduleState"]));
    if(b.getComponentID()=="resetOver"&&b.getToggleState()){g.setColour(red);g.drawRoundedRectangle(r,4,2);}
}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool over,bool)
{
    const auto r=b.getLocalBounds().toFloat().reduced(1);g.setColour(over?raised:black);g.fillRoundedRectangle(r,3);
    g.setColour(silver.withAlpha(.4f));g.drawRoundedRectangle(r,3,1);
    const bool on=b.getToggleState();led(g,{r.getX()+12,r.getCentreY()},on);
    const auto label=b.getComponentID().startsWith("note")&&b.getButtonText().length()<=2?b.getButtonText():(b.getComponentID()=="masterEnabled"?"MASTER ":juce::String())+(on?"ON":"OFF");
    text(g,label,r.withTrimmedLeft(24),13,white,juce::Justification::centred);
}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider& slider)
{
    const float diameter=juce::jmax(16.f,float(juce::jmin(w,h))-12.f),radius=diameter*.5f;
    const juce::Point<float> c(float(x)+w*.5f,float(y)+h*.5f);
    g.setColour(juce::Colours::black.withAlpha(.6f));g.fillEllipse(c.x-radius+2,c.y-radius+3,diameter,diameter);
    g.setColour(silver.darker(.35f));g.fillEllipse(c.x-radius,c.y-radius,diameter,diameter);
    g.setColour(black);g.fillEllipse(c.x-radius+3,c.y-radius+3,diameter-6,diameter-6);
    g.setColour(raised);g.drawEllipse(c.x-radius+5,c.y-radius+5,diameter-10,diameter-10,1);
    const float angle=start+value*(end-start);
    g.setColour(white);g.drawLine(c.x+std::sin(angle)*radius*.42f,c.y-std::cos(angle)*radius*.42f,c.x+std::sin(angle)*radius*.78f,c.y-std::cos(angle)*radius*.78f,2.5f);
    juce::Path arc;arc.addCentredArc(c.x,c.y,radius+3,radius+3,0,start,end,true);g.setColour(silver.withAlpha(.7f));g.strokePath(arc,juce::PathStrokeType(1.2f));
    if(slider.getMinimum()<=0&&slider.getMaximum()>=0)
    {
        const float zero=float(slider.valueToProportionOfLength(0));const float z=start+zero*(end-start);
        g.setColour(red);g.drawLine(c.x+std::sin(z)*(radius+1),c.y-std::cos(z)*(radius+1),c.x+std::sin(z)*(radius+8),c.y-std::cos(z)*(radius+8),2);
    }
}
struct JerzyAutoTuneAudioProcessorEditor::ParameterControl final : juce::Component
{
    JerzyAutoTuneAudioProcessor& processor;juce::String id;juce::Label name;juce::Slider slider;juce::ComboBox choice;juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    bool isNote=false,inspector=false;
    ParameterControl(JerzyAutoTuneAudioProcessor& p,const juce::String& parameterId):processor(p),id(parameterId)
    {
        auto* parameter=processor.parameters.getParameter(id);jassert(parameter!=nullptr);
        name.setText(parameter->getName(40),juce::dontSendNotification);name.setJustificationType(juce::Justification::centred);name.setFont(juce::FontOptions(12.5f));name.setBorderSize({});addAndMakeVisible(name);
        if(auto* c=dynamic_cast<juce::AudioParameterChoice*>(parameter))
        {
            choice.addItemList(c->choices,1);choice.setComponentID(id);choice.setTooltip(parameter->getName(100));addAndMakeVisible(choice);ca=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,id,choice);
        }
        else if(dynamic_cast<juce::AudioParameterBool*>(parameter))
        {
            isNote=id.startsWith("note")&&parameter->getName(10).length()<=2;
            toggle.setButtonText(isNote?parameter->getName(10):"ON");toggle.setComponentID(id);toggle.setTooltip(parameter->getName(100));addAndMakeVisible(toggle);ba=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,id,toggle);
        }
        else
        {
            slider.setComponentID(id);slider.setName(parameter->getName(100));slider.setTooltip(parameter->getName(100)+". Double-click: default. Shift-drag: fine adjustment. Red tick: zero reference.");
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,22);slider.setScrollWheelEnabled(false);slider.setMouseDragSensitivity(200);
            slider.setDoubleClickReturnValue(true,parameter->convertFrom0to1(parameter->getDefaultValue()));slider.setTextValueSuffix(" "+parameter->getLabel());
            slider.onDragStart=[this]{processor.undoManager.beginNewTransaction("Adjust "+id);};
            addAndMakeVisible(slider);sa=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,id,slider);
        }
    }
    void resized() override
    {
        auto r=getLocalBounds();if(isNote||id=="masterEnabled"){name.setVisible(false);toggle.setBounds(r);return;}
        name.setBounds(r.removeFromTop(19));r.reduce(4,0);
        if(ca)choice.setBounds(r.withHeight(32).withY(r.getY()+8));
        else if(ba)toggle.setBounds(r.withHeight(32).withY(r.getY()+8));
        else {slider.setBounds(r);slider.setTextBoxStyle(inspector?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,inspector?80:juce::jmax(44,r.getWidth()-4),22);}
    }
};

JerzyAutoTuneAudioProcessorEditor::JerzyAutoTuneAudioProcessorEditor(JerzyAutoTuneAudioProcessor& p):AudioProcessorEditor(&p),processor(p)
{
    const int savedWidth=processor.editorWidth.load(),savedHeight=processor.editorHeight.load();
    setLookAndFeel(&look);setOpaque(true);addAndMakeVisible(surface);surface.setSize(1280,800);
    steel=juce::ImageCache::getFromMemory(BinaryData::steel_texture_png,BinaryData::steel_texture_pngSize);
    for(int i=0;i<7;++i)
    {
        auto& button=navigation[size_t(i)];button.setButtonText(pages[i]);button.setComponentID("page"+juce::String(i));button.setTooltip("Open "+juce::String(pages[i]));button.onClick=[this,i]{showPage(i,false);};surface.addAndMakeVisible(button);
    }
    presets.addItemList({"Natural Lead","Tight Rap","Soft Dialogue","Wide Chorus","Telephone","Airy Plate"},1);presets.setSelectedId(1,juce::dontSendNotification);presets.setComponentID("factoryPreset");
    presets.onChange=[this]{processor.applyFactoryPreset(presets.getSelectedId()-1);};surface.addAndMakeVisible(presets);
    for(int percent:{75,90,100,125,150})zoom.addItem(juce::String(percent)+"%",percent);
    zoom.setComponentID("uiZoom");zoom.onChange=[this]{const int n=zoom.getSelectedId();if(n>0)setSize(1280*n/100,800*n/100);};surface.addAndMakeVisible(zoom);
    compare.setComponentID("compareAB");compareState=processor.parameters.copyState();compare.onClick=[this]{auto current=processor.parameters.copyState();processor.parameters.replaceState(compareState);compareState=current;compare.setToggleState(!compare.getToggleState(),juce::dontSendNotification);};
    undo.setComponentID("undo");undo.onClick=[this]{processor.undoManager.undo();};
    advancedButton.setComponentID("allControls");advancedButton.onClick=[this]{showPage(page,!expanded);};
    learnNoise.setComponentID("learnNoise");learnNoise.onClick=[this]{processor.learnNoise();};
    learnKey.setComponentID("learnKey");learnKey.onClick=[this]{processor.learnKey();};
    applyKey.setComponentID("applyKey");applyKey.onClick=[this]{processor.applySuggestedKey();};
    resetPeak.setComponentID("resetOver");resetPeak.onClick=[this]{processor.resetOverload();};
    allNotes.setComponentID("allNotes");allNotes.onClick=[this]{for(auto* id:noteIds)setReal(id,1);};
    noNotes.setComponentID("noNotes");noNotes.onClick=[this]{for(auto* id:noteIds)setReal(id,0);};
    for(auto* button:{&compare,&undo,&advancedButton,&learnNoise,&learnKey,&applyKey,&resetPeak,&allNotes,&noNotes})surface.addAndMakeVisible(button);
    for(auto* id:{"masterEnabled","outputGain","limiterCeiling","limiterEnabled","mix"}){auto* c=master.add(new ParameterControl(processor,id));surface.addAndMakeVisible(c);}
    spectrum.fill(-90);processedSpectrum.fill(-90);pitchHistory.fill(-1);targetHistory.fill(-1);reductionHistory.fill(0);stageBHistory.fill(0);
    showPage(0,false);setResizable(true,true);setResizeLimits(960,600,1920,1200);
    setSize(savedWidth,savedHeight);startTimerHz(30);
}
JerzyAutoTuneAudioProcessorEditor::~JerzyAutoTuneAudioProcessorEditor(){stopTimer();parameters.clear();master.clear();setLookAndFeel(nullptr);}
void JerzyAutoTuneAudioProcessorEditor::paint(juce::Graphics& g){g.fillAll(black);}
void JerzyAutoTuneAudioProcessorEditor::resized()
{
    const float scale=juce::jmin(getWidth()/1280.f,getHeight()/800.f);
    surface.setTransform(juce::AffineTransform::scale(scale).translated((getWidth()-1280*scale)*.5f,(getHeight()-800*scale)*.5f));
    zoom.setSelectedId(0,juce::dontSendNotification);zoom.setText(juce::String(juce::roundToInt(scale*100))+"%",juce::dontSendNotification);
    if(getWidth()>=960&&getHeight()>=600){processor.editorWidth.store(getWidth());processor.editorHeight.store(getHeight());}
}
void JerzyAutoTuneAudioProcessorEditor::showPage(int index,bool allControls)
{
    page=juce::jlimit(0,6,index);expanded=allControls;parameters.clear();
    auto ids=expanded?originalsForPage(page):primaryForPage(page);
    if(expanded||page==5)for(const auto& spec:jerzy::studio::parameterSpecs())if(spec.page==page&&std::find(ids.begin(),ids.end(),spec.id)==ids.end())ids.push_back(spec.id);
    if(page==0)for(auto* id:noteIds)if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);
    for(const auto& id:ids){auto* c=parameters.add(new ParameterControl(processor,id));surface.addAndMakeVisible(c);}
    for(int i=0;i<7;++i)navigation[size_t(i)].setToggleState(i==page,juce::dontSendNotification);
    advancedButton.setButtonText(expanded?"FOCUS VIEW":"ALL CONTROLS");advancedButton.setToggleState(expanded,juce::dontSendNotification);
    layoutControls();timerCallback();surface.repaint();
}
JerzyAutoTuneAudioProcessorEditor::ParameterControl* JerzyAutoTuneAudioProcessorEditor::control(const juce::String& id) const
{
    for(auto* c:parameters)if(c->id==id)return c;for(auto* c:master)if(c->id==id)return c;return nullptr;
}
void JerzyAutoTuneAudioProcessorEditor::layoutControls()
{
    for(int i=0;i<7;++i)navigation[size_t(i)].setBounds(15,126+i*86,100,72);
    presets.setBounds(360,18,205,32);compare.setBounds(578,18,60,32);undo.setBounds(649,18,66,32);zoom.setBounds(1040,18,83,32);
    advancedButton.setBounds(969,88,180,30);learnNoise.setVisible(page==1);learnNoise.setBounds(145,126,150,30);
    learnKey.setVisible(page==0);learnKey.setBounds(145,126,142,30);applyKey.setVisible(page==0);applyKey.setBounds(297,126,128,30);
    allNotes.setVisible(page==0&&!expanded);noNotes.setVisible(page==0&&!expanded);allNotes.setBounds(775,690,95,35);noNotes.setBounds(880,690,105,35);
    resetPeak.setBounds(1168,89,96,32);
    control("masterEnabled")->setBounds(1158,9,112,65);control("outputGain")->setBounds(1167,455,100,113);control("limiterCeiling")->setBounds(1167,575,100,110);control("limiterEnabled")->setBounds(1167,696,100,65);control("mix")->setBounds(1038,656,110,105);
    control("mix")->setVisible(page==0);
    if(expanded)
    {
        const int count=parameters.size();const int columns=8,rows=(count+columns-1)/columns;
        const int height=juce::jmin(102,344/juce::jmax(1,rows));
        for(int i=0;i<count;++i)parameters[i]->setBounds(139+(i%columns)*126,402+(i/columns)*height,121,height-2);
        control("mix")->setBounds(993,185,152,130);return;
    }
    if(page==5)
    {
        const auto place=[this](const juce::String& id,int x,int y,int w,int h){if(auto* c=control(id))c->setBounds(x,y,w,h);};
        for(int i=0;i<4;++i)place(juce::StringArray{"harmonyEnabled","harmonyMode","harmonyMix","harmonyHumanize"}[i],142+i*245,140,225,95);
        const char* suffix[]={"Enabled","Interval","Level","Pan","Formant"};
        for(int v=0;v<4;++v)for(int k=0;k<5;++k)place("harmony"+juce::String(v+1)+suffix[k],260+k*172,247+v*90,160,86);
        int i=0;for(auto* id:{"choirEnabled","choirVoices","choirPitch","choirTiming","choirVibrato","choirWidth","choirMix"})place(id,140+(i++)*139,649,132,90);
        control("mix")->setBounds(1038,738,110,56);return;
    }
    auto ids=primaryForPage(page);
    if(page==2)
    {
        for(int stage=0;stage<2;++stage)for(int i=0;i<6;++i)
            if(auto* c=control(ids[size_t(stage*6+i)]))c->setBounds(143+stage*510+(i%3)*166,460+(i/3)*125,158,115);
    }
    else
    {
        int bottom=0;
        for(size_t i=0;i<ids.size();++i)
        {
            auto* c=control(ids[i]);if(!c)continue;
            const bool inspector=page==0?(ids[i]=="speed"||ids[i]=="amount"||ids[i]=="vibratoPreserve"||ids[i]=="formantShift"):i<4;
            if(inspector)
            {
                int row=page==0?(ids[i]=="speed"?0:ids[i]=="amount"?1:ids[i]=="vibratoPreserve"?2:3):int(i);
                c->inspector=true;c->setBounds(973,160+row*94,174,90);
            }
            else c->setBounds(142+(bottom%8)*126,550+(bottom/8)*100,121,94),++bottom;
        }
    }
    if(page==0)
    {
        for(int i=0;i<12;++i)if(auto* c=control(noteIds[i]))c->setBounds(142+i*51,686,47,46);
    }
}
juce::Rectangle<float> JerzyAutoTuneAudioProcessorEditor::plotBounds() const
{
    if(expanded)return {150,166,815,195};
    if(page==2)return {150,166,990,248};
    return {150,166,799,335};
}
void JerzyAutoTuneAudioProcessorEditor::drawSpectrum(juce::Graphics& g,juce::Rectangle<float> r,bool showOutput)
{
    grid(g,r);const float fs=processor.graphRate.load();
    for(int pass=0;pass<(showOutput?2:1);++pass)
    {
        juce::Path path;bool first=true;
        for(int x=0;x<int(r.getWidth());++x)
        {
            const float hz=xFrequency(r.getX()+float(x),r);const int bin=juce::jlimit(1,1024,int(hz*2048/fs));
            const float y=juce::jlimit(r.getY(),r.getBottom(),dbY(pass==0?spectrum[size_t(bin)]:processedSpectrum[size_t(bin)],r));
            if(first){path.startNewSubPath(r.getX()+float(x),r.getBottom());path.lineTo(r.getX()+float(x),y);first=false;}else path.lineTo(r.getX()+float(x),y);
        }
        auto fill=path;fill.lineTo(r.getRight(),r.getBottom());fill.closeSubPath();g.setColour((pass==0?silver:white).withAlpha(pass==0?.15f:.06f));g.fillPath(fill);
        g.setColour(pass==0?silver:white);g.strokePath(path,juce::PathStrokeType(pass==0?1.f:1.6f));
    }
}
void JerzyAutoTuneAudioProcessorEditor::paintSurface(juce::Graphics& g)
{
    g.fillAll(black);if(steel.isValid()){g.setOpacity(.38f);g.drawImage(steel,surface.getLocalBounds().toFloat());g.setOpacity(1);}
    g.setColour(silver.darker(.6f));g.drawRoundedRectangle(surface.getLocalBounds().toFloat().reduced(3),9,3);
    g.setColour(silver);g.drawRoundedRectangle(surface.getLocalBounds().toFloat().reduced(5),8,1.5f);
    g.setColour(black);g.drawRoundedRectangle(surface.getLocalBounds().toFloat().reduced(8),6,1);
    g.setColour(panel.withAlpha(.97f));g.fillRoundedRectangle(125,78,1035,683,5);
    g.setColour(black.withAlpha(.96f));g.fillRoundedRectangle(1165,78,105,683,4);
    text(g,"JERZY",{20,15,100,30},25);text(g,"VOCAL STUDIO",{122,18,225,27},20,silver);
    const double rate=processor.graphRate.load();const int latency=processor.getLatencySamples();
    text(g,latency>0?"STUDIO  /  "+juce::String(1000.0*latency/rate,1)+" ms":"STUDIO  /  -- ms",{745,18,280,30},14,silver);
    text(g,juce::String(pages[page])+"  /  "+juce::StringArray{"PITCH CORRECTION","SPECTRAL PREP + DE-ESS","DUAL-STAGE COMPRESSION","VOCAL EQ + COLOUR","REVERB + TEMPO DELAY","HARMONY + UNISON CHOIR","FORMANTS + BREATH"}[page],{145,87,810,28},17);
    const auto r=plotBounds();
    if(page==0)
    {
        g.setColour(black);g.fillRoundedRectangle(r,4);
        const float pitch=processor.getDetectedPitch();const int centre=pitch>=0?int(std::round(pitch)):60;
        for(int note=centre-6;note<=centre+6;++note)
        {
            const float y=r.getCentreY()-(note-centre)*r.getHeight()/12;
            if(y<r.getY()||y>r.getBottom())continue;
            g.setColour(raised);g.drawHorizontalLine(int(y),r.getX(),r.getRight());
            text(g,juce::String(noteNames[(note%12+12)%12])+juce::String(note/12-1),{r.getX()+5,y-17,40,16},11,muted);
        }
        for(int i=0;i<9;++i){g.setColour(raised);g.drawVerticalLine(int(r.getX()+r.getWidth()*i/8),r.getY(),r.getBottom());}
        for(int pass=0;pass<2;++pass)
        {
            juce::Path path;bool pen=false;
            for(size_t i=0;i<pitchHistory.size();++i)
            {
                const size_t index=(historyWrite+i)%pitchHistory.size();const float v=pass==0?targetHistory[index]:pitchHistory[index];
                if(v<0){pen=false;continue;}const float x=r.getX()+float(i)*r.getWidth()/float(pitchHistory.size()-1),y=juce::jlimit(r.getY(),r.getBottom(),r.getCentreY()-(v-centre)*r.getHeight()/12);
                if(pen)path.lineTo(x,y);else path.startNewSubPath(x,y);pen=true;
            }
            g.setColour(pass==0?silver.withAlpha(.55f):white);g.strokePath(path,juce::PathStrokeType(pass==0?3.f:1.5f));
        }
        const int note=juce::roundToInt(pitch);text(g,pitch<0?"--":juce::String(noteNames[(note%12+12)%12])+juce::String(note/12-1),{800,121,105,39},30);
        text(g,juce::String(processor.getCorrectionCents(),0)+" ct",{902,125,95,30},17,silver);
        const int key=processor.getSuggestedKey();
        text(g,key<0?"Analyse a phrase to suggest key":juce::String("Suggestion: ")+noteNames[key]+(processor.getSuggestedMinor()?" minor":" major")+" / "+juce::String(processor.getKeyConfidence()*100,0)+"%",{439,126,355,28},13,muted);
        text(g,"INPUT PITCH  /  TARGET NOTE     Confidence "+juce::String(processor.getPitchConfidence()*100,0)+"%",{150,510,720,22},13,silver);
    }
    else if(page==1||page==3||page==6)
    {
        drawSpectrum(g,r,page!=3);
        if(page==1)text(g,processor.getNoiseProgress()>0&&processor.getNoiseProgress()<1?"Learning noise: "+juce::String(processor.getNoiseProgress()*100,0)+"%":processor.hasNoiseProfile()?"Noise profile learned — "+juce::String(processor.getNoiseReduction(),1)+" dB spectral attenuation":"Play noise alone, then LEARN NOISE (1 second)",{315,126,630,28},13,silver);
        if(page==6)text(g,"Estimated harmonic / aspiration shaping · use your ears to protect consonants",{150,126,790,26},13,muted);
        if(page==3)
        {
            juce::Path curve;
            for(int x=0;x<int(r.getWidth());++x)
            {
                const float hz=xFrequency(r.getX()+float(x),r),gain=processor.eqResponse(hz);
                const float y=juce::jlimit(r.getY(),r.getBottom(),r.getCentreY()-gain*r.getHeight()/36);
                if(x==0)curve.startNewSubPath(r.getX(),y);else curve.lineTo(r.getX()+float(x),y);
            }
            g.setColour(white);g.strokePath(curve,juce::PathStrokeType(2));
            const auto value=[this](const char* id){return processor.parameters.getRawParameterValue(id)->load();};
            const float hz[]={value("eqLowCut"),160,value("eqMidFreq"),10000,value("eqHighCut")};
            const float gain[]={0,value("eqLow"),value("eqMid"),value("eqHigh"),0};
            for(int i=0;i<5;++i)
            {
                const auto p=juce::Point<float>(frequencyX(hz[i],r),r.getCentreY()-gain[i]*r.getHeight()/36);
                g.setColour(raised);g.fillEllipse(p.x-8,p.y-8,16,16);g.setColour(i==dragBand?white:silver);g.drawEllipse(p.x-8,p.y-8,16,16,1.5f);text(g,juce::String(i+1),{p.x-7,p.y-8,14,16},11,white,juce::Justification::centred);
            }
        }
    }
    else if(page==2)
    {
        for(int stage=0;stage<2;++stage)
        {
            auto chart=r.withWidth((r.getWidth()-30)*.5f).translated(stage*(r.getWidth()+30)*.5f,0);g.setColour(black);g.fillRoundedRectangle(chart,4);
            const char* thresholdId=stage==0?"compThreshold":"comp2Threshold";const char* ratioId=stage==0?"compRatio":"comp2Ratio";
            const float threshold=processor.parameters.getRawParameterValue(thresholdId)->load(),ratio=processor.parameters.getRawParameterValue(ratioId)->load();
            for(int i=0;i<5;++i){g.setColour(raised);g.drawVerticalLine(int(chart.getX()+chart.getWidth()*i/4),chart.getY(),chart.getBottom());g.drawHorizontalLine(int(chart.getY()+chart.getHeight()*i/4),chart.getX(),chart.getRight());}
            juce::Path path;for(int i=0;i<=100;++i){const float level=-60+.6f*i,out=level>threshold?threshold+(level-threshold)/ratio:level;const float x=chart.getX()+i*chart.getWidth()/100,y=dbY(out,chart,-60,0);if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
            const bool on=processor.parameters.getRawParameterValue(stage==0?"compEnabled":"comp2Enabled")->load()>.5f;
            g.setColour(on?white:muted.withAlpha(.4f));g.strokePath(path,juce::PathStrokeType(2));led(g,{chart.getX()+12,chart.getY()+16},on);
            text(g,juce::String(stage==0?"STAGE A":"STAGE B")+"   GR "+juce::String(stage==0?processor.getCompressorReduction():processor.getStageBReduction(),1)+" dB",{chart.getX()+27,chart.getY()+5,chart.getWidth()-30,24},14);
        }
        text(g,"Stereo-linked detection · sidechain and Warm controls in ALL CONTROLS",{150,425,950,25},13,muted);
    }
    else if(page==4)
    {
        g.setColour(black);g.fillRect(r);const float decay=processor.parameters.getRawParameterValue("reverbDecay")->load(),pre=processor.parameters.getRawParameterValue("reverbPreDelay")->load()*.001f;
        for(int i=0;i<=4;++i){const float x=r.getX()+r.getWidth()*i/4;g.setColour(raised);g.drawVerticalLine(int(x),r.getY(),r.getBottom());text(g,juce::String((decay*1.2f+pre)*i/4,2)+" s",{x-25,r.getBottom()+2,60,18},11,muted);}
        for(int i=0;i<=4;++i){const float y=r.getY()+r.getHeight()*i/4;g.setColour(raised);g.drawHorizontalLine(int(y),r.getX(),r.getRight());text(g,juce::String(-18*i)+" dB",{r.getX()+4,y,70,18},11,muted);}
        juce::Path path;for(int i=0;i<240;++i){const float t=float(i)*(decay*1.2f+pre)/239;const float value=t<pre?-72.f:-60.f*(t-pre)/decay;const float x=r.getX()+r.getWidth()*i/239,y=juce::jlimit(r.getY(),r.getBottom(),dbY(value,r));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(white);g.strokePath(path,juce::PathStrokeType(2));
        text(g,"DECAY TARGET  -60 dB / "+juce::String(decay,2)+" s   ·   Host tempo "+juce::String(processor.getTempo(),1)+" BPM",{150,126,800,28},14,silver);
        text(g,"Algorithmic room / plate-style / hall · parallel sends · vocal ducking",{150,510,790,24},13,muted);
    }
    else if(page==5)
    {
        for(int v=0;v<4;++v)text(g,"VOICE "+juce::String(v+1),{145,float(270+v*90),100,28},15,silver);
    }
    g.setColour(silver.withAlpha(.3f));g.drawHorizontalLine(769,125,1267);
    text(g,"PREP  >  TUNE  +  HARMONY  >  DYNAMICS  >  TONE  >  CHOIR / SPACE  >  LIMIT",{145,771,910,22},11,muted);
    const juce::Rectangle<float> meterArea(1176,147,80,287);
    for(int ch=0;ch<2;++ch)
    {
        const float x=meterArea.getX()+ch*43;const float level=ch==0?inputDb:outputDb;
        text(g,ch==0?"IN":"OUT",{x-6,127,36,20},12,silver,juce::Justification::centred);
        g.setColour(raised);g.fillRect(x,meterArea.getY(),13.f,meterArea.getHeight());
        const float height=meterArea.getHeight()*juce::jlimit(0.f,1.f,(level+60)/60);g.setColour(white);g.fillRect(x,meterArea.getBottom()-height,13.f,height);
        g.setColour(red);g.fillRect(x-2,meterArea.getY(),17.f,2.f);
        const float peak=ch==0?processor.getInputOverload():processor.getOutputOverload();if(peak>=1){g.setColour(red);g.fillRect(x-2,meterArea.getY()-5,17.f,5.f);}
        text(g,level<=-59?"-inf":juce::String(level,1),{x-10,437,42,18},11,silver,juce::Justification::centred);
    }
}
void JerzyAutoTuneAudioProcessorEditor::timerCallback()
{
    processor.copyAnalysis(input,output);
    std::array<float,4096> data{};
    for(int pass=0;pass<2;++pass)
    {
        data.fill(0);for(size_t i=0;i<input.size();++i)data[i]=(pass==0?input[i]:output[i])*(.5f-.5f*std::cos(2*juce::MathConstants<float>::pi*float(i)/2048));
        fft.performFrequencyOnlyForwardTransform(data.data());
        auto& dest=pass==0?spectrum:processedSpectrum;
        for(size_t i=0;i<dest.size();++i){const float value=juce::Decibels::gainToDecibels(data[i]/512.f,-90.f);dest[i]=.45f*value+.55f*dest[i];}
    }
    pitchHistory[historyWrite]=processor.getDetectedPitch();targetHistory[historyWrite]=processor.getTargetNote();reductionHistory[historyWrite]=processor.getCompressorReduction();stageBHistory[historyWrite]=processor.getStageBReduction();historyWrite=(historyWrite+1)%pitchHistory.size();
    inputDb=juce::jmax(juce::Decibels::gainToDecibels(processor.getInputPeak(),-60.f),inputDb-1.5f);outputDb=juce::jmax(juce::Decibels::gainToDecibels(processor.getOutputPeak(),-60.f),outputDb-1.5f);
    const float over=juce::jmax(processor.getInputOverload(),processor.getOutputOverload());resetPeak.setToggleState(over>=1,juce::dontSendNotification);resetPeak.setButtonText(over>=1?"OVER +"+juce::String(juce::Decibels::gainToDecibels(over),1):"PEAK RESET");
    for(int i=0;i<7;++i)navigation[size_t(i)].getProperties().set("moduleState",processor.parameters.getRawParameterValue(pagePower[i])->load()>.5f);
    undo.setEnabled(processor.undoManager.canUndo());applyKey.setEnabled(processor.getSuggestedKey()>=0);surface.repaint();
}
void JerzyAutoTuneAudioProcessorEditor::setReal(const char* id,float value)
{
    auto* p=processor.parameters.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();
}
void JerzyAutoTuneAudioProcessorEditor::chartMouseDown(const juce::MouseEvent& event)
{
    if(page!=3||!plotBounds().contains(event.position))return;
    const auto r=plotBounds();const auto value=[this](const char* id){return processor.parameters.getRawParameterValue(id)->load();};
    const float hz[]={value("eqLowCut"),160,value("eqMidFreq"),10000,value("eqHighCut")},gain[]={0,value("eqLow"),value("eqMid"),value("eqHigh"),0};
    float best=24;dragBand=-1;
    for(int i=0;i<5;++i){const float distance=event.position.getDistanceFrom({frequencyX(hz[i],r),r.getCentreY()-gain[i]*r.getHeight()/36});if(distance<best){best=distance;dragBand=i;}}
    if(dragBand>=0){processor.undoManager.beginNewTransaction("Drag EQ band");for(auto* id:{"eqLowCut","eqLow","eqMidFreq","eqMid","eqHigh","eqHighCut"})processor.parameters.getParameter(id)->beginChangeGesture();chartMouseDrag(event);}
}
void JerzyAutoTuneAudioProcessorEditor::chartMouseDrag(const juce::MouseEvent& event)
{
    if(dragBand<0)return;const auto r=plotBounds();const float hz=xFrequency(event.position.x,r),gain=juce::jlimit(-12.f,12.f,(r.getCentreY()-event.position.y)*36/r.getHeight());
    const auto set=[this](const char* id,float v){auto* p=processor.parameters.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(p->convertFrom0to1(0),p->convertFrom0to1(1),v)));};
    if(dragBand==0)set("eqLowCut",hz);if(dragBand==1)set("eqLow",gain);if(dragBand==2){set("eqMidFreq",hz);set("eqMid",gain);}if(dragBand==3)set("eqHigh",gain);if(dragBand==4)set("eqHighCut",hz);surface.repaint();
}
void JerzyAutoTuneAudioProcessorEditor::chartMouseUp(const juce::MouseEvent&)
{
    if(dragBand>=0)for(auto* id:{"eqLowCut","eqLow","eqMidFreq","eqMid","eqHigh","eqHighCut"})processor.parameters.getParameter(id)->endChangeGesture();dragBand=-1;
}
void JerzyAutoTuneAudioProcessorEditor::Surface::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel)
{
    if(owner.page==3&&owner.plotBounds().contains(e.position)){const auto current=owner.processor.parameters.getRawParameterValue("eqMidQ")->load();owner.setReal("eqMidQ",juce::jlimit(.3f,6.f,current+wheel.deltaY*2));}
}
