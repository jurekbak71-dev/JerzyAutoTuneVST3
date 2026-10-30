#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
const juce::Colour black(17,18,19),panel(25,26,27),raised(40,41,42),silver(193,190,181),white(247,244,235),muted(185,185,177),red(236,48,43),green(54,213,94);
const char* pages[]={"TUNE","CLEAN","DYNAMICS","TONE","SPACE","HARMONY","VOICE"};
const char* pagePower[]={"tuneEnabled","cleanEnabled","compEnabled","eqEnabled","reverbEnabled","harmonyEnabled","voiceEnabled"};
const char* noteIds[]={"noteC","noteCs","noteD","noteDs","noteE","noteF","noteFs","noteG","noteGs","noteA","noteAs","noteB"};
const char* noteNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
juce::Font uiFont(float size,bool value=false)
{
    static const auto labels=juce::Typeface::createSystemTypefaceFor(BinaryData::Labels_ttf,BinaryData::Labels_ttfSize);
    static const auto values=juce::Typeface::createSystemTypefaceFor(BinaryData::Values_ttf,BinaryData::Values_ttfSize);
    return juce::Font(juce::FontOptions(value?values:labels).withHeight(size));
}
void hardwarePanel(juce::Graphics& g,juce::Rectangle<float> r,bool metallic=false)
{
    g.setColour(juce::Colours::black.withAlpha(.8f));g.fillRoundedRectangle(r.translated(0,3),5);
    g.setGradientFill(juce::ColourGradient(metallic?juce::Colour(78,78,73):juce::Colour(34,37,36),r.getTopLeft(),metallic?juce::Colour(24,26,25):juce::Colour(13,16,16),r.getBottomRight(),false));g.fillRoundedRectangle(r,5);
    g.setColour(metallic?silver:juce::Colour(75,79,76));g.drawRoundedRectangle(r,5,1);
    g.setColour(juce::Colours::white.withAlpha(metallic?.23f:.07f));g.drawLine(r.getX()+5,r.getY()+1,r.getRight()-5,r.getY()+1);
}
void navIcon(juce::Graphics& g,int page,juce::Rectangle<float> r)
{
    static const std::array<const char*,7> data{{BinaryData::activity_svg,BinaryData::barchart2_svg,BinaryData::activity_svg,BinaryData::sliders_svg,BinaryData::box_svg,BinaryData::users_svg,BinaryData::mic_svg}};
    static const std::array<int,7> sizes{{BinaryData::activity_svgSize,BinaryData::barchart2_svgSize,BinaryData::activity_svgSize,BinaryData::sliders_svgSize,BinaryData::box_svgSize,BinaryData::users_svgSize,BinaryData::mic_svgSize}};
    static std::array<std::unique_ptr<juce::Drawable>,7> icons;
    if(!icons[size_t(page)])icons[size_t(page)]=juce::Drawable::createFromImageData(data[size_t(page)],size_t(sizes[size_t(page)]));
    if(icons[size_t(page)])icons[size_t(page)]->drawWithin(g,r,juce::RectanglePlacement::centred,1);
}
void text(juce::Graphics& g,const juce::String& value,juce::Rectangle<float> r,float size=14,juce::Colour c=white,juce::Justification align=juce::Justification::centredLeft)
{
    g.setColour(c);g.setFont(uiFont(juce::jmax(14.f,size)));g.drawText(value,r.toNearestInt(),align,false);
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
        "tuneEnabled speed amount vibratoPreserve formantShift engineMode key scale",
        "cleanEnabled cleanReduction cleanSmooth cleanMix cleanAudition",
        "compEnabled compStyle compThreshold compRatio compAttack compRelease comp2Enabled comp2Style comp2Threshold comp2Ratio comp2Attack comp2Release",
        "eqEnabled eqMidFreq eqMid eqMidQ eqLowCut eqHighCut",
        "reverbEnabled reverbType reverbDecay reverbPreDelay reverbDamping reverbWidth reverbMix delayEnabled delayDivision delayMix",
        "harmonyEnabled harmonyMode harmonyMix harmonyHumanize",
        "voiceEnabled formantShift voiceCharacter voiceAir voiceColour voiceMix"
    };
    std::vector<juce::String> result;for(const auto& id:juce::StringArray::fromTokens(lists[page]," ",""))result.push_back(id);return result;
}
}

JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::StudioLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId,black);setColour(juce::ComboBox::textColourId,white);setColour(juce::ComboBox::outlineColourId,silver.withAlpha(.4f));
    setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,white);setColour(juce::PopupMenu::highlightedBackgroundColourId,raised);
    setColour(juce::Slider::textBoxTextColourId,white);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::thumbColourId,silver);setColour(juce::Slider::trackColourId,white);setColour(juce::Slider::backgroundColourId,juce::Colour(7,11,10));
    setColour(juce::Label::textColourId,white);setColour(juce::TextButton::textColourOffId,white);setColour(juce::TextButton::textColourOnId,white);
}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getTextButtonFont(juce::TextButton&,int){return uiFont(17.f);}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getComboBoxFont(juce::ComboBox&){return uiFont(18.f);}
juce::Font JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::getLabelFont(juce::Label& l){return l.getComponentID()=="parameterCaption"?l.getFont():uiFont(18.f,true);}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down)
{
    const auto r=b.getLocalBounds().toFloat().reduced(1);
    hardwarePanel(g,r,b.getToggleState()||over);
    if(down){g.setColour(juce::Colours::black.withAlpha(.25f));g.fillRoundedRectangle(r,4);}
    if(b.getComponentID()=="resetOver"&&b.getToggleState()){g.setColour(red.withAlpha(.23f));g.fillRoundedRectangle(r,4);g.setColour(red);g.drawRoundedRectangle(r,4,2);}
}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool)
{
    if(b.getComponentID().startsWith("page"))
    {
        const int index=b.getComponentID().substring(4).getIntValue();
        navIcon(g,index,{27,12,34,30});
        if(b.getProperties().contains("moduleState"))led(g,{13,26},bool(b.getProperties()["moduleState"]));
        text(g,b.getButtonText(),{3,47,float(b.getWidth()-6),23},17,white,juce::Justification::centred);
    }
    else text(g,b.getButtonText(),b.getLocalBounds().toFloat().reduced(5,1),17,b.isEnabled()?white:muted.withAlpha(.55f),juce::Justification::centred);
}

void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool over,bool)
{
    const auto r=b.getLocalBounds().toFloat().reduced(1);g.setColour(over?raised:black);g.fillRoundedRectangle(r,3);
    g.setColour(silver.withAlpha(.4f));g.drawRoundedRectangle(r,3,1);
    const bool on=b.getToggleState();led(g,{r.getX()+12,r.getCentreY()},on);
    if(b.getComponentID()=="masterEnabled")
    {
        static auto powerIcon=juce::Drawable::createFromImageData(BinaryData::power_svg,BinaryData::power_svgSize);
        text(g,on?"ON":"OFF",{25,0,35,float(b.getHeight())},17,white,juce::Justification::centred);
        if(powerIcon)powerIcon->drawWithin(g,{66,6,27,27},juce::RectanglePlacement::centred,1);
        return;
    }
    const auto label=b.getComponentID().startsWith("note")&&b.getButtonText().length()<=2?b.getButtonText():(b.getComponentID()=="masterEnabled"?"MASTER ":juce::String())+(on?"ON":"OFF");
    text(g,label,r.withTrimmedLeft(24),13,white,juce::Justification::centred);
}
void JerzyAutoTuneAudioProcessorEditor::StudioLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider& slider)
{
    const float diameter=juce::jmax(16.f,float(juce::jmin(w,h))-14.f),radius=diameter*.5f;
    const juce::Point<float> c(float(x)+w*.5f,float(y)+h*.5f);
    g.setColour(juce::Colours::black.withAlpha(.85f));g.fillEllipse(c.x-radius+3,c.y-radius+5,diameter+2,diameter+2);
    g.setGradientFill(juce::ColourGradient(juce::Colour(223,220,209),c.x-radius,c.y-radius,juce::Colour(68,69,65),c.x+radius,c.y+radius,false));g.fillEllipse(c.x-radius,c.y-radius,diameter,diameter);
    const float inset=3.f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(72,75,73),c.x-radius,c.y-radius,juce::Colour(12,15,15),c.x+radius,c.y+radius,false));g.fillEllipse(c.x-radius+inset,c.y-radius+inset,diameter-2*inset,diameter-2*inset);
    g.setColour(juce::Colour(11,13,12));g.drawEllipse(c.x-radius+3,c.y-radius+3,diameter-6,diameter-6,1);
    g.setColour(silver.withAlpha(.3f));g.drawEllipse(c.x-radius+5,c.y-radius+5,diameter-10,diameter-10,.7f);
    const float angle=start+value*(end-start);
    g.setColour(white);g.drawLine(c.x+std::sin(angle)*radius*.24f,c.y-std::cos(angle)*radius*.24f,c.x+std::sin(angle)*radius*.82f,c.y-std::cos(angle)*radius*.82f,3.f);
    juce::Path arc;arc.addCentredArc(c.x,c.y,radius+4,radius+4,0,start,end,true);g.setColour(silver.withAlpha(.75f));g.strokePath(arc,juce::PathStrokeType(1.3f));
    if(slider.getMinimum()<=0&&slider.getMaximum()>=0)
    {
        const float zero=float(slider.valueToProportionOfLength(0)),z=start+zero*(end-start);
        g.setColour(red);g.drawLine(c.x+std::sin(z)*(radius+1),c.y-std::cos(z)*(radius+1),c.x+std::sin(z)*(radius+9),c.y-std::cos(z)*(radius+9),2);
    }
}

struct JerzyAutoTuneAudioProcessorEditor::ParameterControl final : juce::Component
{
    JerzyAutoTuneAudioProcessor& processor;juce::String id;juce::Label name;juce::Slider slider;juce::ComboBox choice;juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    bool isNote=false,inspector=false,compact=false;
    ParameterControl(JerzyAutoTuneAudioProcessor& p,const juce::String& parameterId):processor(p),id(parameterId)
    {
        auto* parameter=processor.parameters.getParameter(id);jassert(parameter!=nullptr);
        auto label=parameter->getName(60);
        for(const auto& pair:std::array<std::pair<const char*,const char*>,8>{{{"Noise Reduction Depth","Reduction"},{"Listen Removed Noise","Reduction Only"},{"Compressor Character","Character"},{"Compressor Stage B","Stage B"},{"Sidechain High Pass","Sidechain HP"},{"Compressor","Comp"},{"Preserve Formants","Preserve Formants"},{"Spectral Cleanup","Noise Cleanup"}}})label=label.replace(pair.first,pair.second);
        name.setComponentID("parameterCaption");name.setText(label,juce::dontSendNotification);name.setJustificationType(juce::Justification::centred);name.setFont(uiFont(17.f));name.setBorderSize({});addAndMakeVisible(name);
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
        auto r=getLocalBounds();if(isNote||id=="masterEnabled"||compact){name.setVisible(false);toggle.setBounds(compact?r.withSizeKeepingCentre(r.getWidth(),juce::jmin(36,r.getHeight())):r);return;}
        if(inspector){name.setJustificationType(juce::Justification::centredLeft);const bool narrow=getWidth()<180;name.setBounds(narrow?82:106,6,getWidth()-(narrow?86:110),25);slider.setBounds(r);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,narrow?78:132,28);return;}
        name.setBounds(r.removeFromTop(23));r.reduce(4,0);
        if(ca)choice.setBounds(r.withHeight(32).withY(r.getY()+8));
        else if(ba)toggle.setBounds(r.withHeight(32).withY(r.getY()+8));
        else {slider.setBounds(r);slider.setTextBoxStyle(inspector?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,inspector?108:juce::jmax(44,r.getWidth()-4),26);}
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
    for(int i=0;i<7;++i)navigation[size_t(i)].setBounds(28,104+i*94,88,88);
    presets.setBounds(354,34,264,39);compare.setBounds(632,34,66,39);undo.setBounds(710,34,70,39);zoom.setBounds(1032,34,78,39);
    advancedButton.setBounds(937,114,163,32);
    learnNoise.setVisible(page==1);learnNoise.setBounds(150,expanded?166:640,166,expanded?35:85);
    learnKey.setVisible(page==0);learnKey.setBounds(650,160,96,36);learnKey.setButtonText("ANALYSE");applyKey.setVisible(page==0);applyKey.setBounds(752,160,80,36);applyKey.setButtonText("APPLY");
    allNotes.setVisible(page==0&&!expanded);noNotes.setVisible(false);allNotes.setBounds(957,738,139,31);
    resetPeak.setBounds(1134,117,104,33);
    control("masterEnabled")->setBounds(1135,34,103,39);
    control("outputGain")->setBounds(1130,573,112,112);
    control("limiterCeiling")->setBounds(1130,687,112,78);
    control("limiterEnabled")->compact=true;control("limiterEnabled")->setBounds(1134,767,104,24);
    control("mix")->setVisible(page==0&&!expanded);
    if(expanded)
    {
        if(page==0)
        {
            int n=0;
            for(auto* c:parameters)
            {
                if(c->isNote){for(int i=0;i<12;++i)if(c->id==noteIds[i])c->setBounds(156+i*57,723,53,37);}
                else {c->setBounds(145+(n%6)*159,392+(n/6)*149,153,142);++n;}
            }
            control("mix")->setVisible(true);control("mix")->setBounds(948,198,152,128);return;
        }
        const int count=parameters.size(),columns=8,rows=(count+columns-1)/columns;
        const int height=juce::jmin(108,368/juce::jmax(1,rows));
        for(int i=0;i<count;++i)parameters[i]->setBounds(137+(i%columns)*121,392+(i/columns)*height,117,height-2);
        control("mix")->setVisible(page==0);control("mix")->setBounds(948,198,152,128);return;
    }
    const auto place=[this](const juce::String& id,int x,int y,int w,int h,bool inspect=false,bool compact=false)
    {if(auto* c=control(id)){c->inspector=inspect;c->compact=compact;c->setBounds(x,y,w,h);}};
    if(page==0)
    {
        place("tuneEnabled",433,114,110,32,false,true);
        place("key",155,153,145,63);place("scale",311,153,160,63);place("engineMode",481,153,154,63);
        const char* inspectorIds[]={"speed","amount","vibratoPreserve","mix","formantShift"};
        for(int i=0;i<5;++i)place(inspectorIds[i],856,252+i*91,245,88,true);
        for(int i=0;i<12;++i)place(noteIds[i],156+i*57,723,53,37);
        return;
    }
    if(page==5)
    {
        for(int i=0;i<4;++i)place(juce::StringArray{"harmonyEnabled","harmonyMode","harmonyMix","harmonyHumanize"}[i],144+i*241,155,226,91,i>=2);
        const char* suffix[]={"Enabled","Interval","Level","Pan","Formant"};
        for(int v=0;v<4;++v)for(int k=0;k<5;++k)
        {
            const auto id="harmony"+juce::String(v+1)+suffix[k];place(id,249+k*168,264+v*83,158,81,k>=3,k==0);
            if(k==1||k==2)control(id)->slider.setSliderStyle(juce::Slider::LinearHorizontal);
            control(id)->name.setText(suffix[k],juce::dontSendNotification);
        }
        int i=0;for(auto* id:{"choirEnabled","choirVoices","choirPitch","choirTiming","choirVibrato","choirWidth","choirMix"})place(id,143+(i++)*137,638,132,112);
        return;
    }
    if(page==4)
    {
        place("reverbEnabled",665,114,110,32,false,true);place("reverbType",166,156,210,63);
        place("delayEnabled",865,263,220,40,false,true);place("delayDivision",864,319,223,67);place("delayMix",854,416,245,97,true);
        int i=0;for(auto* id:{"reverbPreDelay","reverbDecay","reverbDamping","reverbWidth","reverbMix"})place(id,156+(i++)*189,589,180,150);
        return;
    }
    auto ids=primaryForPage(page);
    if(page==2)
    {
        for(int stage=0;stage<2;++stage)
        {
            place(ids[size_t(stage*6)],149+stage*484,157,150,44,false,true);
            place(ids[size_t(stage*6+1)],310+stage*484,147,294,61);
            for(int i=2;i<6;++i)place(ids[size_t(stage*6+i)],146+stage*484+(i-2)*117,573,112,130);
        }
    }
    else
    {
        place(ids[0],665,114,110,32,false,true);
        for(size_t i=1;i<ids.size();++i)
        {
            const int n=int(i)-1;
            place(ids[i],page==1?330+n*190:153+n*189,589,page==1?180:181,150);
        }
    }
}

juce::Rectangle<float> JerzyAutoTuneAudioProcessorEditor::plotBounds() const
{
    if(expanded)return {166,218,758,145};
    if(page==0)return {166,236,665,293};
    if(page==4)return {166,225,665,303};
    if(page==2)return {155,230,944,305};
    return {166,225,929,303};
}

void JerzyAutoTuneAudioProcessorEditor::drawSpectrum(juce::Graphics& g,juce::Rectangle<float> r,bool showOutput)
{
    grid(g,r);const float fs=processor.graphRate.load();
    for(int i=0;i<5;++i)text(g,juce::String(page==3?18-9*i:-18*i),{130,r.getY()+r.getHeight()*i/4-9,32,20},14,silver,juce::Justification::centred);
    if(!expanded&&page!=3)text(g,"INPUT  /  SPECTRAL OUT",{166,r.getBottom()+29,600,24},16,silver);
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
    g.fillAll(juce::Colour(10,13,13));
    auto outer=surface.getLocalBounds().toFloat().reduced(8);
    g.setGradientFill(juce::ColourGradient(juce::Colour(173,171,157),outer.getTopLeft(),juce::Colour(68,72,69),outer.getBottomRight(),false));g.fillRoundedRectangle(outer,15);
    if(steel.isValid()){g.setOpacity(.27f);g.drawImage(steel,outer);g.setOpacity(1);}
    g.setColour(juce::Colour(230,225,210));g.drawRoundedRectangle(outer,15,1.5f);
    g.setColour(juce::Colour(16,19,18));g.drawRoundedRectangle(outer.reduced(5),11,2);
    g.setColour(silver.withAlpha(.7f));g.drawRoundedRectangle(outer.reduced(8),8,1);
    hardwarePanel(g,{25,26,1228,61});
    hardwarePanel(g,{126,104,986,678});hardwarePanel(g,{1118,104,135,678});
    g.setColour(white);g.setFont(uiFont(28,true));g.drawText("JERZY",42,35,96,38,juce::Justification::centredLeft);text(g,"VOCAL STUDIO",{145,38,201,31},26);
    const double rate=processor.graphRate.load();const int latency=processor.getLatencySamples();
    text(g,latency>0?"STUDIO  /  "+juce::String(1000.0*latency/rate,1)+" ms":"STUDIO  /  -- ms",{801,39,220,29},18,silver);
    const auto titles=juce::StringArray{"PITCH CORRECTION","NOISE REDUCTION","DUAL VOCAL COMPRESSION","VOCAL EQUALIZER + COLOUR","REVERB & DELAY","VOICE HARMONY + CHOIR","VOCAL CHARACTER"};
    led(g,{147,130},processor.parameters.getRawParameterValue(pagePower[page])->load()>.5f);
    text(g,titles[page],{161,114,491,33},22);
    g.setColour(silver.withAlpha(.22f));g.drawHorizontalLine(153,139,1100);
    const auto r=plotBounds();
    if(page==0)
    {
        g.setColour(juce::Colour(11,16,16));g.fillRect(r);
        const float pitch=processor.getDetectedPitch();const int centre=pitch>=0?int(std::round(pitch)):60;
        for(int note=centre-6;note<=centre+6;++note)
        {
            const float y=r.getCentreY()-(note-centre)*r.getHeight()/12;
            if(y<r.getY()||y>r.getBottom())continue;
            g.setColour(juce::Colour(38,46,43));g.drawHorizontalLine(int(y),r.getX(),r.getRight());
            if(note%2==0)text(g,juce::String(noteNames[(note%12+12)%12])+juce::String(note/12-1),{129,y-10,35,20},15,white,juce::Justification::centred);
        }
        for(int i=0;i<9;++i){const float x=r.getX()+r.getWidth()*i/8;g.setColour(juce::Colour(38,46,43));g.drawVerticalLine(int(x),r.getY(),r.getBottom());if(!expanded)text(g,juce::String(-8.67f+8.67f*i/8,1)+"s",{x-19,531,45,19},14,silver,juce::Justification::centred);}
        for(int pass=0;pass<2;++pass)
        {
            juce::Path path;bool pen=false;
            for(size_t i=0;i<pitchHistory.size();++i)
            {
                const size_t index=(historyWrite+i)%pitchHistory.size();const float v=pass==0?targetHistory[index]:pitchHistory[index];
                if(v<0){pen=false;continue;}const float x=r.getX()+float(i)*r.getWidth()/float(pitchHistory.size()-1),y=juce::jlimit(r.getY(),r.getBottom(),r.getCentreY()-(v-centre)*r.getHeight()/12);
                if(pen)path.lineTo(x,y);else path.startNewSubPath(x,y);pen=true;
            }
            g.setColour(pass==0?juce::Colour(195,192,180):white);g.strokePath(path,juce::PathStrokeType(pass==0?3.f:1.4f));
        }
        text(g,"INPUT PITCH    /    TARGET NOTE",{170,211,460,23},16,silver);
        if(!expanded)
        {
            hardwarePanel(g,{852,157,248,87});
            text(g,"CURRENT NOTE",{867,163,222,20},16,silver);
            const int note=juce::roundToInt(pitch);
            text(g,pitch<0?"--":juce::String(noteNames[(note%12+12)%12])+juce::String(note/12-1),{869,184,108,49},43);
            text(g,juce::String(juce::roundToInt(processor.getCorrectionCents()))+" ct",{977,192,111,31},25);
            for(int pass=0;pass<2;++pass)
            {
                const float y=pass==0?587.f:663.f,height=pass==0?28.f:22.f;
                text(g,pass==0?"IN":"PREP",{129,y-10,34,20},14,silver,juce::Justification::centred);
                g.setColour(juce::Colour(8,12,11));g.fillRect(166.f,y-height,665.f,height*2);
                g.setColour(silver.withAlpha(pass==0?.68f:.44f));
                for(size_t i=0;i<waveHistory.size();++i)
                {
                    const size_t index=(historyWrite+i)%waveHistory.size();const float v=pass==0?waveHistory[index]:wavePeakHistory[index];
                    const float amplitude=juce::jlimit(0.f,height,v*height*3.5f);
                    g.fillRect(166.f+float(i)*665.f/float(waveHistory.size()),y-amplitude,1.7f,2*amplitude);
                }
            }
        }
        const int key=processor.getSuggestedKey();
        text(g,key<0?"ANALYSE A PHRASE TO SUGGEST KEY":juce::String("KEY: ")+noteNames[key]+(processor.getSuggestedMinor()?" MINOR":" MAJOR")+" / "+juce::String(juce::roundToInt(processor.getKeyConfidence()*100))+"%",{166,expanded?193.f:695.f,660,22},15,silver);
    }
    else if(page==1||page==3||page==6)
    {
        drawSpectrum(g,r,page!=3);
        if(page==1)text(g,processor.getNoiseProgress()>0&&processor.getNoiseProgress()<1?"LEARNING NOISE: "+juce::String(processor.getNoiseProgress()*100,0)+"%":processor.hasNoiseProfile()?"PROFILE LEARNED  /  "+juce::String(processor.getNoiseReduction(),1)+" dB REDUCTION":"PLAY NOISE ALONE, THEN LEARN (1 SECOND)",{340,170,750,27},17,silver);
        if(page==6)text(g,"SPECTRAL ENVELOPE   /   HARMONIC CHARACTER   /   BREATH",{166,173,920,26},17,silver);
        if(page==3)
        {
            text(g,"DRAG A NODE TO ADJUST  /  SCROLL FOR MID Q",{166,173,920,26},17,silver);
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
                const auto pt=juce::Point<float>(frequencyX(hz[i],r),r.getCentreY()-gain[i]*r.getHeight()/36);
                g.setColour(juce::Colour(35,39,36));g.fillEllipse(pt.x-10,pt.y-10,20,20);g.setColour(i==dragBand?white:silver);g.drawEllipse(pt.x-10,pt.y-10,20,20,2);text(g,juce::String(i+1),{pt.x-8,pt.y-10,16,20},15,white,juce::Justification::centred);
            }
        }
    }
    else if(page==2)
    {
        for(int stage=0;stage<2;++stage)
        {
            auto chart=r.withWidth((r.getWidth()-30)*.5f).translated(stage*(r.getWidth()+30)*.5f,0);
            g.setColour(juce::Colour(10,15,14));g.fillRect(chart);
            const auto value=[this](const char* id){return processor.parameters.getRawParameterValue(id)->load();};
            const float threshold=value(stage==0?"compThreshold":"comp2Threshold"),ratio=value(stage==0?"compRatio":"comp2Ratio"),makeup=value(stage==0?"compMakeup":"comp2Makeup");
            const float knee=std::max(value(stage==0?"compKnee":"comp2Knee"),value(stage==0?"compStyle":"comp2Style")>=2?8.f:0.f);
            for(int i=0;i<5;++i){g.setColour(raised);g.drawVerticalLine(int(chart.getX()+chart.getWidth()*i/4),chart.getY(),chart.getBottom());g.drawHorizontalLine(int(chart.getY()+chart.getHeight()*i/4),chart.getX(),chart.getRight());text(g,juce::String(-60+15*i),{chart.getX()+chart.getWidth()*i/4-14,chart.getBottom()+4,35,20},14,silver);}
            juce::Path path;
            for(int i=0;i<=100;++i)
            {
                const float level=-60+.6f*i,over=level-threshold;
                const float gr=jerzy::studio::Compressor::gainReduction(over,ratio,knee);
                const float x=chart.getX()+i*chart.getWidth()/100,y=juce::jlimit(chart.getY(),chart.getBottom(),dbY(level-gr+makeup,chart,-60,0));
                if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
            }
            const bool on=value(stage==0?"compEnabled":"comp2Enabled")>.5f;
            g.setColour(on?white:muted.withAlpha(.5f));g.strokePath(path,juce::PathStrokeType(2));
            text(g,juce::String(stage==0?"COMPRESSOR 1":"COMPRESSOR 2")+"    GR "+juce::String(stage==0?processor.getCompressorReduction():processor.getStageBReduction(),1)+" dB",{chart.getX()+10,chart.getY()+9,chart.getWidth()-20,28},19);
        }
        if(!expanded)text(g,"STEREO LINKED   /   SOFT KNEE, SIDECHAIN AND WARM IN ALL CONTROLS",{153,731,938,25},17,silver);
    }
    else if(page==4)
    {
        if(!expanded){hardwarePanel(g,{851,225,248,324});text(g,"TEMPO DELAY",{868,233,210,23},20);}
        g.setColour(black);g.fillRect(r);const float decay=processor.parameters.getRawParameterValue("reverbDecay")->load(),pre=processor.parameters.getRawParameterValue("reverbPreDelay")->load()*.001f;
        for(int i=0;i<=4;++i){const float x=r.getX()+r.getWidth()*i/4;g.setColour(raised);g.drawVerticalLine(int(x),r.getY(),r.getBottom());text(g,juce::String((decay*1.2f+pre)*i/4,2)+" s",{x-25,r.getBottom()+4,65,20},14,silver);}
        for(int i=0;i<=4;++i){const float y=r.getY()+r.getHeight()*i/4;g.setColour(raised);g.drawHorizontalLine(int(y),r.getX(),r.getRight());text(g,juce::String(-18*i)+" dB",{r.getX()+6,y+2,70,20},14,silver);}
        juce::Path path;for(int i=0;i<240;++i){const float t=float(i)*(decay*1.2f+pre)/239,value=t<pre?-72.f:-60.f*(t-pre)/decay,x=r.getX()+r.getWidth()*i/239,y=juce::jlimit(r.getY(),r.getBottom(),dbY(value,r));if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        g.setColour(white);g.strokePath(path,juce::PathStrokeType(2));
        text(g,"-60 dB / "+juce::String(decay,2)+" s   /   "+juce::String(processor.getTempo(),1)+" BPM",{403,173,430,26},18,silver);
    }
    else if(page==5&&!expanded)
    {
        for(int v=0;v<4;++v)text(g,"VOICE "+juce::String(v+1),{151,float(290+v*83),92,28},20,silver);
    }
    // Fixed master strip: the scale is in dBFS and red is reserved for zero/overload.
    for(int i=0;i<=10;++i)
    {
        const float y=211+326.f*i/10;
        text(g,juce::String(-6*i),{1178,y-10,31,20},15,i==0?red:silver,juce::Justification::centred);
    }
    for(int ch=0;ch<2;++ch)
    {
        const float x=ch==0?1148.f:1222.f,level=ch==0?inputDb:outputDb;
        text(g,ch==0?"IN":"OUT",{x-10,174,38,25},17,white,juce::Justification::centred);
        g.setColour(juce::Colour(6,9,8));g.fillRect(x,211.f,12.f,326.f);
        g.setColour(silver.withAlpha(.38f));g.drawRect(x,211.f,12.f,326.f,1.f);
        const float height=326*juce::jlimit(0.f,1.f,(level+60)/60);
        g.setGradientFill(juce::ColourGradient(white,x,211,juce::Colour(142,146,136),x,537,false));g.fillRect(x+2,537-height,8.f,height);
        g.setColour(red);g.fillRect(x-2,209.f,16.f,2.f);
        if((ch==0?processor.getInputOverload():processor.getOutputOverload())>=1){g.setColour(red);g.fillRect(x-2,203.f,16.f,6.f);}
        text(g,level<=-59?"-inf":juce::String(level,1),{x-15,546,47,20},14,silver,juce::Justification::centred);
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
    float inEnergy=0,outEnergy=0;for(size_t i=0;i<input.size();++i){inEnergy+=input[i]*input[i];outEnergy+=output[i]*output[i];}
    waveHistory[historyWrite]=std::sqrt(inEnergy/float(input.size()));wavePeakHistory[historyWrite]=std::sqrt(outEnergy/float(output.size()));
    pitchHistory[historyWrite]=processor.getDetectedPitch();targetHistory[historyWrite]=processor.getTargetNote();reductionHistory[historyWrite]=processor.getCompressorReduction();stageBHistory[historyWrite]=processor.getStageBReduction();historyWrite=(historyWrite+1)%pitchHistory.size();
    inputDb=juce::jmax(juce::Decibels::gainToDecibels(processor.getInputPeak(),-60.f),inputDb-1.5f);outputDb=juce::jmax(juce::Decibels::gainToDecibels(processor.getOutputPeak(),-60.f),outputDb-1.5f);
    const float over=juce::jmax(processor.getInputOverload(),processor.getOutputOverload());resetPeak.setToggleState(over>=1,juce::dontSendNotification);resetPeak.setButtonText(over>=1?"! OVER":"RESET PEAK");
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
