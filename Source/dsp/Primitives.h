#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace patina
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

/** Exponential interpolation between lo and hi for t in [0, 1]. */
inline float expMap (float lo, float hi, float t) { return lo * std::pow (hi / lo, t); }

/** Circular delay buffer. Read with tap*() *before* push() for a delay of D samples (D >= 1). */
class DelayBuffer
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    int capacity() const { return mask - 4; }

    void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    float tap (int d) const { return buffer[(size_t) ((writePos - d) & mask)]; }

    float tapLinear (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = tap (i), b = tap (i + 1);
        return a + f * (b - a);
    }

    /** 4-point Hermite interpolation; d must be >= 2. */
    float tapHermite (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float x0 = tap (i - 1), x1 = tap (i), x2 = tap (i + 1), x3 = tap (i + 2);
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * f + c2) * f + c1) * f + x1;
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

/** One-pole lowpass, y += a (x - y). */
struct OnePoleLP
{
    float a = 1.0f, z = 0.0f;
    void setCutoff (float hz, float fs) { a = 1.0f - std::exp (-kTwoPi * std::min (hz, 0.49f * fs) / fs); }
    float process (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, float fs) { lp.setCutoff (hz, fs); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

/** Topology-preserving-transform state variable filter (Zavalishin / Simper). */
struct SVF
{
    float g = 0.0f, k = 1.4142f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;

    void set (float hz, float q, float fs)
    {
        g = std::tan (kPi * std::min (hz, 0.45f * fs) / fs);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void process (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }

    float processLP (float x) { process (x); return lp; }
    float processHP (float x) { process (x); return hp; }

    void reset() { ic1 = ic2 = lp = bp = hp = 0.0f; }
};

/** Cheap xorshift white noise in [-1, 1]. */
struct Noise
{
    uint32_t state = 0x9E3779B9u;
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

/** Slowly wandering random value in [-1, 1], used for tape drift. */
struct Drift
{
    Noise noise;
    float target = 0.0f, value = 0.0f, coef = 0.0f;
    int counter = 0, period = 1;

    void prepare (float fs, float rateHz, uint32_t seed)
    {
        noise.state = seed;
        period = std::max (1, (int) (fs / rateHz));
        coef = 1.0f - std::exp (-kTwoPi * rateHz / fs);
    }

    float next()
    {
        if (--counter <= 0)
        {
            counter = period;
            target = noise.next();
        }
        value += coef * (target - value);
        return value;
    }
};
} // namespace patina
