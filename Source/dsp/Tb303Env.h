// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Env.h - envelope generators used by the 303 voice.
#pragma once

#include "Tb303Common.h"

namespace tb303
{

// The 303 has a single "decay" envelope (attack is instant) that drives both
// the filter (via ENV MOD) and the VCA. Analog style exponential decay.
class DecayEnv
{
public:
    void prepare (double sr) { sampleRate = sr; setTime (0.4); reset(); }

    void setTime (double seconds)
    {
        seconds = clampd (seconds, 1.0e-4, 30.0);
        coef = std::exp (-1.0 / (seconds * sampleRate));
        // The hardware envelope is not a pure exponential - it has a slightly
        // convex shape. The extra curvature term gives a more convincing
        // "pluck" and a longer usable tail near zero.
        curveCoef = std::exp (-1.0 / (seconds * 2.2 * sampleRate));
    }

    void reset() { level = 0.0; tail = 0.0; }

    // Hard restart (a new note)
    inline void trigger (double to = 1.0)
    {
        if (level < to) level = to;
        tail = level;
    }

    inline double process()
    {
        level *= coef;
        tail  *= curveCoef;
        return 0.5 * (level + tail);
    }

    double value() const { return 0.5 * (level + tail); }
    bool   finished() const { return level < 1.0e-5 && tail < 1.0e-5; }

private:
    double sampleRate = 44100.0, level = 0.0, tail = 0.0;
    double coef = 0.999, curveCoef = 0.9995;
};

// Accent creates a fast extra "hit" on the filter, resonance and VCA.
class AccentEnv
{
public:
    void prepare (double sr) { sampleRate = sr; setTime (0.045); reset(); }
    void setTime (double seconds) { coef = std::exp (-1.0 / (clampd (seconds, 1e-4, 5.0) * sampleRate)); }
    void reset() { level = 0.0; }
    inline void trigger() { level = 1.0; }
    inline double process() { level *= coef; return level; }
    double value() const { return level; }

private:
    double sampleRate = 44100.0, level = 0.0, coef = 0.99;
};

// Simple one-pole smoother for parameter changes (removes zipper noise).
class Smoother
{
public:
    void prepare (double sr, double ms)
    {
        coef = 1.0 - std::exp (-1.0 / (std::max (ms, 0.01) * 0.001 * sr));
    }

    void reset (double v) { current = v; }

    inline double process (double target)
    {
        current += coef * (target - current);
        return current;
    }

    double value() const { return current; }

private:
    double current = 0.0, coef = 0.05;
};

} // namespace tb303
