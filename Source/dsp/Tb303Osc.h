// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Osc.h - the TB-303 VCO: a single saw/square oscillator.
#pragma once

#include "Tb303Common.h"

namespace tb303
{

// The hardware has a waveform switch (saw or square). We expose a continuous
// morph between the two, which reproduces both switch positions exactly and
// gives some extra mileage in between.
class Osc
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        phase = 0.0;
        // avoid a perfectly periodic restart (subtle analogue-ish variation)
        lastDcIn = lastDcOut = 0.0;
    }

    void setFrequency (double hz) { freq = clampd (hz, 0.1, sampleRate * 0.45); }
    void setMorph (double m)      { morph = clampd (m, 0.0, 1.0); }

    inline double process()
    {
        const double dt = freq / sampleRate;
        const double t  = phase;

        // Sawtooth (band limited square-free ramp)
        double saw = 2.0 * t - 1.0 - polyBlep (t, dt);

        // Square, 50% duty, band limited. Polarity matches the rising sawtooth
        // so that blending the two adds up instead of cancelling.
        double sq = (t < 0.5 ? -1.0 : 1.0);
        sq -= polyBlep (t, dt);
        double t2 = t + 0.5;
        if (t2 >= 1.0) t2 -= 1.0;
        sq += polyBlep (t2, dt);

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;

        const double out = sq + morph * (saw - sq);

        // remove the small DC step the BLEP residual can leave behind
        const double dc = out - lastDcIn + 0.9995 * lastDcOut;
        lastDcIn  = out;
        lastDcOut = dc;
        return dc;
    }

private:
    double sampleRate = 44100.0, phase = 0.0, freq = 100.0, morph = 1.0;
    double lastDcIn = 0.0, lastDcOut = 0.0;
};

} // namespace tb303
