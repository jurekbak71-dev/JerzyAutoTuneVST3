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
        }
        std::cout<<"PASS: "<<checks<<" DSP cases: sample rates, harmonic pitch, noise/DC rejection, stereo ceiling, latency\n";
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
