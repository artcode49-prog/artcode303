// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Common.h - shared DSP utilities for the TB-303 emulation core.
// Pure C++ (no JUCE dependency) so the DSP can be tested standalone.
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef TB303_PI
#define TB303_PI 3.14159265358979323846
#endif

namespace tb303
{

inline double clampd (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float  clampf (float  v, float  lo, float  hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int    clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Cheap, monotonic tanh approximation (max error ~1e-3, |x|<3).
inline double fastTanh (double x)
{
    if (x < -3.0) return -1.0;
    if (x >  3.0) return  1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

// Soft limiter: transparent below the knee, gently saturating above it and
// bounded at 1.0. Used as the instrument's output stage.
inline double softLimit (double x, double knee = 0.75)
{
    const double a = std::fabs (x);
    if (a <= knee)
        return x;
    const double sign = x < 0.0 ? -1.0 : 1.0;
    const double room = 1.0 - knee;
    return sign * (knee + room * std::tanh ((a - knee) / room));
}

// PolyBLEP step correction, t = phase in [0,1), dt = phase increment.
inline double polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// ---------------------------------------------------------------- parameter mappings
// All knobs are 0..1 for easy DAW automation; mappings below reproduce the
// ranges of the original hardware as closely as is musically sensible.

// Cut-off knob -> Hz. The 303 VCF sweeps roughly 30 Hz .. 5.5 kHz.
inline double cutoffKnobToHz (double knob)
{
    return 30.0 * std::pow (5500.0 / 30.0, clampd (knob, 0.0, 1.0));
}

// Decay knob -> seconds (short, snappy to long, rubbery).
inline double decayKnobToSeconds (double knob)
{
    return 0.035 * std::pow (2500.0 / 35.0, clampd (knob, 0.0, 1.0));
}

// Resonance knob -> ladder feedback coefficient.
// k = 4.0 is the self-oscillation threshold of a 4-pole ladder, so with this
// curve the hardware's behaviour is reproduced: the filter starts singing at
// ~83% of the knob and screams at the top of the range.
inline double resonanceKnobToK (double knob)
{
    return clampd (knob, 0.0, 1.0) * 4.7 * std::pow (clampd (knob, 0.0, 1.0), 0.2);
}

inline double tuningKnobToCents (double knob) { return (clampd (knob, 0.0, 1.0) - 0.5) * 200.0; } // +/- 100 cents

// ---------------------------------------------------------------- Biquad (for decimation)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (double fc, double q, double sr)
    {
        fc = clampd (fc, 1.0, 0.49 * sr);
        const double w  = 2.0 * TB303_PI * fc / sr;
        const double cw = std::cos (w);
        const double sw = std::sin (w);
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - cw) * 0.5) / a0;
        b1 = (1.0 - cw) / a0;
        b2 = b0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0.0; }
};

// 6th order Butterworth anti-aliasing / decimation filter.
struct Butterworth6
{
    Biquad s[3];

    void setup (double fc, double sr)
    {
        static const double qs[3] = { 0.5176380902050415, 0.7071067811865476, 1.9318516525781366 };
        for (int i = 0; i < 3; ++i)
            s[i].setLowpass (fc, qs[i], sr);
    }

    inline double process (double x)
    {
        for (int i = 0; i < 3; ++i)
            x = s[i].process (x);
        return x;
    }

    void reset() { for (auto& b : s) b.reset(); }
};

} // namespace tb303
