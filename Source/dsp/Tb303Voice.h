// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Voice.h - one monophonic 303 voice: VCO -> VCF -> VCA with slide.
#pragma once

#include "Tb303Osc.h"
#include "Tb303Filter.h"
#include "Tb303Env.h"
#include "Tb303Params.h"

namespace tb303
{

class Voice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        osc.prepare (sr);
        filter.prepare (sr);
        vcfEnv.prepare (sr);
        ampEnv.prepare (sr);
        accentEnv.prepare (sr);
        cutoffSmooth.prepare (sr, 3.0);
        resSmooth.prepare (sr, 3.0);
        glideCoef = 1.0 - std::exp (-1.0 / (0.075 * sr)); // ~75 ms slide time
        releaseCoef = std::exp (-1.0 / (0.004 * sr));     // 4 ms MIDI note-off release
        reset();
    }

    void reset()
    {
        osc.reset();
        filter.reset();
        vcfEnv.reset();
        ampEnv.reset();
        accentEnv.reset();
        freq = targetFreq = 110.0;
        gate = false;
        sounding = false;
        cutoffSmooth.reset (cutoffHzFromParams());
        resSmooth.reset (resFromParams());
    }

    void setParams (const Params& p)
    {
        params = p;
        // The single 303 decay envelope drives both the VCF (via ENV MOD) and
        // the VCA. Without this the DECAY knob would have no effect and every
        // note would ring for the hard-coded default time.
        const double decaySec = decayKnobToSeconds (p.decay);
        vcfEnv.setTime (decaySec);
        ampEnv.setTime (decaySec);
    }

    // note: MIDI note number. accent: accent flag. slide: glide into the note
    // without re-triggering the envelope (hardware SLIDE behaviour).
    void noteOn (int midiNote, bool accent, bool slide)
    {
        targetFreq = noteToHz (midiNote);

        if (! slide || ! sounding)
        {
            freq = targetFreq;
            vcfEnv.trigger();
            ampEnv.trigger();
        }

        if (accent)
            accentEnv.trigger();

        gate = true;
        sounding = true;
    }

    void noteOff()
    {
        gate = false;
        // The sequencer never releases (decay does the job), but MIDI notes do.
        releaseRequested = true;
    }

    void allNotesOff()
    {
        ampEnv.reset();
        vcfEnv.reset();
        accentEnv.reset();
        gate = false;
        sounding = false;
        releaseRequested = false;
    }

    bool isSounding() const { return sounding; }
    bool envActive() const { return ! ampEnv.finished(); }

    inline double process()
    {
        if (! sounding && ampEnv.finished())
            return 0.0;

        // ---- glide
        freq += (targetFreq - freq) * glideCoef;
        osc.setFrequency (freq);
        osc.setMorph (params.waveform);

        // ---- envelopes
        const double ampEnvV = ampEnv.process();
        const double vcfEnvV = vcfEnv.process();
        const double accEnvV = accentEnv.process();

        // ---- cut-off = base + env-mod (+ accent sweep)
        double cutHz = cutoffHzFromParams();

        const double envDepth = 6.0 * (double) params.envMod + 4.0 * (double) params.accent * accEnvV * 0.75;
        cutHz *= std::pow (2.0, envDepth * vcfEnvV);
        cutHz = clampd (cutHz, 8.0, 0.45 * sampleRate);
        filter.setCutoff (cutoffSmooth.process (cutHz));

        // ---- resonance = base + accent boost
        double k = resFromParams();
        k += 0.7 * (double) params.accent * accEnvV;
        filter.setResonance (resSmooth.process (clampd (k, 0.0, 5.5)));

        // ---- VCO into the filter, with the hardware's hot input level
        const double oscOut = osc.process() * 0.75;   // square and saw arrive at a similar level
        double in = 1.15 * oscOut;
        const double drive = 0.9 + 2.4 * (double) params.drive;
        in = fastTanh (in * drive);

        double y = filter.process (in);

        // ---- VCA (saturating, like the 303's transistor VCA)
        double amp = ampEnvV;
        if (releaseRequested)
            amp *= releaseGain;
        amp *= 1.0 + 0.35 * (double) params.accent * accEnvV;   // accent hits harder
        amp *= 1.0 + 0.15 * (double) params.resonance;          // and resonance eats level

        // VCA colour, then the instrument's output stage: transparent at normal
        // playing levels, smoothly limiting a screaming self-oscillating filter.
        y = fastTanh (y) * 0.95;
        y *= amp;
        y = softLimit (y, 0.80);

        if (releaseRequested)
        {
            releaseGain *= releaseCoef;
            if (releaseGain < 1.0e-4)
            {
                allNotesOff();
                releaseGain = 1.0;
            }
        }

        if (ampEnv.finished() && ! gate)
            sounding = false;

        return y;
    }

private:
    double noteToHz (int midiNote) const
    {
        const double cents = tuningKnobToCents (params.tuning);
        return 440.0 * std::pow (2.0, (midiNote - 69 + cents / 100.0) / 12.0);
    }

    double cutoffHzFromParams() const { return cutoffKnobToHz (params.cutoff); }
    double resFromParams()      const { return resonanceKnobToK (params.resonance); }

    double sampleRate = 44100.0;
    Params params;

    Osc osc;
    DiodeLadder filter;
    DecayEnv vcfEnv, ampEnv;
    AccentEnv accentEnv;
    Smoother cutoffSmooth, resSmooth;

    double freq = 110.0, targetFreq = 110.0, glideCoef = 0.01;
    bool gate = false, sounding = false, releaseRequested = false;
    double releaseGain = 1.0;
    double releaseCoef = 0.9995; // set in prepare
};

} // namespace tb303
