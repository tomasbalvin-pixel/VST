#pragma once

#include "Primitives.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace patina
{
enum class SatType { Tape = 0, Tube, Fuzz, Crush };

/** Per-channel state for the stateful shapers (sample-and-hold for Crush). */
struct ShaperState
{
    float held = 0.0f;
    float phase = 0.0f;
};

/**
    Static waveshapers. Every curve has unity slope at the origin so the caller
    controls small-signal gain explicitly (drive and make-up are applied outside).
*/
struct Shaper
{
    // Tape: biased tanh. The bias yields a little 2nd harmonic, like an
    // imperfectly biased tape head.
    static float tape (float u)
    {
        constexpr float b = 0.12f;
        static const float tb = std::tanh (b);
        static const float slope = 1.0f - tb * tb;
        return (std::tanh (u + b) - tb) / slope;
    }

    // Tube: single-ended triode-style asymmetry. Soft on the positive swing,
    // earlier and harder on the negative, generating even harmonics.
    static float tube (float u)
    {
        return u >= 0.0f ? 1.0f - std::exp (-u)
                         : -(1.0f - std::exp (1.6f * u)) / 1.6f;
    }

    // Germanium fuzz: asymmetric cubic soft-knee clip with a low negative rail.
    static float fuzz (float u)
    {
        auto knee = [] (float v)
        {
            if (v >= 1.5f) return 1.0f;
            if (v <= -1.5f) return -1.0f;
            return v - (4.0f / 27.0f) * v * v * v;
        };
        return u >= 0.0f ? 0.85f * knee (u / 0.85f) : 0.45f * knee (u / 0.45f);
    }

    // Crush: soft limit, quantise, then sample-and-hold.
    // amount01 sets bit depth (16 -> 4 bits) and hold length (1 -> 24 base-rate samples).
    static float crush (float x, float amount01, float osFactor, ShaperState& s)
    {
        const float bits = 16.0f - 12.0f * std::pow (amount01, 0.7f);
        const float levels = std::exp2 (bits - 1.0f);
        const float hold = (1.0f + 23.0f * amount01 * amount01) * osFactor;

        s.phase += 1.0f;
        if (s.phase >= hold)
        {
            s.phase -= hold;
            const float limited = std::tanh (x);
            s.held = std::round (limited * levels) / levels;
        }
        return s.held;
    }

    /** Shapes u (already multiplied by drive gain). For Crush, amount01 is used instead of gain. */
    static float shape (SatType type, float u, float amount01, float osFactor, ShaperState& s)
    {
        switch (type)
        {
            case SatType::Tape:  return tape (u);
            case SatType::Tube:  return tube (u);
            case SatType::Fuzz:  return fuzz (u);
            case SatType::Crush: return crush (u, amount01, osFactor, s);
        }
        return u;
    }

    /**
        Full insert-style stage: drive, shape, make-up gain.
        Make-up of 1/sqrt(gain) keeps perceived loudness roughly steady while
        still letting drive push things harder.
    */
    static float insert (SatType type, float x, float gain, float amount01, float osFactor, ShaperState& s)
    {
        if (type == SatType::Crush)
            return shape (type, x * (1.0f + amount01), amount01, osFactor, s) / (1.0f + 0.5f * amount01);
        return shape (type, x * gain, amount01, osFactor, s) / std::sqrt (gain);
    }
};

/**
    Oversampled block saturator used for the Pre and Post positions.
    The oversampler always runs so the plugin's latency never changes,
    even when the stage is bypassed.
*/
class BlockSaturator
{
public:
    static constexpr int kOversamplingOrder = 2; // 4x
    static constexpr float kOsFactor = 4.0f;

    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        fs = (float) sampleRate;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, (size_t) kOversamplingOrder,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversampler->initProcessing ((size_t) maxBlock);

        states.assign ((size_t) numChannels, {});
        toneFilters.assign ((size_t) numChannels, {});
        dcBlockers.assign ((size_t) numChannels, {});
        for (auto& dc : dcBlockers)
            dc.setCutoff (12.0f, fs);
        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();
        for (auto& s : states) s = {};
        for (auto& f : toneFilters) f.reset();
        for (auto& f : dcBlockers) f.reset();
        firstBlock = true;
    }

    int getLatencySamples() const
    {
        return oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    }

    void setParameters (SatType t, float driveDb, float tone01, float blend01)
    {
        type = t;
        targetGain = dbToGain (driveDb);
        amount = juce::jlimit (0.0f, 1.0f, driveDb / 36.0f);
        targetBlend = blend01;
        toneHz = expMap (900.0f, 22000.0f, tone01);
        if (firstBlock)
        {
            gain = targetGain;
            blend = targetBlend;
            firstBlock = false;
        }
    }

    void process (juce::AudioBuffer<float>& buffer, bool active)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampler->processSamplesUp (block);

        const int n = (int) up.getNumSamples();
        const int numCh = (int) up.getNumChannels();

        if (active && n > 0)
        {
            const float osRate = fs * kOsFactor;
            const float gStep = (targetGain - gain) / (float) n;
            const float bStep = (targetBlend - blend) / (float) n;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                auto& st = states[(size_t) ch];
                auto& tone = toneFilters[(size_t) ch];
                tone.set (toneHz, 0.6f, osRate);

                float g = gain, b = blend;
                for (int i = 0; i < n; ++i)
                {
                    const float x = d[i];
                    const float wet = tone.processLP (Shaper::insert (type, x, g, amount, kOsFactor, st));
                    d[i] = x + b * (wet - x);
                    g += gStep;
                    b += bStep;
                }
            }
            gain = targetGain;
            blend = targetBlend;
        }

        oversampler->processSamplesDown (block);

        if (active)
        {
            for (int ch = 0; ch < (int) buffer.getNumChannels(); ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                auto& dc = dcBlockers[(size_t) ch];
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    d[i] = dc.process (d[i]);
            }
        }
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<ShaperState> states;
    std::vector<SVF> toneFilters;
    std::vector<OnePoleHP> dcBlockers;

    float fs = 44100.0f;
    SatType type = SatType::Tape;
    float gain = 1.0f, targetGain = 1.0f;
    float blend = 1.0f, targetBlend = 1.0f;
    float amount = 0.0f;
    float toneHz = 22000.0f;
    bool firstBlock = true;
};
} // namespace patina
