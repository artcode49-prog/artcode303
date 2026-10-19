// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Params.h - parameter block shared by the plugin and the DSP core.
#pragma once

#include "Tb303Common.h"

namespace tb303
{

// All values are normalised 0..1 unless noted, mirroring the DAW automation
// parameters. Mapping to physical units happens inside the DSP.
struct Params
{
    // --- voice
    float waveform   = 0.55f;   // 0 = square, 1 = sawtooth
    float tuning     = 0.5f;    // +/- 100 cents
    float cutoff     = 0.30f;   // VCF cut-off frequency
    float resonance  = 0.45f;   // VCF resonance
    float envMod     = 0.60f;   // envelope -> cut-off depth
    float decay      = 0.32f;   // envelope decay time
    float accent     = 0.70f;   // accent intensity
    float drive      = 0.20f;   // extra VCF input overdrive (bonus)
    float volume     = 0.80f;   // output level

    // --- sequencer
    float tempo      = 0.5f;    // mapped to BPM below
    float swing      = 0.0f;    // 0 = straight, 1 = heavy shuffle
    float transpose  = 0.5f;    // -12..+12 semitones (0.5 = 0)

    // --- live overrides (not automatable in the DAW sense)
    bool  hostSync   = false;
};

inline double tempoKnobToBpm (double knob)
{
    return 40.0 * std::pow (300.0 / 40.0, clampd (knob, 0.0, 1.0));
}

inline double bpmToTempoKnob (double bpm)
{
    return std::log (clampd (bpm, 40.0, 300.0) / 40.0) / std::log (300.0 / 40.0);
}

inline int transposeKnobToSemis (double knob)
{
    return clampi ((int) std::lround ((clampd (knob, 0.0, 1.0) - 0.5) * 24.0), -12, 12);
}

} // namespace tb303
