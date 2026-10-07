#pragma once

#include "Primitives.h"
#include "Saturation.h"

namespace patina
{
/** Saturation settings used when the saturator sits inside the space engine ("Space" position). */
struct SpaceSat
{
    bool enabled = false;
    SatType type = SatType::Tape;
    float gain = 1.0f;    // linear drive
    float amount = 0.0f;  // drive normalised 0..1
    float blend = 1.0f;

    /** Insert-style saturation for signals entering the engine. */
    float input (float x, ShaperState& s) const
    {
        if (! enabled) return x;
        const float y = Shaper::insert (type, x, gain, amount, 1.0f, s);
        return x + blend * (y - x);
    }

    /**
        Unity small-signal saturation for feedback paths. The clip threshold
        (1/sqrt(gain)) matches the peak level of a driven input, so each repeat
        is squashed a little further without the loop gain ever exceeding the
        Feedback setting.
    */
    float loop (float x, ShaperState& s) const
    {
        if (! enabled) return x;
        const float g = std::sqrt (gain);
        const float y = type == SatType::Crush ? Shaper::crush (x, amount, 1.0f, s)
                                               : Shaper::shape (type, x * g, amount, 1.0f, s) / g;
        return x + blend * (y - x);
    }
};

/**
    Tape echo (Echoplex / Space Echo style) and bucket-brigade analog delay.

    Tape:  slow glide on time changes (pitch-bending like a varispeed motor),
           wow + flutter + random drift, head-bump high-pass and tape-roll-off
           low-pass inside the loop, tanh tape compression, hiss.
    BBD:   clock-dependent bandwidth (longer delay = darker, as with a real
           4096-stage BBD), triangle-LFO chorus wobble, op-amp-style hard knee,
           compander noise.
*/
class EchoEngine
{
public:
    enum class Flavor { Tape, BBD };

    struct Params
    {
        float delayMs = 300.0f;
        float feedback = 0.4f;   // 0..1.1
        float tone = 0.6f;       // 0..1
        float age = 0.3f;        // 0..1
        SpaceSat sat;
    };

    explicit EchoEngine (Flavor f) : flavor (f) {}

    void prepare (double sampleRate, float maxDelayMs)
    {
        fs = (float) sampleRate;
        const int maxSamples = (int) (maxDelayMs * 0.001f * fs) + 64;
        for (auto& c : chans)
        {
            c.line.prepare (maxSamples);
            c.hp.setCutoff (flavor == Flavor::Tape ? 90.0f : 60.0f, fs);
        }
        maxDelaySamples = (float) chans[0].line.capacity() - 4.0f;

        drift.prepare (fs, 0.7f, 0xC0FFEEu);
        glideCoef = 1.0f - std::exp (-1.0f / ((flavor == Flavor::Tape ? 0.18f : 0.05f) * fs));
        reset();
    }

    void reset()
    {
        for (auto& c : chans)
        {
            c.line.clear();
            c.lp.reset();
            c.hp.reset();
            c.sat = {};
            c.inSat = {};
            c.delay = -1.0f;
        }
        wowPhase = flutterPhase = lfoPhase = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        targetDelay = juce::jlimit (4.0f, maxDelaySamples, p.delayMs * 0.001f * fs);

        float cutoff;
        if (flavor == Flavor::Tape)
        {
            cutoff = expMap (1400.0f, 14000.0f, p.tone) * (1.0f - 0.55f * p.age);
            for (auto& c : chans) c.lp.set (cutoff, 0.62f, fs);
            hissLevel = p.age * p.age * 0.0012f;
        }
        else
        {
            // 4096-stage BBD: clock = N / (2 * delay); usable bandwidth ~ clock / 2.5
            const float delaySec = targetDelay / fs;
            const float clockLimit = 4096.0f / (2.0f * delaySec) / 2.5f;
            cutoff = std::min (expMap (1200.0f, 11000.0f, p.tone), std::max (900.0f, clockLimit));
            cutoff *= (1.0f - 0.35f * p.age);
            for (auto& c : chans) c.lp.set (cutoff, 0.75f, fs);
            hissLevel = 0.0001f + p.age * p.age * 0.0018f;
        }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        float modL = 0.0f, modR = 0.0f;
        const float age = params.age;

        if (flavor == Flavor::Tape)
        {
            wowPhase += 0.55f / fs;          if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            flutterPhase += 6.8f / fs;       if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
            const float dr = drift.next();

            const float wowL = std::sin (kTwoPi * wowPhase) + 0.6f * dr;
            const float wowR = std::sin (kTwoPi * wowPhase + 1.3f) + 0.6f * dr;
            const float flL = std::sin (kTwoPi * flutterPhase);
            const float flR = std::sin (kTwoPi * flutterPhase + 2.1f);

            const float d = chans[0].delay > 0.0f ? chans[0].delay : targetDelay;
            const float wowDepth = d * (0.0006f + 0.0035f * age);
            const float flutterDepth = d * 0.0005f * age + 1.2f * age;
            modL = wowL * wowDepth + flL * flutterDepth;
            modR = wowR * wowDepth + flR * flutterDepth;
        }
        else
        {
            lfoPhase += 0.37f / fs; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            auto tri = [] (float ph) { return 4.0f * std::abs (ph - 0.5f) - 1.0f; };
            const float depth = (0.0002f + 0.0018f * age) * fs;
            modL = tri (lfoPhase) * depth;
            float phR = lfoPhase + 0.25f; if (phR >= 1.0f) phR -= 1.0f;
            modR = tri (phR) * depth;
        }

        outL = processChannel (chans[0], inL, modL);
        outR = processChannel (chans[1], inR, modR);
    }

private:
    struct Channel
    {
        DelayBuffer line;
        SVF lp;
        OnePoleHP hp;
        ShaperState sat, inSat;
        float delay = -1.0f;
    };

    float processChannel (Channel& c, float in, float mod)
    {
        if (c.delay < 0.0f)
            c.delay = targetDelay;
        c.delay += glideCoef * (targetDelay - c.delay);

        const float d = juce::jlimit (2.0f, maxDelaySamples, c.delay + mod);
        const float y = c.line.tapHermite (d);

        float fb = c.hp.process (y * params.feedback);
        fb = params.sat.loop (fb, c.sat);

        // Medium compression: tape soaks up level softly, BBD's op-amps clip harder.
        if (flavor == Flavor::Tape)
            fb = std::tanh (fb);
        else
            fb = fb > 1.5f ? 1.0f : (fb < -1.5f ? -1.0f : fb - (4.0f / 27.0f) * fb * fb * fb);

        const float write = c.lp.processLP (params.sat.input (in, c.inSat) + fb) + hissLevel * noise.next();
        c.line.push (write);
        return y;
    }

    Flavor flavor;
    Params params;
    Channel chans[2];
    Drift drift;
    Noise noise;
    float fs = 44100.0f;
    float targetDelay = 1000.0f, maxDelaySamples = 1000.0f;
    float glideCoef = 0.001f;
    float wowPhase = 0.0f, flutterPhase = 0.0f, lfoPhase = 0.0f;
    float hissLevel = 0.0f;
};
} // namespace patina
