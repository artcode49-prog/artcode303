// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Filter.h - TB-303 voltage controlled filter (diode ladder).
//
// The 303 uses a 4-pole diode ladder with a resonance feedback loop taken from
// the last pole. Its defining character comes from the non-linear feedback
// path: the resonance compresses and thins the bass out as it is turned up and
// eventually self-oscillates. This is a zero-delay-feedback (topology
// preserving transform) 4-pole ladder with a saturating feedback path, solved
// with a couple of fixed point iterations per sample.
#pragma once

#include "Tb303Common.h"

namespace tb303
{

class DiodeLadder
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 4; ++i) s[i] = 0.0;
    }

    void setCutoff (double hz)
    {
        hz = clampd (hz, 8.0, sampleRate * 0.45);
        g = std::tan (TB303_PI * hz / sampleRate);
        G = g / (1.0 + g);
    }

    void setResonance (double k) { K = clampd (k, 0.0, 5.5); }

    inline double process (double u)
    {
        const double G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const double inv = 1.0 / (1.0 + g);

        const double S1 = s[0] * inv, S2 = s[1] * inv, S3 = s[2] * inv, S4 = s[3] * inv;
        const double Ssum = G3 * S1 + G2 * S2 + G * S3 + S4;

        // Solve the non-linear zero-delay feedback loop:
        //   x  = u - K * tanh(y4 * satScale) / satScale
        //   y4 = G^4 * x + Ssum
        // satScale keeps the small signal behaviour identical (slope 1 at the
        // origin) while holding the self-oscillation amplitude to a sane level.
        double y4 = G4 * u + Ssum;
        double x  = u;
        for (int i = 0; i < 2; ++i)
        {
            x  = u - K * fastTanh (y4 * satScale) / satScale;
            y4 = G4 * x + Ssum;
        }

        // Run the four TPT one-pole stages forward with the solved input.
        double v, y;
        v = (x - s[0]) * G; y = v + s[0]; s[0] = y + v;
        v = (y - s[1]) * G; y = v + s[1]; s[1] = y + v;
        v = (y - s[2]) * G; y = v + s[2]; s[2] = y + v;
        v = (y - s[3]) * G; y = v + s[3]; s[3] = y + v;
        return y;
    }

    // Last computed 4th pole state, useful for self-oscillation tests.
    double lastOutput() const { return s[3]; }

private:
    static constexpr double satScale = 1.3;
    double sampleRate = 44100.0;
    double g = 0.0, G = 0.0, K = 0.0;
    double s[4] = { 0, 0, 0, 0 };
};

} // namespace tb303
