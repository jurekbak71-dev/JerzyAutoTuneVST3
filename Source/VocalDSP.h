#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace jerzy
{
// YIN cumulative mean normalised difference (de Cheveigne & Kawahara, 2002).
// Fixed time/frequency bounds; fourth-order anti-alias filter before decimation.
class PitchTracker
{
public:
    void prepare(double rate)
    {
        decimation = std::max(1, static_cast<int>(rate / 12000.0));
        analysisRate = rate / decimation;
        minLag = std::max(2, static_cast<int>(analysisRate / 1200.0));
        maxLag = static_cast<int>(std::ceil(analysisRate / 50.0));
        length = 3 * (maxLag + 1);
        hop = std::max(1, static_cast<int>(analysisRate * 0.005));
        ring.assign(static_cast<size_t>(length), 0.0f);
        ordered.resize(ring.size()); scores.resize(static_cast<size_t>(maxLag + 2));
        write = filled = count = phase = 0; midi = -1; confidence = 0; pending = -1; stable = 0;
        constexpr double pi = 3.14159265358979323846;
        for (size_t i = 0; i < filters.size(); ++i)
        {
            const double omega = 2.0 * pi * 3600.0 / rate;
            const double c = std::cos(omega), s = std::sin(omega);
            const double q = i == 0 ? 0.5411961001 : 1.306562965;
            const double a = s / (2.0 * q), inv = 1.0 / (1.0 + a);
            filters[i] = { static_cast<float>((1-c)*0.5*inv), static_cast<float>((1-c)*inv),
                           static_cast<float>(-2*c*inv), static_cast<float>((1-a)*inv), 0, 0 };
        }
    }
    bool push(float input) noexcept
    {
        if (!std::isfinite(input)) input = 0;
        input=std::clamp(input,-16.f,16.f);
        for (auto& f : filters)
        {
            const float y = f.b0 * input + f.z1;
            f.z1 = f.b1 * input - f.a1 * y + f.z2;
            f.z2 = f.b0 * input - f.a2 * y;
            input = y;
        }
        if (++phase < decimation) return false;
        phase = 0; ring[static_cast<size_t>(write)] = input; write = (write + 1) % length;
        filled = std::min(filled + 1, length);
        if (++count < hop) return false;
        count = 0;
        if (filled < length) return false;
        analyse(); return true;
    }
    float pitch() const noexcept { return midi; }
    float certainty() const noexcept { return confidence; }
private:
    void analyse() noexcept
    {
        double mean = 0, energy = 0;
        for (int i = 0; i < length; ++i)
        {
            const float x = ring[static_cast<size_t>((write + i) % length)];
            ordered[static_cast<size_t>(i)] = x; mean += x; energy += x*x;
        }
        mean /= length;
        if (energy / length - mean*mean < 2.0e-6) { unvoiced(); return; }
        double sum = 0;
        scores[0] = 1;
        const int window = 2 * maxLag;
        for (int lag = 1; lag <= maxLag + 1; ++lag)
        {
            double difference = 0;
            for (int i = 0; i < window; ++i)
            {
                const double d = ordered[static_cast<size_t>(i)] - ordered[static_cast<size_t>(i+lag)];
                difference += d*d;
            }
            sum += difference;
            scores[static_cast<size_t>(lag)] = static_cast<float>(difference * lag / (sum + 1.0e-20));
        }
        int best = minLag;
        float bestValue = 1.0f;
        for (int lag = 2; lag <= maxLag; ++lag)
        {
            const float l=scores[static_cast<size_t>(lag-1)], c=scores[static_cast<size_t>(lag)], r=scores[static_cast<size_t>(lag+1)];
            if (c > l || c > r) continue;
            const float curvature=l-2*c+r;
            const float delta=curvature>1.e-9f ? std::clamp(.5f*(l-r)/curvature,-.5f,.5f) : 0;
            const float value=c-.25f*(l-r)*delta;
            if(value<bestValue) { best=lag; bestValue=value; }
            if(value<.15f) break;
        }
        confidence = std::clamp(1.0f - bestValue, 0.0f, 1.0f);
        if (confidence < 0.80f || best < minLag) { unvoiced(); return; }
        const float l=scores[static_cast<size_t>(best-1)], c=scores[static_cast<size_t>(best)], r=scores[static_cast<size_t>(best+1)];
        const float curvature = l - 2*c + r;
        const float delta = std::abs(curvature) > 1.0e-9f ? std::clamp(0.5f*(l-r)/curvature, -0.5f, 0.5f) : 0;
        const float candidate = 69.0f + 12.0f * std::log2(static_cast<float>(analysisRate) / ((best+delta)*440.0f));
        if (midi >= 0 && std::abs(candidate-midi) > 1.0f)
        {
            if (std::abs(candidate-pending) < 0.5f) ++stable;
            else { pending = candidate; stable = 1; }
            if (stable < 2) return;
        }
        midi = candidate; stable = 0;
    }
    void unvoiced() noexcept { midi = -1; confidence = 0; stable = 0; pending = -1; }
    struct Filter { float b0=0,b1=0,a1=0,a2=0,z1=0,z2=0; };
    std::array<Filter, 2> filters;
    std::vector<float> ring, ordered, scores;
    double analysisRate=8000;
    int decimation=1,minLag=6,maxLag=160,length=483,hop=40,write=0,filled=0,count=0,phase=0,stable=0;
    float midi=-1,confidence=0,pending=-1;
};

// Stereo-linked lookahead sample-peak limiter. All storage allocated in prepare.
// Sliding maximum includes the delayed sample; gain never exceeds its safe bound.
class OutputLimiter
{
public:
    void prepare(double rate)
    {
        delay = std::max(1, static_cast<int>(rate * 0.003));
        audio.assign(static_cast<size_t>(delay + 1), {});
        peaks.assign(static_cast<size_t>(delay + 2), {});
        head=tail=0; write=0; clock=0; gain=1;
        release = static_cast<float>(std::exp(-1.0/(0.080*rate)));
    }
    int latency() const noexcept { return delay; }
    void process(float& left, float& right, float ceiling, bool enabled) noexcept
    {
        if (!std::isfinite(left)) left=0;
        if (!std::isfinite(right)) right=0;
        const float peak = std::max(std::abs(left), std::abs(right));
        const auto capacity = peaks.size();
        while (head != tail && peaks[head].time + static_cast<uint64_t>(delay) < clock) head = (head+1)%capacity;
        while (head != tail && peaks[(tail+capacity-1)%capacity].value <= peak) tail = (tail+capacity-1)%capacity;
        peaks[tail] = {clock, peak}; tail=(tail+1)%capacity;
        const float required = enabled ? std::min(1.0f, ceiling/std::max(1.0e-12f,peaks[head].value)) : 1.0f;
        gain = std::min(required, 1.0f + (gain-1.0f)*release);
        audio[static_cast<size_t>(write)]={left,right};
        const int read=(write+1)%static_cast<int>(audio.size());
        left=audio[static_cast<size_t>(read)][0]*gain;
        right=audio[static_cast<size_t>(read)][1]*gain;
        write=read; ++clock;
    }
private:
    struct Peak { uint64_t time=0; float value=0; };
    std::vector<std::array<float,2>> audio;
    std::vector<Peak> peaks;
    size_t head=0,tail=0;
    uint64_t clock=0;
    int delay=1,write=0;
    float gain=1,release=0;
};
}
