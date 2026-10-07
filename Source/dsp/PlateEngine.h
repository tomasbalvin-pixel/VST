#pragma once

#include "EchoEngine.h"

namespace patina
{
/**
    EMT 140-style plate, built on Jon Dattorro's figure-of-eight tank
    ("Effect Design Part 1", JAES 1997). Dense, bright, fast-building
    diffusion with no discrete early echoes, which is what a steel plate does.
    Age deepens the tank modulation and darkens the damping, like an
    old plate with tired damping pads.
*/
class PlateEngine
{
public:
    struct Params
    {
        float preDelayMs = 10.0f;
        float decaySec = 2.5f;
        float tone = 0.6f;
        float age = 0.3f;
        SpaceSat sat;
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        scale = fs / 29761.0f;

        preDelay.prepare ((int) (0.31f * fs));
        for (int i = 0; i < 4; ++i)
            inputAP[i].prepare (len (kInputLens[i]));

        const int maxExcursion = len (48);
        tankAP1[0].prepare (len (672) + maxExcursion);
        tankAP1[1].prepare (len (908) + maxExcursion);
        delay1[0].prepare (len (4453));
        delay1[1].prepare (len (4217));
        tankAP2[0].prepare (len (1800));
        tankAP2[1].prepare (len (2656));
        delay2[0].prepare (len (3720));
        delay2[1].prepare (len (3163));
        reset();
    }

    void reset()
    {
        preDelay.clear();
        for (auto& d : inputAP) d.clear();
        for (int i = 0; i < 2; ++i)
        {
            tankAP1[i].clear(); delay1[i].clear(); tankAP2[i].clear(); delay2[i].clear();
            damp[i] = 0.0f;
        }
        bandwidthState = 0.0f;
        lfoPhase = 0.0f;
        satState = {};
        preDelaySmoothed = -1.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        targetPreDelay = std::max (1.0f, p.preDelayMs * 0.001f * fs);

        // Each branch of the tank is ~0.358 s at the reference rate and
        // applies the decay coefficient once.
        constexpr float branchSec = (672.0f + 4453.0f + 1800.0f + 3720.0f) / 29761.0f;
        decay = std::min (0.97f, std::pow (10.0f, -3.0f * branchSec / std::max (0.1f, p.decaySec)));
        decayDiffusion2 = juce::jlimit (0.25f, 0.5f, decay + 0.15f);

        bandwidth = 0.25f + 0.7495f * p.tone;
        damping = juce::jlimit (0.0f, 0.85f, 0.65f * (1.0f - p.tone) + 0.25f * p.age);
        excursion = (8.0f + 22.0f * p.age) * scale;
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        if (preDelaySmoothed < 0.0f) preDelaySmoothed = targetPreDelay;
        preDelaySmoothed += 0.0005f * (targetPreDelay - preDelaySmoothed);

        float x = params.sat.input (0.5f * (inL + inR), satState);
        preDelay.push (x);
        x = preDelay.tapLinear (preDelaySmoothed);

        bandwidthState += bandwidth * (x - bandwidthState);
        x = bandwidthState;

        x = allpass (inputAP[0], len (142), 0.75f, x);
        x = allpass (inputAP[1], len (107), 0.75f, x);
        x = allpass (inputAP[2], len (379), 0.625f, x);
        x = allpass (inputAP[3], len (277), 0.625f, x);

        lfoPhase += 1.0f / fs;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float modA = excursion * (1.0f + std::sin (kTwoPi * lfoPhase));
        const float modB = excursion * (1.0f + std::cos (kTwoPi * lfoPhase));

        const float tailL = delay2[0].tap (len (3720));
        const float tailR = delay2[1].tap (len (3163));

        // Left branch, fed by the right branch's tail.
        float l = x + decay * tailR;
        l = allpassMod (tankAP1[0], (float) len (672) + modA, -0.7f, l);
        l = delayLine (delay1[0], len (4453), l);
        damp[0] += (1.0f - damping) * (l - damp[0]);
        l = decay * damp[0];
        l = allpass (tankAP2[0], len (1800), decayDiffusion2, l);
        delay2[0].push (l);

        // Right branch, fed by the left branch's tail.
        float r = x + decay * tailL;
        r = allpassMod (tankAP1[1], (float) len (908) + modB, -0.7f, r);
        r = delayLine (delay1[1], len (4217), r);
        damp[1] += (1.0f - damping) * (r - damp[1]);
        r = decay * damp[1];
        r = allpass (tankAP2[1], len (2656), decayDiffusion2, r);
        delay2[1].push (r);

        float yl = delay1[1].tap (len (266)) + delay1[1].tap (len (2974)) - tankAP2[1].tap (len (1913))
                 + delay2[1].tap (len (1996)) - delay1[0].tap (len (1990)) - tankAP2[0].tap (len (187))
                 - delay2[0].tap (len (1066));
        float yr = delay1[0].tap (len (353)) + delay1[0].tap (len (3627)) - tankAP2[0].tap (len (1228))
                 + delay2[0].tap (len (2673)) - delay1[1].tap (len (2111)) - tankAP2[1].tap (len (335))
                 - delay2[1].tap (len (121));

        outL = 0.6f * yl;
        outR = 0.6f * yr;
    }

private:
    static constexpr int kInputLens[4] = { 142, 107, 379, 277 };

    int len (int refSamples) const { return std::max (1, (int) std::round ((float) refSamples * scale)); }

    static float allpass (DelayBuffer& d, int length, float g, float x)
    {
        const float delayed = d.tap (length);
        const float w = x + g * delayed;
        d.push (w);
        return delayed - g * w;
    }

    static float allpassMod (DelayBuffer& d, float length, float g, float x)
    {
        const float delayed = d.tapLinear (length);
        const float w = x + g * delayed;
        d.push (w);
        return delayed - g * w;
    }

    static float delayLine (DelayBuffer& d, int length, float x)
    {
        const float y = d.tap (length);
        d.push (x);
        return y;
    }

    Params params;
    DelayBuffer preDelay, inputAP[4], tankAP1[2], delay1[2], tankAP2[2], delay2[2];
    float damp[2] {};
    ShaperState satState;
    float fs = 44100.0f, scale = 1.0f;
    float decay = 0.5f, decayDiffusion2 = 0.5f, bandwidth = 0.9995f, damping = 0.0005f, excursion = 16.0f;
    float bandwidthState = 0.0f, lfoPhase = 0.0f;
    float targetPreDelay = 1.0f, preDelaySmoothed = -1.0f;
};
} // namespace patina
