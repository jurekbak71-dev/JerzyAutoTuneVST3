#include "VocalDSP.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    try
    {
        int checks=0;
        constexpr double pi=3.14159265358979323846;
        for (double fs : {44100.,48000.,88200.,96000.,192000.})
        {
            for (double hz : {55.,82.4069,110.,220.,447.,880.,1100.})
                for (bool harmonic : {false,true})
                {
                    jerzy::PitchTracker tracker; tracker.prepare(fs);
                    for (int i=0;i<static_cast<int>(fs*.24);++i)
                    {
                        const double phase=2*pi*hz*i/fs;
                        const float x=static_cast<float>(harmonic ? .10*std::sin(2*phase)+.08*std::sin(3*phase)+.04*std::sin(4*phase) : .2*std::sin(phase));
                        tracker.push(x);
                    }
                    const float expected=static_cast<float>(69+12*std::log2(hz/440));
                    check(std::abs(tracker.pitch()-expected)<.15f,"pitch error fs="+std::to_string(fs)+" hz="+std::to_string(hz)+" harmonic="+std::to_string(harmonic)+" detected="+std::to_string(tracker.pitch()));
                    check(tracker.certainty()>.8f,"low periodic confidence"); ++checks;
                }
            for(double hz : {1800.,2200.})
            {
                jerzy::PitchTracker outside; outside.prepare(fs);
                for(int i=0;i<static_cast<int>(fs*.24);++i) outside.push(.2f*static_cast<float>(std::sin(2*pi*hz*i/fs)));
                check(outside.pitch()<0,"Out-of-range tone folds to a vocal octave"); ++checks;
            }
            jerzy::PitchTracker noise; noise.prepare(fs);
            std::mt19937 rng(42); std::uniform_real_distribution<float> dist(-.2f,.2f);
            int voiced=0,frames=0;
            for (int i=0;i<static_cast<int>(fs*.4);++i)
                if(noise.push(dist(rng))) { ++frames; voiced+=noise.pitch()>0 ? 1 : 0; }
            check(frames>0 && voiced==0,"noise accepted as voiced");
            for (int i=0;i<static_cast<int>(fs*.12);++i) noise.push(.15f);
            check(noise.pitch()<0,"DC accepted as voiced"); ++checks;
            jerzy::OutputLimiter limiter; limiter.prepare(fs);
            const float ceiling=.7f;
            int audible=0;
            for(int i=0;i<static_cast<int>(fs*.25);++i)
            {
                float l=i%113==0 ? 8.f : .1f, r=-l*.5f;
                limiter.process(l,r,ceiling,true);
                check(std::abs(l)<=ceiling+1.e-6f && std::abs(r)<=ceiling+1.e-6f,"output ceiling exceeded");
                if(std::abs(l)>0) { ++audible; check(std::abs(r+l*.5f)<1.e-6f,"stereo image changed"); }
            }
            check(audible>100,"limiter muted audio"); ++checks;
            limiter.prepare(fs);
            for(int i=0;i<limiter.latency()+3;++i)
            {
                float l=i==0 ? .2f : 0, r=l;
                limiter.process(l,r,ceiling,false);
                check(std::abs(l-(i==limiter.latency() ? .2f : 0))<1.e-7f,"limiter latency mismatch");
            }
            ++checks;
            // The quarter-Nyquist sinusoid has true peaks far above its
            // individual samples at a 45-degree phase. A sample-peak-only
            // limiter cannot detect this particular intersample overload.
            {
                jerzy::OutputLimiter truePeak;
                truePeak.prepare(fs);
                constexpr double phase=0.78539816339744830961566;
                const float actualAmplitude=.95f, ceiling=.7f;
                double sinPart=0,cosPart=0;
                int measured=0;
                for(int n=0;n<static_cast<int>(fs*.15);++n)
                {
                    const double angle=2*pi*0.25*double(n)+phase;
                    float left=actualAmplitude*static_cast<float>(std::sin(angle)),right=-left*.5f;
                    check(std::abs(left)<ceiling,"Adversarial true-peak test is not sample-peak-free");
                    truePeak.process(left,right,ceiling,true);
                    if(n>static_cast<int>(fs*.07))
                    {
                        sinPart+=double(left)*std::sin(angle);
                        cosPart+=double(left)*std::cos(angle);
                        ++measured;
                    }
                }
                const double reconstructedAmplitude=2*std::hypot(sinPart,cosPart)/measured;
                check(reconstructedAmplitude<ceiling*1.01,
                      "Four-phase true-peak limiter did not contain intersample sine amplitude");
                check(reconstructedAmplitude>ceiling*.45,
                      "True-peak limiter attenuated a normal sine excessively");
                ++checks;
            }
        }
        // Independent 8x/32-tap reconstruction of the LIMITED waveform,
        // not the detector's internal 4x/16-tap estimate. This tests
        // intersample overshoots on multitone and high-frequency content.
        {
            constexpr int oversampling=8,filterTaps=32,centre=16;
            constexpr double fs=48000.,ceiling=.7;
            std::array<std::array<double,filterTaps>,oversampling> reference{};
            for(int p=0;p<oversampling;++p)
            {
                double sum=0;
                for(int k=0;k<filterTaps;++k)
                {
                    const double distance=double(k-centre)+double(p)/oversampling;
                    const double sinc=std::abs(distance)<1.e-9?1.0:std::sin(pi*distance)/(pi*distance);
                    const double window=.42-.5*std::cos(2*pi*k/(filterTaps-1))
                                            +.08*std::cos(4*pi*k/(filterTaps-1));
                    reference[p][k]=sinc*window;sum+=reference[p][k];
                }
                for(auto& coefficient:reference[p])coefficient/=sum;
            }
            for(int kind=0;kind<5;++kind)
            {
                jerzy::OutputLimiter limiter;
                limiter.prepare(fs);
                std::array<std::vector<float>,2> output;
                for(auto& ch:output)ch.reserve(12000);
                for(int n=0;n<12000;++n)
                {
                    const double t=double(n),phase=.7853981633974483;
                    const double l=kind==0?.95*std::sin(2*pi*.25*t+phase)
                        :kind==1?1.3*std::sin(2*pi*.20*t+.33)
                        :kind==2?1.3*std::sin(2*pi*.33*t+.33)
                        :kind==3?1.3*std::sin(2*pi*.45*t+.33)
                                :.6*std::sin(2*pi*.20*t)+.6*std::sin(2*pi*.37*t+.7);
                    const double r=kind==0?-.5*l:.85*std::sin(2*pi*.29*t+.24);
                    float left=float(l),right=float(r);
                    limiter.process(left,right,float(ceiling),true);
                    check(std::isfinite(left)&&std::isfinite(right),"Nonfinite output in independent true-peak test");
                    output[0].push_back(left);output[1].push_back(right);
                }
                double reconstructed=0;
                for(int n=2500;n<11500;n+=2)
                    for(int p=0;p<oversampling;++p)
                        for(int ch=0;ch<2;++ch)
                        {
                            double y=0;
                            for(int k=0;k<filterTaps;++k)
                                y+=reference[p][k]*output[ch][size_t(n-k)];
                            reconstructed=std::max(reconstructed,std::abs(y));
                        }
                check(reconstructed<ceiling*1.025,
                      "8x independent true-peak reconstruction exceeds ceiling tolerance");
                ++checks;
            }
        }
        // Compare ADAA against the previous direct tanh at a frequency
        // whose third harmonic folds from 30 kHz down to 18 kHz at 48 kHz.
        for(float bias : {0.f,.15f,.38f})
        {
            jerzy::AntialiasedTanh shaper;
            constexpr int fs=48000, total=48000, warmup=24000;
            constexpr float drive=8.f, amplitude=.35f;
            double refReal=0,refImag=0,aaReal=0,aaImag=0;
            double outputEnergy=0;
            for(int i=0;i<total;++i)
            {
                const float x=amplitude*static_cast<float>(std::sin(2*pi*10000.0*double(i)/fs));
                const float direct=std::tanh(drive*x+bias);
                const float filtered=shaper.process(x,drive,bias);
                check(std::isfinite(filtered),"Nonfinite ADAA sample");
                if(i>=warmup)
                {
                    const double angle=2*pi*18000*i/fs;
                    refReal+=double(direct)*std::cos(angle);
                    refImag+=double(direct)*std::sin(angle);
                    aaReal+=double(filtered)*std::cos(angle);
                    aaImag+=double(filtered)*std::sin(angle);
                    outputEnergy+=double(filtered)*filtered;
                }
            }
            const double refAlias=std::hypot(refReal,refImag);
            const double aaAlias=std::hypot(aaReal,aaImag);
            check(refAlias>1000 && aaAlias<refAlias*.55,
                  "ADAA failed to reduce folded 18-kHz saturation harmonic by >= 5 dB");
            check(outputEnergy>100,"ADAA muted saturated audio");
            shaper.reset();
            check(std::abs(shaper.process(.35f,drive,bias)-std::tanh(drive*.35f+bias))<1.e-6f,
                  "ADAA reentry is not initialised to an ordinary tanh sample");
            ++checks;
        }
        std::cout<<"PASS: "<<checks<<" DSP cases: sample rates, harmonic pitch, noise/DC rejection, stereo ceiling, latency, saturation aliasing\n";
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
