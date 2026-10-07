#pragma once

#include "Primitives.h"

namespace patina
{
/**
    Control-rate LFO, bipolar output in [-1, 1].
    Free-running, or phase-locked to the host transport when synced.
*/
class Lfo
{
public:
    enum class Shape { Sine = 0, Triangle, Square, Saw, SampleHold, Drift };

    void reset()
    {
        absPhase = 0.0;
        current = noise.next();
        next = noise.next();
    }

    /** Advances by `seconds` at `rateHz`. */
    void advance (double seconds, double rateHz)
    {
        setPhase (absPhase + seconds * rateHz);
    }

    /** Sets the absolute phase in cycles. Crossing into a new cycle refreshes the random shapes. */
    void setPhase (double newAbsPhase)
    {
        if ((int64_t) std::floor (newAbsPhase) != (int64_t) std::floor (absPhase))
        {
            current = next;
            next = noise.next();
        }
        absPhase = newAbsPhase;
    }

    /** Phase within the current cycle, [0, 1). */
    float getPhase() const { return (float) (absPhase - std::floor (absPhase)); }

    float value (Shape shape) const { return valueAt (shape, getPhase()); }

    /** Shape evaluated at an arbitrary phase (the random shapes use the current segment). */
    float valueAt (Shape shape, float ph) const
    {
        switch (shape)
        {
            case Shape::Sine:       return std::sin (kTwoPi * ph);
            case Shape::Triangle:   return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case Shape::Square:     return ph < 0.5f ? 1.0f : -1.0f;
            case Shape::Saw:        return 1.0f - 2.0f * ph;
            case Shape::SampleHold: return current;
            case Shape::Drift:
            {
                const float t = 0.5f - 0.5f * std::cos (kPi * ph);
                return current + t * (next - current);
            }
        }
        return 0.0f;
    }

private:
    double absPhase = 0.0;
    Noise noise;
    float current = 0.0f, next = 0.0f;
};
} // namespace patina
