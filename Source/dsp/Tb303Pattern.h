// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Pattern.h - 16 step pattern data, matching the hardware's memory.
#pragma once

#include "Tb303Common.h"

namespace tb303
{

// One step of the 303's internal sequencer. Note is a semitone offset inside
// one octave (0 = the pattern's root, 12 = root + octave), oct is the hardware
// OCTAVE UP / DOWN switch.
struct Step
{
    int note    = 0;    // 0..12 semitones above the pattern root
    int oct     = 0;    // -1, 0, +1  (octave down / normal / octave up)
    bool rest   = false;
    bool accent = false;
    bool slide  = false;

    int midiNote (int rootNote, int transpose) const
    {
        return rootNote + note + 12 * oct + transpose;
    }
};

enum class Direction { forward = 0, reverse, pingPong, random };

struct Pattern
{
    static constexpr int maxSteps = 16;

    Step steps[maxSteps];
    int  length    = 16;
    int  rootNote  = 36;                     // C2
    int  transpose = 0;                      // -12 .. +12
    Direction direction = Direction::forward;

    void clear()
    {
        for (auto& s : steps)
            s = Step{};
        length = 16;
    }
};

// A handful of classic acid lines shipped as factory content.
namespace factory
{
inline Pattern classicAcid()
{
    Pattern p;
    p.clear();
    p.rootNote = 36; // C2
    const int notes[16] = { 0, 0, 12, 0, 3, 0, 10, 0, 0, 0, 12, 10, 8, 0, 6, 0 };
    const bool acc[16]  = { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
    const bool sld[16]  = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0 };
    for (int i = 0; i < 16; ++i)
    {
        p.steps[i].note   = notes[i];
        p.steps[i].accent = acc[i] != 0;
        p.steps[i].slide  = sld[i] != 0;
    }
    p.steps[3].oct  = 1;
    p.steps[11].oct = -1;
    return p;
}

inline Pattern raveStab()
{
    Pattern p;
    p.clear();
    p.rootNote = 36;
    for (int i = 0; i < 16; ++i)
    {
        p.steps[i].note   = (i % 4 == 0) ? 0 : ((i % 4 == 2) ? 12 : 7);
        p.steps[i].accent = (i % 4 == 0);
        p.steps[i].slide  = (i % 4 == 3);
    }
    return p;
}

inline Pattern funkBass()
{
    Pattern p;
    p.clear();
    p.rootNote = 33; // A1
    const int notes[16] = { 0, 0, 3, 0, 5, 3, 0, 0, 0, 0, 3, 5, 7, 5, 3, 0 };
    const bool acc[16]  = { 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0 };
    const bool sld[16]  = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
    const bool rst[16]  = { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
    for (int i = 0; i < 16; ++i)
    {
        p.steps[i].note   = notes[i];
        p.steps[i].accent = acc[i] != 0;
        p.steps[i].slide  = sld[i] != 0;
        p.steps[i].rest   = rst[i] != 0;
    }
    return p;
}

inline Pattern hiEnergy()
{
    Pattern p;
    p.clear();
    p.rootNote = 36;
    for (int i = 0; i < 16; ++i)
    {
        p.steps[i].note   = (i * 5) % 13;
        p.steps[i].accent = (i % 3 == 0);
        p.steps[i].slide  = (i % 5 == 4);
        p.steps[i].oct    = (i % 8 == 7) ? 1 : 0;
        p.steps[i].rest   = (i % 7 == 6);
    }
    return p;
}
} // namespace factory

} // namespace tb303
