#include "PluginProcessor.h"
#include <random>
#include <iostream>
#include <stdexcept>
#include <functional>

namespace
{
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void set(JerzyAutoTuneAudioProcessor& p,const char* id,float real){auto* x=p.parameters.getParameter(id);check(x!=nullptr,"Studio parameter missing");x->setValueNotifyingHost(x->convertTo0to1(real));}
void dry(JerzyAutoTuneAudioProcessor& p)
{
    for(auto* id:{"gateEnabled","noiseEnabled","deEsserEnabled","satEnabled","doublerEnabled","compEnabled","eqEnabled","limiterEnabled","comp2Enabled","cleanEnabled","voiceEnabled","reverbEnabled","delayEnabled","harmonyEnabled","choirEnabled"})set(p,id,0);
    set(p,"mix",0);set(p,"engineMode",1);set(p,"outputGain",0);
}
std::vector<float> render(JerzyAutoTuneAudioProcessor& p,int samples,float hz=220,bool midi=false)
{
    constexpr int block=128;juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer events;
    p.prepareToPlay(48000,block);std::vector<float> result;result.reserve(size_t(samples));
    for(int offset=0;offset<samples;offset+=block)
    {
        events.clear();if(midi&&offset==0)events.addEvent(juce::MidiMessage::noteOn(1,64,juce::uint8(100)),0);
        for(int i=0;i<block;++i){const float x=.2f*std::sin(2*juce::MathConstants<float>::pi*hz*float(offset+i)/48000.f);buffer.setSample(0,i,x);buffer.setSample(1,i,x);}
        p.processBlock(buffer,events);for(int i=0;i<block&&offset+i<samples;++i){const float x=buffer.getSample(0,i);check(std::isfinite(x),"Studio processor produced nonfinite audio");result.push_back(x);}
    }
    return result;
}
double energy(const std::vector<float>& v,size_t start){double e=0;for(size_t i=start;i<v.size();++i)e+=double(v[i])*v[i];return e/std::max(size_t(1),v.size()-start);}
double toneAmplitude(const std::vector<float>& v,double hz)
{
    double re=0,im=0;const size_t start=v.size()/2;
    for(size_t i=start;i<v.size();++i){const double angle=2*juce::MathConstants<double>::pi*hz*double(i)/48000.;re+=v[i]*std::cos(angle);im+=v[i]*std::sin(angle);}
    return 2*std::sqrt(re*re+im*im)/double(v.size()-start);
}
}

void runStudioTests()
{
    using namespace jerzy::studio;
    int cases=0;
    for(double fs:{44100.,48000.,96000.,192000.})
    {
        auto prep=std::make_unique<SpectralPrep>();prep->prepare(fs);prep->settings.voice=true;
        std::vector<float> source(size_t(fs*.25));double error=0,signal=0;
        for(size_t i=0;i<source.size();++i)
        {
            float l=.2f*std::sin(float(i)*.123f),r=-l;source[i]=l;prep->process(l,r,220);
            if(i>size_t(prep->latency()+4096))
            {
                const float expected=source[i-size_t(prep->latency())];error+=std::pow(l-expected,2);signal+=expected*expected;
                check(std::abs(r+l)<1.e-6f,"Spectral prep collapses opposite-polarity stereo");
            }
        }
        check(signal>0&&error/signal<1.e-8,"WOLA unity reconstruction or latency incorrect");++cases;
    }
    {
        auto prep=std::make_unique<SpectralPrep>();prep->prepare(48000);prep->settings.clean=true;prep->settings.reduction=18;prep->requestLearn.store(true);
        std::mt19937 rng(71);std::normal_distribution<float> noise(0,.02f);double in=0,out=0;
        for(int i=0;i<96000;++i){float l=noise(rng),r=l;const float raw=l;prep->process(l,r,0);if(i>72000){in+=raw*raw;out+=l*l;}}
        check(prep->profileReady.load(),"Noise profile learning did not complete");
        std::cout<<"Learned-noise attenuation: "<<10*std::log10(in/out)<<" dB\n";
        check(out<in*.35,"Learned spectral cleanup does not reduce stationary noise");
        double real=0,imag=0;int n=0;
        for(int i=0;i<48000;++i){float l=noise(rng)+.2f*std::sin(2*pi*220*i/48000.f),r=l;prep->process(l,r,220);if(i>24000){const float angle=2*pi*220*i/48000.f;real+=l*std::cos(angle);imag+=l*std::sin(angle);++n;}}
        const double amplitude=2*std::sqrt(real*real+imag*imag)/n;
        check(amplitude>.14,"Noise reduction damages strong voiced tone excessively");cases+=2;
    }
    {
        std::array<double,4> energies{};
        for(int style=0;style<4;++style)
        {
            Compressor c;c.settings.threshold=-24;c.settings.ratio=4;c.settings.attack=8;c.settings.release=80;c.settings.style=style;c.prepare(48000);
            for(int i=0;i<48000;++i){float l=(i%6000<1000?.7f:.1f)*std::sin(i*.1f),r=l;c.process(l,r,1);energies[size_t(style)]+=l*l;check(std::abs(l-r)<1.e-6f,"Compressor changes centred stereo image");}
        }
        for(int i=0;i<4;++i)for(int j=i+1;j<4;++j)check(std::abs(energies[size_t(i)]-energies[size_t(j)])>1.e-3,"Compressor styles are identical");++cases;
    }
    {
        JerzyAutoTuneAudioProcessor a,b;dry(a);dry(b);set(b,"comp2Enabled",1);set(b,"comp2Threshold",-30);set(b,"comp2Ratio",4);
        const auto bypass=render(a,48000),compressed=render(b,48000);
        check(energy(compressed,24000)<energy(bypass,24000)*.6,"Second compressor does not independently compress audio");++cases;
    }
    {
        JerzyAutoTuneAudioProcessor a,b;dry(a);dry(b);
        for(auto* p:{&a,&b}){set(*p,"deEsserEnabled",1);set(*p,"deEsserAmount",0);set(*p,"deEsserHardThreshold",-36);}
        set(b,"deEsserHard",14);auto plain=render(a,48000,4000),reduced=render(b,48000,4000);
        check(energy(reduced,24000)<energy(plain,24000)*.92,"Hard consonant control has no measurable high-band effect");++cases;
    }
    {
        const auto impulse=[](int type,float decay)
        {
            Space s;s.settings.reverb=true;s.settings.type=type;s.settings.decay=decay;s.settings.reverbMix=50;s.settings.duck=0;s.prepare(48000);
            std::vector<float> result(144000);for(size_t i=0;i<result.size();++i){float l=i==0?1.f:0.f,r=l;s.process(l,r);check(std::isfinite(l)&&std::abs(l)<2,"Reverb is unstable");result[i]=l;}
            return result;
        };
        const auto shortTail=impulse(0,.5f),longTail=impulse(0,3.f),plate=impulse(1,1.8f),hall=impulse(2,1.8f);
        check(energy(longTail,48000)>energy(shortTail,48000)*2,"Reverb decay does not extend tail");
        check(energy(plate,5000)>1.e-10&&energy(hall,5000)>1.e-10,"Reverb styles do not produce a wet tail");
        double difference=0;for(size_t i=2000;i<plate.size();++i)difference+=std::pow(plate[i]-hall[i],2);check(difference>1.e-6,"Reverb styles are identical");cases+=2;
        Space delay;delay.settings.delay=true;delay.settings.delayMix=50;delay.settings.duck=0;delay.settings.feedback=50;delay.prepare(48000);delay.configure(120);
        float firstL=0,firstR=0,secondR=0;
        for(int i=0;i<25000;++i){float l=i==0?1.f:0,r=0;delay.process(l,r);if(i==12000){firstL=l;firstR=r;}if(i==24000)secondR=r;}
        check(firstL>.4f&&std::abs(firstR)<1.e-7f&&secondR>.01f,"Tempo-synced ping-pong delay routing/time incorrect");++cases;
    }
    {
        std::vector<float> four,thirtyTwo;
        for(int count:{4,32})
        {
            Choir c;c.enabled=true;c.voices=count;c.mix=100;c.prepare(48000);auto& result=count==4?four:thirtyTwo;result.resize(48000);
            for(int i=0;i<48000;++i){float l=.2f*std::sin(i*.11f),r=l;c.process(l,r);check(std::isfinite(l)&&std::abs(l)<.5f,"Choir gain unstable");result[size_t(i)]=l;}
        }
        double delta=0;for(size_t i=12000;i<four.size();++i)delta+=std::pow(four[i]-thirtyTwo[i],2);check(delta>1.e-4,"Choir size does not change ensemble");++cases;
    }
    {
        JerzyAutoTuneAudioProcessor harmony;dry(harmony);set(harmony,"formantEnabled",0);set(harmony,"harmonyEnabled",1);set(harmony,"harmonyMode",0);set(harmony,"harmonyMix",100);set(harmony,"harmonyHumanize",0);
        for(int v=1;v<=4;++v){const auto prefix="harmony"+juce::String(v);set(harmony,(prefix+"Enabled").toRawUTF8(),v==1?1.f:0.f);}
        set(harmony,"harmony1Interval",12);set(harmony,"harmony1Level",-6);set(harmony,"harmony1Pan",0);
        auto octave=render(harmony,72000,220);
        std::cout<<"Harmony octave amplitude: "<<toneAmplitude(octave,440)<<"\n";
        check(toneAmplitude(octave,440)>.025,"Harmony does not generate independently shifted octave");
        set(harmony,"harmonyMode",2);auto midi=render(harmony,72000,220,true);
        check(toneAmplitude(midi,329.6276)>.02,"MIDI does not control absolute harmony pitch");cases+=2;
    }
    {
        KeyDetector d;d.reset();const int notes[]={60,60,60,60,64,64,67,67,62,65,69,71};
        for(int i=0;i<200;++i)for(int n:notes)d.add(float(n),1);
        check(d.key==0&&!d.minor,"Key suggestion fails reference C-major phrase");++cases;
    }
    {
        JerzyAutoTuneAudioProcessor p;dry(p);set(p,"masterEnabled",0);p.prepareToPlay(48000,128);
        juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;int peak=-1;
        for(int block=0;block<p.getLatencySamples()/128+3;++block)
        {
            b.clear();if(block==0){b.setSample(0,0,.25f);b.setSample(1,0,.25f);}p.processBlock(b,midi);
            for(int i=0;i<128;++i)if(std::abs(b.getSample(0,i))>1.e-7f){check(peak<0,"Master bypass impulse smeared");peak=block*128+i;}
        }
        check(peak==p.getLatencySamples(),"Master bypass is not latency aligned");++cases;
    }
    {
        JerzyAutoTuneAudioProcessor p;dry(p);p.prepareToPlay(48000,128);p.learnNoise();
        juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;std::mt19937 rng(71);std::normal_distribution<float> noise(0,.02f);
        for(int block=0;block<400;++block){for(int i=0;i<128;++i){const float x=noise(rng);b.setSample(0,i,x);b.setSample(1,i,x);}p.processBlock(b,midi);}
        check(p.hasNoiseProfile(),"Processor noise learning did not complete");
        juce::MemoryBlock state;p.getStateInformation(state);JerzyAutoTuneAudioProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));
        check(restored.hasNoiseProfile(),"Saved noise profile was not restored");
        auto tree=restored.parameters.copyState();tree.setProperty("noiseProfile","invalid",nullptr);auto xml=tree.createXml();juce::MemoryBlock invalid;juce::AudioProcessor::copyXmlToBinary(*xml,invalid);
        restored.setStateInformation(invalid.getData(),int(invalid.getSize()));check(!restored.hasNoiseProfile(),"Malformed profile retained stale learned noise");++cases;
    }
    std::cout<<"PASS: "<<cases<<" studio audio regressions (WOLA, learned noise, dynamics, sibilance, reverb, delay, choir, harmony, MIDI, key, master bypass)\n";
}
