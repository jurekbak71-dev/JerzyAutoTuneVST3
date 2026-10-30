#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <complex>
#include <vector>

namespace jerzy::studio
{
constexpr float pi = 3.14159265358979323846f;
inline float db(float x) noexcept { return std::pow(10.f, x * .05f); }
inline float msCoefficient(float ms, double fs) noexcept { return std::exp(-1.f / (float(fs) * std::max(.05f, ms) * .001f)); }
inline float readDelay(const std::vector<float>& ring, size_t write, float delay) noexcept
{
    delay = std::clamp(delay, 1.f, float(ring.size() - 2));
    float position = float(write) - delay;
    if (position < 0) position += float(ring.size());
    const auto a = size_t(position) % ring.size(), b = (a + 1) % ring.size();
    const float fraction = position - std::floor(position);
    return ring[a] + (ring[b] - ring[a]) * fraction;
}

// Audio-thread producer, UI-thread consumer; atomics avoid races without audio locks.
struct AnalysisTap
{
    static constexpr size_t size = 2048;
    std::array<std::atomic<float>, size> input {}, output {};
    std::atomic<unsigned> position { 0 };
    unsigned write = 0;
    void push(float in, float out) noexcept
    {
        input[write].store(in, std::memory_order_relaxed);
        output[write].store(out, std::memory_order_relaxed);
        write = (write + 1) % unsigned(size);
    }
    void publish() noexcept { position.store(write, std::memory_order_release); }
    void copy(std::array<float, size>& in, std::array<float, size>& out) const noexcept
    {
        const auto start = position.load(std::memory_order_acquire);
        for (size_t i = 0; i < size; ++i)
        {
            in[i] = input[(start + i) % size].load(std::memory_order_relaxed);
            out[i] = output[(start + i) % size].load(std::memory_order_relaxed);
        }
    }
};

// Four-times-overlapped square-root Hann WOLA. No heap allocation in process().
class SpectralPrep
{
public:
    static constexpr int order = 11, size = 1 << order, bins = size / 2 + 1, hop = size / 4;
    struct Settings { bool clean = false, voice = false, audition = false; float reduction = 6, smoothMs = 120, mix = 1, air = 0, colour = 3000, character = 0, voiceMix = 1; } settings;
    std::atomic<bool> requestLearn { false }, profileReady { false }, restoreProfile { false };
    std::atomic<float> progress { 0 }, removedDb { 0 };
    std::atomic<double> profileRate {0};
    std::array<std::atomic<float>, bins> savedProfile {};
    SpectralPrep() : fft(order) {}
    int latency() const noexcept { return size; }
    void prepare(double rate)
    {
        fs = rate; index = phase = frames = 0; learning = false; wasActive=false; fade = 0; warm = size;
        input = {}; overlap = {}; spectral = {}; dry = {}; gain.fill(1); noise.fill(0); accumulation.fill(0);
        for (int i = 0; i < size; ++i) window[size_t(i)] = std::sqrt(.5f - .5f * std::cos(2.f * pi * float(i) / float(size)));
        for (int k = 0; k < bins; ++k) noise[size_t(k)] = std::abs(profileRate.load()-fs)<.1 ? savedProfile[size_t(k)].load() : 0;
        profileReady.store(std::any_of(noise.begin(), noise.end(), [](float x) { return x > 1.e-12f; }));
        progress.store(0); requestLearn.store(false); restoreProfile.store(false);
    }
    void process(float& left, float& right, float fundamental) noexcept
    {
        const bool active = settings.clean || settings.voice;
        if (active && !wasActive) warm = size;
        wasActive = active;
        const float target = active && warm <= 0 ? 1.f : 0;
        fade += (target - fade) * (1.f - msCoefficient(10, fs));
        const float l = left, r = right;
        std::array<float, 2> current { l, r };
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const float delayed = dry[ch][size_t(index)];
            dry[ch][size_t(index)] = current[ch]; input[ch][size_t(index)] = current[ch];
            float wet = overlap[ch][size_t(index)]; overlap[ch][size_t(index)] = 0;
            if (settings.audition && settings.clean) wet = delayed - wet;
            current[ch] = delayed + (wet - delayed) * fade;
        }
        left = current[0]; right = current[1]; index = (index + 1) % size;
        if (warm > 0) --warm;
        if (++phase == hop)
        {
            phase = 0;
            if (requestLearn.exchange(false))
            {
                learning = true; frames = 0; accumulation.fill(0); profileReady.store(false); progress.store(0);
            }
            if (restoreProfile.exchange(false))
            {
                for (int k = 0; k < bins; ++k) noise[size_t(k)] = std::abs(profileRate.load()-fs)<.1 ? savedProfile[size_t(k)].load() : 0;
            }
            if (active || learning) frame(fundamental);
        }
    }
private:
    void frame(float fundamental) noexcept
    {
        std::array<float, bins> power {};
        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto& s = spectral[ch]; s.fill(0);
            for (int i = 0; i < size; ++i) s[size_t(i)] = input[ch][size_t((index + i) % size)] * window[size_t(i)];
            fft.performRealOnlyForwardTransform(s.data());
            for (int k = 0; k < bins; ++k) power[size_t(k)] += .5f * (s[size_t(2*k)] * s[size_t(2*k)] + s[size_t(2*k+1)] * s[size_t(2*k+1)]);
        }
        if (learning)
        {
            ++frames;
            const int wanted = std::max(4, int(fs / hop));
            for (int k = 0; k < bins; ++k) accumulation[size_t(k)] += power[size_t(k)];
            progress.store(std::min(1.f, float(frames) / float(wanted)));
            if (frames >= wanted)
            {
                float energy = 0;
                for (int k = 0; k < bins; ++k)
                {
                    noise[size_t(k)] = accumulation[size_t(k)] / float(frames);
                    savedProfile[size_t(k)].store(noise[size_t(k)]); energy += noise[size_t(k)];
                }
                profileRate.store(fs);profileReady.store(energy > 1.e-8f); learning = false;
            }
        }
        const float floor = db(-std::clamp(settings.reduction, 0.f, 24.f));
        const float recovery = std::exp(-float(hop) / (float(fs) * std::max(.02f, settings.smoothMs * .001f)));
        float average = 0;
        for (int k = 0; k < bins; ++k)
        {
            const auto ki = size_t(k);
            const float p = (power[size_t(std::max(0,k-1))] + 2*power[ki] + power[size_t(std::min(bins-1,k+1))]) * .25f;
            const float n = (noise[size_t(std::max(0,k-1))] + 2*noise[ki] + noise[size_t(std::min(bins-1,k+1))]) * .25f;
            float desired = settings.clean && profileReady.load() ? std::max(floor, 1.f - n / (p + 1.e-15f)) : 1.f;
            gain[ki] = desired < gain[ki] ? .35f*gain[ki] + .65f*desired : recovery*gain[ki] + (1-recovery)*desired;
            float applied = 1.f + (gain[ki]-1.f)*settings.mix;
            if (settings.voice && fundamental >= 50 && fundamental <= 1200)
            {
                const float hz = float(k) * float(fs) / size;
                const float distance = std::abs(hz / fundamental - std::round(hz / fundamental));
                const float spread = std::clamp(float(fs) / (size*fundamental), .10f, .28f);
                const float harmonic = std::exp(-.5f * distance * distance / (spread*spread));
                const float noiseGain = db(settings.air) * std::pow(std::max(100.f,hz) / std::max(500.f,settings.colour), settings.air * .015f);
                const float harmonicGain = db(settings.character * .04f * std::clamp(std::log2(std::max(100.f,hz)/1000.f), -2.f, 2.f));
                applied *= 1.f + (std::clamp(harmonic*harmonicGain + (1-harmonic)*noiseGain, .1f, 4.f)-1.f)*settings.voiceMix;
            }
            average += gain[ki];
            for (auto& s : spectral) { s[size_t(2*k)] *= applied; s[size_t(2*k+1)] *= applied; }
        }
        removedDb.store(-20.f * std::log10(std::max(.0001f, average / bins)));
        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto& s = spectral[ch]; fft.performRealOnlyInverseTransform(s.data());
            for (int i = 0; i < size; ++i) overlap[ch][size_t((index+i)%size)] += .5f * s[size_t(i)] * window[size_t(i)];
        }
    }
    juce::dsp::FFT fft;
    double fs = 48000;
    int index = 0, phase = 0, frames = 0, warm = size;
    bool learning = false, wasActive = false;
    float fade = 0;
    std::array<std::array<float, size>, 2> input {}, overlap {}, dry {};
    std::array<std::array<float, size*2>, 2> spectral {};
    std::array<float, size> window {};
    std::array<float, bins> noise {}, accumulation {}, gain {};
};

struct Compressor
{
    struct Settings { float threshold=-18, ratio=3, attack=10, release=120, knee=6, makeup=0, sidechain=0, warm=0; int style=0; } settings;
    void prepare(double rate) noexcept { fs=rate; envelope=0; sideLow={}; previous={}; dc={}; reduction=0; configure(); }
    void configure() noexcept
    {
        const float attackFactor=settings.style==1 ? .3f : settings.style==2 ? 1.6f : settings.style==3 ? 2.4f : 1.f;
        const float releaseFactor=settings.style==2 ? 2.f : settings.style==3 ? 3.f : 1.f;
        attack=msCoefficient(settings.attack*attackFactor,fs); release=msCoefficient(settings.release*releaseFactor,fs);
        hp=std::exp(-2*pi*std::clamp(settings.sidechain,0.f,600.f)/float(fs));
        dcCoeff=std::exp(-2*pi*12.f/float(fs)); makeup=db(settings.makeup);
    }
    void process(float& left,float& right,float wet) noexcept
    {
        float peak=0;
        for(size_t ch=0;ch<2;++ch)
        {
            const float x=ch==0?left:right;
            sideLow[ch]=(1-hp)*x+hp*sideLow[ch];
            const float v=settings.sidechain>0 ? x-sideLow[ch] : x;
            peak=std::max(peak,std::abs(v));
        }
        if(settings.style>=2)
        {
            const float square=peak*peak;
            const float c=square>envelope?attack:release;
            envelope=c*envelope+(1-c)*square;
            peak=std::sqrt(std::max(0.f,envelope));
        }
        else { const float c=peak>envelope?attack:release; envelope=c*envelope+(1-c)*peak; peak=envelope; }
        const float over=20*std::log10(std::max(peak,1.e-8f))-settings.threshold;
        const float knee=std::max(settings.knee,settings.style>=2?8.f:0.f);
        float gr=0;
        if(knee>.001f && over>-knee*.5f && over<knee*.5f) gr=(1-1/std::max(1.f,settings.ratio))*(over+knee*.5f)*(over+knee*.5f)/(2*knee);
        else if(over>knee*.5f) gr=(1-1/std::max(1.f,settings.ratio))*over;
        reduction=std::max(0.f,gr)*wet;
        const float factor=1+(db(-gr)*makeup-1)*wet;
        left*=factor; right*=factor;
        for(size_t ch=0;ch<2;++ch)
        {
            float& x=ch==0?left:right;
            const float coloured=(std::tanh(x*2.f+.12f)-std::tanh(.12f))/2.f;
            const float filtered=coloured-previous[ch]+dcCoeff*dc[ch]; previous[ch]=coloured;dc[ch]=filtered;
            x+=(filtered-x)*settings.warm*.01f*wet;
        }
    }
    float reduction=0;
private:
    double fs=48000;
    float envelope=0,attack=0,release=0,hp=1,makeup=1,dcCoeff=0;
    std::array<float,2> sideLow{},previous{},dc{};
};

class Space
{
public:
    struct Settings { bool reverb=false,delay=false,ping=true,sync=true; int type=0,division=1; float decay=1.8f,preDelay=40,damping=45,width=100,reverbMix=18,duck=6,delayMs=250,feedback=25,delayMix=12; } settings;
    void prepare(double rate)
    {
        fs=rate;position=0;duckEnvelope=0;revFade=delFade=0;hpState={};lpState={};delayLow={};
        for(auto& b:echo)b.assign(size_t(fs*6)+8,0);
        for(auto& b:pre)b.assign(size_t(fs*.26)+8,0);
        for(auto& line:lines){line.assign(size_t(fs*.24)+8,0);}
        low.fill(0);length.fill(1);prePosition=linePosition=0; configure(120);
    }
    void configure(double bpm) noexcept
    {
        static constexpr float times[]={.0297f,.0371f,.0411f,.0437f,.0531f,.0617f,.0713f,.0797f};
        const float sizeScale=settings.type==0?.65f:settings.type==1?.82f:1.4f;
        for(size_t i=0;i<8;++i){target[i]=float(fs)*times[i]*sizeScale; feedback[i]=std::pow(.001f,(target[i]/float(fs))/std::clamp(settings.decay,.2f,12.f));}
        static constexpr float beats[]={1.f,.5f,.75f,.25f,1.5f,2.f};
        echoTarget=float(fs)*(settings.sync?60.f/float(std::clamp(bpm,20.,400.))*beats[size_t(std::clamp(settings.division,0,5))]:settings.delayMs*.001f);
        echoTarget=std::clamp(echoTarget,1.f,float(echo[0].size()-2));
        damp=std::exp(-2*pi*(2000+(1-settings.damping*.01f)*10000)/float(fs));
        hp=std::exp(-2*pi*160.f/float(fs)); lp=std::exp(-2*pi*8500.f/float(fs));smooth=1-msCoefficient(25,fs);
        if(echoLength<=1){echoLength=echoTarget;length=target;}
    }
    void process(float& left,float& right) noexcept
    {
        revFade+=(float(settings.reverb)-revFade)*smooth;delFade+=(float(settings.delay)-delFade)*smooth;
        const float peak=std::max(std::abs(left),std::abs(right));
        duckEnvelope=std::max(peak,duckEnvelope*msCoefficient(150,fs));
        const float duckGain=db(-settings.duck*std::clamp(duckEnvelope*4,0.f,1.f));
        const std::array<float,2> in{left,right};
        std::array<float,2> early{};
        for(size_t ch=0;ch<2;++ch){pre[ch][prePosition]=in[ch];early[ch]=readDelay(pre[ch],prePosition,std::max(1.f,float(fs)*settings.preDelay*.001f));}
        prePosition=(prePosition+1)%pre[0].size();
        std::array<float,8> delayed{};float mean=0;
        for(size_t i=0;i<8;++i)
        {
            length[i]+=(target[i]-length[i])*smooth;
            const float d=readDelay(lines[i],linePosition,length[i]);
            low[i]=(1-damp)*d+damp*low[i];delayed[i]=low[i];mean+=low[i]*.125f;
        }
        float wetL=0,wetR=0;
        for(size_t i=0;i<8;++i)
        {
            // Householder feedback matrix is orthogonal; feedback is strictly < 1.
            const float reflect=2*mean-delayed[i];
            const float feed=(i%2==0?early[0]:early[1])*(settings.type==1 && i%3==0?-.20f:.20f);
            lines[i][linePosition]=feed+feedback[i]*reflect;
            wetL+=delayed[i]*(i%2==0?.25f:-.12f);wetR+=delayed[i]*(i%3==0?-.18f:.22f);
        }
        linePosition=(linePosition+1)%lines[0].size();
        std::array<float,2> reverb{wetL,wetR};
        for(size_t ch=0;ch<2;++ch){hpState[ch]=(1-hp)*reverb[ch]+hp*hpState[ch];const float high=reverb[ch]-hpState[ch];lpState[ch]=(1-lp)*high+lp*lpState[ch];reverb[ch]=lpState[ch];}
        const float mid=(reverb[0]+reverb[1])*.5f,side=(reverb[0]-reverb[1])*.5f*settings.width*.01f;
        left+=(mid+side)*revFade*settings.reverbMix*.01f*duckGain;
        right+=(mid-side)*revFade*settings.reverbMix*.01f*duckGain;
        echoLength+=(echoTarget-echoLength)*smooth;
        const float dl=readDelay(echo[0],position,echoLength),dr=readDelay(echo[1],position,echoLength);
        const float f=std::clamp(settings.feedback*.01f,0.f,.75f);
        delayLow[0]+=.35f*((settings.ping?dr:dl)-delayLow[0]);delayLow[1]+=.35f*((settings.ping?dl:dr)-delayLow[1]);
        echo[0][position]=in[0]+f*delayLow[0];echo[1][position]=(settings.ping?0.f:in[1])+f*delayLow[1];
        left+=dl*delFade*settings.delayMix*.01f*duckGain;right+=dr*delFade*settings.delayMix*.01f*duckGain;
        position=(position+1)%echo[0].size();
    }
private:
    double fs=48000;
    size_t position=0,prePosition=0,linePosition=0;
    std::array<std::vector<float>,2> echo,pre;
    std::array<std::vector<float>,8> lines;
    std::array<float,8> low{},length{},target{},feedback{};
    std::array<float,2> hpState{},lpState{},delayLow{};
    float revFade=0,delFade=0,echoLength=1,echoTarget=1,duckEnvelope=0,damp=0,hp=0,lp=0,smooth=0;
};

class Choir
{
public:
    bool enabled=false; int voices=8; float pitch=8,timing=10,vibrato=8,width=100,mix=25;
    void prepare(double rate){fs=rate;index=0;clock=0;fade=0;for(auto& b:ring)b.assign(size_t(fs*.16)+8,0);configure();}
    void configure() noexcept
    {
        count=std::clamp(voices,4,32);
        const float ratio=std::pow(2.f,pitch/1200.f)-1;
        vibDepth=(std::pow(2.f,vibrato/1200.f)-1)*float(fs)/(2*pi*5.1f);
        for(int v=0;v<count;++v)
        {
            const size_t i=size_t(v);frequency[i]=.21f+float(v%7)*.037f;phaseSeed[i]=float(v)*2.399963f;
            depth[i]=ratio*float(fs)/(2*pi*frequency[i]);base[i]=float(fs)*(.060f+timing*.0005f*std::sin(phaseSeed[i]));
            const float pan=std::clamp((2.f*float(v)/float(count-1)-1)*width*.01f,-1.f,1.f);
            gainL[i]=std::sqrt(1-pan)/float(count);gainR[i]=std::sqrt(1+pan)/float(count);
        }
        fadeCoefficient=1-msCoefficient(20,fs);
    }
    void process(float& left,float& right) noexcept
    {
        fade+=(float(enabled)-fade)*fadeCoefficient;
        ring[0][index]=left;ring[1][index]=right;
        if(fade>1.e-5f)
        {
            float l=0,r=0;
            const double t=double(clock)/fs;
            for(int v=0;v<count;++v)
            {
                const size_t i=size_t(v);
                const float delay=std::max(1.f,base[i]+depth[i]*std::sin(float(t)*2*pi*frequency[i]+phaseSeed[i])+vibDepth*std::sin(float(t)*2*pi*5.1f+phaseSeed[i]));
                l+=readDelay(ring[0],index,delay)*gainL[i];r+=readDelay(ring[1],index,delay)*gainR[i];
            }
            const float wet=fade*mix*.01f;left=left*(1-wet)+l*wet;right=right*(1-wet)+r*wet;
        }
        index=(index+1)%ring[0].size();++clock;
    }
private:
    std::array<std::vector<float>,2> ring;
    double fs=48000;size_t index=0;uint64_t clock=0;float fade=0,fadeCoefficient=.001f,vibDepth=0;int count=8;
    std::array<float,32> frequency{},phaseSeed{},depth{},base{},gainL{},gainR{};
};

class KeyDetector
{
public:
    void reset() noexcept { histogram.fill(0);count=0;key=-1;minor=false;confidence=0; }
    void add(float midi,float certainty) noexcept
    {
        if(midi<0||certainty<.8f)return;
        const int pitchClass=(int(std::lround(midi))%12+12)%12;
        histogram[size_t(pitchClass)]+=certainty;++count;
        if(count%32==0) analyse();
    }
    int key=-1;bool minor=false;float confidence=0;
private:
    void analyse() noexcept
    {
        // Krumhansl-Schmuckler pitch-class profile correlation.
        constexpr float majorProfile[]={6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
        constexpr float minorProfile[]={6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};
        float mean=0;for(auto x:histogram)mean+=x/12.f;
        float best=-2,second=-2;
        for(int mode=0;mode<2;++mode)for(int root=0;root<12;++root)
        {
            const auto* profile=mode==0?majorProfile:minorProfile;float pm=0;for(int i=0;i<12;++i)pm+=profile[i]/12.f;
            float xy=0,xx=0,yy=0;
            for(int i=0;i<12;++i){const float x=histogram[size_t((root+i)%12)]-mean,y=profile[i]-pm;xy+=x*y;xx+=x*x;yy+=y*y;}
            const float score=xy/std::sqrt(std::max(1.e-10f,xx*yy));
            if(score>best){second=best;best=score;key=root;minor=mode!=0;}else second=std::max(second,score);
        }
        confidence=std::clamp((best-second)*3.f,0.f,1.f)*std::min(1.f,float(count)/600.f);
    }
    std::array<float,12> histogram{};int count=0;
};
}
