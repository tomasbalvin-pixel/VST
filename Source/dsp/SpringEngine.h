#pragma once

#include "EchoEngine.h"

namespace patina
{
/**
    Spring reverb tank (Accutronics / Fender style), after the
    dispersive-delay-line approach of Parker & Välimäki.

    Each spring is a feedback loop of: a cascade of stretched first-order
    allpasses (dispersion: high frequencies travel slower, producing the
    characteristic "drip" chirp), a transit delay, and a roll-off low-pass
    around 4-5 kHz where real springs stop transmitting. Two springs per side,
    each a different length, give the stereo image. The input driver
    transformer is modelled by a tanh stage that Age pushes harder, which makes
    transients "boing".
*/
class SpringEngine
{
public:
    struct Params
    {
        float preDelayMs = 0.0f;
        float decaySec = 2.0f;
        float tone = 0.6f;
        float age = 0.3f;
        SpaceSat sat;
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        preDelay.prepare ((int) (0.31f * fs));

        // Stretch factor K maps the allpass chain's dispersion into 0..~4.4 kHz.
        stretch = juce::jlimit (1, kRing - 1, (int) std::round (fs / (2.0f * 4400.0f)));

        constexpr float lengthsMs[kSprings] = { 33.7f, 41.3f, 36.1f, 44.9f };
        for (int s = 0; s < kSprings; ++s)
        {
            auto& sp = springs[(size_t) s];
            sp.lengthSamples = lengthsMs[s] * 0.001f * fs;
            sp.line.prepare ((int) (sp.lengthSamples + 0.002f * fs) + 8);
            sp.drift.prepare (fs, 0.9f + 0.17f * (float) s, 0x1234567u + (uint32_t) s * 7919u);
        }

        inputHP.setCutoff (180.0f, fs);
        for (auto& f : outHP) f.setCutoff (110.0f, fs);
        reset();
    }

    void reset()
    {
        preDelay.clear();
        inputHP.reset();
        for (auto& f : outHP) f.reset();
        for (auto& f : outLP) f.reset();
        for (auto& sp : springs)
        {
            for (auto& stage : sp.hist)
                std::fill (std::begin (stage), std::end (stage), 0.0f);
            sp.idx = 0;
            sp.line.clear();
            sp.lp.reset();
            sp.fb = 0.0f;
        }
        satState = {};
        preDelaySmoothed = -1.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        targetPreDelay = std::max (1.0f, p.preDelayMs * 0.001f * fs);

        const float loopLp = expMap (2600.0f, 5200.0f, p.tone) * (1.0f - 0.25f * p.age);
        const float outCut = expMap (2400.0f, 6500.0f, p.tone);
        for (auto& f : outLP) f.set (outCut, 0.7f, fs);

        const float chainDelaySec = (float) (kStages * stretch) / fs;
        for (auto& sp : springs)
        {
            sp.lp.set (loopLp, 0.6f, fs);
            const float loopSec = sp.lengthSamples / fs + chainDelaySec;
            sp.gain = std::min (0.985f, std::pow (10.0f, -3.0f * loopSec / std::max (0.1f, p.decaySec)));
        }
        driverGain = 1.0f + 5.0f * p.age;
        modDepth = (0.00015f + 0.0006f * p.age) * fs;
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        if (preDelaySmoothed < 0.0f) preDelaySmoothed = targetPreDelay;
        preDelaySmoothed += 0.0005f * (targetPreDelay - preDelaySmoothed);

        float x = params.sat.input (0.5f * (inL + inR), satState);
        x = inputHP.process (x);
        x = std::tanh (x * driverGain) / driverGain;

        preDelay.push (x);
        const float tankIn = preDelaySmoothed <= 1.0f ? x : preDelay.tapLinear (preDelaySmoothed);

        const float s0 = processSpring (springs[0], tankIn);
        const float s1 = processSpring (springs[1], tankIn);
        const float s2 = processSpring (springs[2], tankIn);
        const float s3 = processSpring (springs[3], tankIn);

        outL = outHP[0].process (outLP[0].processLP (0.9f * (s0 + 0.6f * s1)));
        outR = outHP[1].process (outLP[1].processLP (0.9f * (s2 + 0.6f * s3)));
    }

private:
    static constexpr int kSprings = 4;
    static constexpr int kStages = 80;
    static constexpr int kRing = 32;       // per-stage history ring (supports K up to 31)
    static constexpr float kDispersion = 0.65f;

    struct Spring
    {
        float hist[kStages + 1][kRing] {};  // hist[s] = input to stage s, hist[s+1] = its output
        int idx = 0;
        DelayBuffer line;
        SVF lp;
        Drift drift;
        float lengthSamples = 1000.0f;
        float gain = 0.5f;
        float fb = 0.0f;
    };

    float processSpring (Spring& sp, float in)
    {
        const int now = sp.idx;
        const int past = (now - stretch) & (kRing - 1);
        constexpr float a = kDispersion;

        // Stretched allpass cascade: y[n] = a x[n] + x[n-K] - a y[n-K]
        float v = in + sp.fb;
        sp.hist[0][now] = v;
        for (int s = 0; s < kStages; ++s)
        {
            v = a * v + sp.hist[s][past] - a * sp.hist[s + 1][past];
            sp.hist[s + 1][now] = v;
        }
        sp.idx = (now + 1) & (kRing - 1);

        const float d = sp.lengthSamples + modDepth * sp.drift.next();
        const float arrived = sp.line.tapLinear (std::max (1.0f, d));
        sp.line.push (v);

        const float filtered = sp.lp.processLP (arrived);
        sp.fb = sp.gain * filtered;
        return filtered;
    }

    Params params;
    Spring springs[kSprings];
    DelayBuffer preDelay;
    OnePoleHP inputHP;
    OnePoleHP outHP[2];
    SVF outLP[2];
    ShaperState satState;
    float fs = 44100.0f;
    int stretch = 5;
    float driverGain = 1.0f, modDepth = 0.0f;
    float targetPreDelay = 1.0f, preDelaySmoothed = -1.0f;
};
} // namespace patina
