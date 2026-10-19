// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// tb303_render.cpp - standalone renderer / measurement harness for the
// TB-303 DSP core. No JUCE required, builds in a second:
//
//   clang++ -std=c++17 -O2 -I ../Source/dsp tb303_render.cpp -o tb303_render
//
// Modes:
//   render out.wav --pattern acid --bpm 130 --seconds 8 --cutoff 0.3 ...
//   selftest            -> numeric verification of oscillator / filter / sequencer
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <complex>

#include "Tb303Engine.h"
#include "Tb303Pattern.h"

using namespace tb303;

// ---------------------------------------------------------------- FFT (radix-2)
static void fft (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * TB303_PI / len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k)
            {
                auto u = a[i + k];
                auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// windowed = true for steady signals, false for impulse responses (an impulse
// response is far shorter than the FFT, so a Hann window would erase it).
static std::vector<double> magnitudeSpectrum (const std::vector<double>& x, int nfft, bool windowed = true)
{
    std::vector<std::complex<double>> a ((size_t) nfft);
    for (int i = 0; i < nfft; ++i)
    {
        double w = 1.0;
        if (windowed)
            w = 0.5 - 0.5 * std::cos (2.0 * TB303_PI * i / (nfft - 1));
        a[(size_t) i] = std::complex<double> (i < (int) x.size() ? x[(size_t) i] * w : 0.0, 0.0);
    }
    fft (a);
    std::vector<double> m ((size_t) (nfft / 2));
    for (int i = 0; i < nfft / 2; ++i)
        m[(size_t) i] = std::abs (a[(size_t) i]);
    return m;
}

static double binToHz (int bin, int nfft, double sr) { return bin * sr / nfft; }
static int    hzToBin (double hz, int nfft, double sr) { return (int) std::lround (hz * nfft / sr); }
static double db (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

// ---------------------------------------------------------------- WAV writer
static bool writeWav (const std::string& path, const std::vector<float>& mono, double sr, int channels = 2)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;

    const int frames = (int) mono.size();
    const int bits = 16;
    const int dataBytes = frames * channels * (bits / 8);

    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };

    std::fwrite ("RIFF", 1, 4, f);
    w32 (36 + (uint32_t) dataBytes);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f);
    w32 (16);
    w16 (1);                                    // PCM
    w16 ((uint16_t) channels);
    w32 ((uint32_t) sr);
    w32 ((uint32_t) (sr * channels * (bits / 8)));   // byte rate
    w16 ((uint16_t) (channels * (bits / 8)));        // block align
    w16 ((uint16_t) bits);
    std::fwrite ("data", 1, 4, f);
    w32 ((uint32_t) dataBytes);

    for (int i = 0; i < frames; ++i)
    {
        double v = clampd (mono[(size_t) i], -1.0, 1.0);
        const int16_t s = (int16_t) std::lround (v * 32767.0);
        for (int c = 0; c < channels; ++c)
            std::fwrite (&s, 2, 1, f);
    }
    std::fclose (f);
    return true;
}

// ---------------------------------------------------------------- helpers
struct Options
{
    std::string out = "tb303.wav";
    std::string pattern = "acid";
    double bpm = 130.0, seconds = 8.0;
    Params params;
    bool analyze = false;
};

static Pattern patternByName (const std::string& name)
{
    if (name == "rave")   return factory::raveStab();
    if (name == "funk")   return factory::funkBass();
    if (name == "hi")     return factory::hiEnergy();
    if (name == "empty")  { Pattern p; p.clear(); return p; }
    return factory::classicAcid();
}

// ---------------------------------------------------------------- self test
static int selfTest()
{
    const double sr = 48000.0;
    int failures = 0;
    auto check = [&] (bool ok, const char* what, const std::string& detail)
    {
        std::printf ("  [%s] %-42s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
        if (! ok) ++failures;
    };

    std::printf ("\n== TB-303 self test (%.0f Hz) ==\n\n", sr);

    // ---- 1. filter cut-off tracking -------------------------------------
    // A 4 pole ladder's -3 dB point sits at sqrt(2^(1/4)-1) = 0.4354 x the pole
    // frequency, which is what the knob maps to.
    const double cascade3Db = std::sqrt (std::pow (2.0, 0.25) - 1.0);
    std::printf ("-- VCF frequency response (impulse -> FFT, resonance 0) --\n");
    for (double knob : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
        const double fc = cutoffKnobToHz (knob);
        const double expect = fc * cascade3Db;
        const int nfft = 32768;
        std::vector<double> ir ((size_t) nfft, 0.0);
        DiodeLadder flt;
        flt.prepare (sr);
        flt.setCutoff (fc);
        flt.setResonance (0.0);
        for (int i = 0; i < nfft; ++i)
            ir[(size_t) i] = flt.process (i == 0 ? 1.0 : 0.0);

        auto spec = magnitudeSpectrum (ir, nfft, false);
        const double dcGain = spec[1];
        int cutBin = 0;
        for (int b = 1; b < (int) spec.size(); ++b)
            if (spec[(size_t) b] <= dcGain * 0.7071) { cutBin = b; break; }
        const double measured = binToHz (cutBin, nfft, sr);
        char buf[176];
        std::snprintf (buf, sizeof buf, "knob %.2f -> pole %7.1f Hz, -3dB %7.1f Hz (expect %7.1f), flat: %.3f",
                       knob, fc, measured, expect, dcGain);
        check (std::fabs (measured - expect) / expect < 0.18 && std::fabs (dcGain - 1.0) < 0.02,
               "cut-off tracks the knob", buf);
    }

    // ---- 2. resonance peak ----------------------------------------------
    std::printf ("\n-- VCF resonance (cut-off 1000 Hz) --\n");
    const double resKnobs[3]   = { 0.35, 0.6, 0.8 };
    const double minPeakDb[3]  = { 3.0, 10.0, 20.0 };
    for (int ri = 0; ri < 3; ++ri)
    {
        const double knob = resKnobs[ri];
        const int nfft = 32768;
        std::vector<double> ir ((size_t) nfft, 0.0);
        DiodeLadder flt;
        flt.prepare (sr);
        flt.setCutoff (1000.0);
        flt.setResonance (resonanceKnobToK (knob));
        for (int i = 0; i < nfft; ++i)
            ir[(size_t) i] = flt.process (i == 0 ? 0.001 : 0.0);
        auto spec = magnitudeSpectrum (ir, nfft, false);
        int peakBin = 1;
        for (int b = 2; b < (int) spec.size() / 8; ++b)   // 0 .. 3 kHz
            if (spec[(size_t) b] > spec[(size_t) peakBin]) peakBin = b;
        const double peakGain = spec[(size_t) peakBin] / 0.001;
        const double dcGain   = spec[1] / 0.001;
        const double peakHz   = binToHz (peakBin, nfft, sr);
        const double overDb   = db (peakGain / dcGain);
        char buf[176];
        std::snprintf (buf, sizeof buf, "res %.2f (k=%.2f) -> peak %+.1f dB at %4.0f Hz, %+.1f dB over DC (min %+.0f)",
                       knob, resonanceKnobToK (knob), db (peakGain), peakHz, overDb, minPeakDb[ri]);
        check (overDb > minPeakDb[ri] && peakHz > 650.0 && peakHz < 1400.0, "resonance peak at cut-off", buf);
    }

    // ---- 3. self oscillation -------------------------------------------
    {
        DiodeLadder flt;
        flt.prepare (sr);
        flt.setCutoff (400.0);
        flt.setResonance (resonanceKnobToK (1.0));
        double peak = 0.0, last = 0.0;
        std::vector<double> tail;
        for (int i = 0; i < (int) (sr * 1.0); ++i)
        {
            const double y = flt.process (i == 0 ? 1.0 : 0.0);
            if (i > (int) (sr * 0.5))
            {
                peak = std::max (peak, std::fabs (y));
                tail.push_back (y);
            }
            last = y;
        }
        (void) last;
        char buf[160];
        std::snprintf (buf, sizeof buf, "res 1.0 -> sustained amplitude %.4f", peak);
        const bool bounded = peak > 0.02 && peak < 4.0 && std::isfinite (peak);
        check (bounded, "self-oscillates without blowing up", buf);
    }

    // ---- 4. sequencer timing -------------------------------------------
    std::printf ("\n-- Sequencer --\n");
    {
        // exact step positions, taken straight from the engine
        const double bpm = 120.0;
        const double blockSize = 128.0;

        auto renderSteps = [blockSize] (double bpmIn, double swingIn, double seconds, std::vector<long long>& positions)
        {
            const double srIn = 48000.0;
            Engine eng;
            eng.prepare (srIn);
            Params p;
            p.tempo = bpmToTempoKnob (bpmIn);
            p.swing = (float) swingIn;
            p.decay = 0.2f;
            eng.setParams (p);
            Pattern pat = factory::funkBass();
            for (auto& s : pat.steps) s.rest = false;   // every step fires
            eng.setPattern (pat);
            eng.setPlaying (true);

            std::vector<float> b ((size_t) (srIn * seconds), 0.0f);
            long long seen = 0;
            for (size_t i = 0; i < b.size(); i += (size_t) blockSize)
            {
                const int n = (int) std::min<double> (blockSize, (double) (b.size() - i));
                float* ch[2] = { b.data() + i, b.data() + i };
                eng.processBlock (ch, 2, n);
                const long long c = eng.stepCount();
                if (c > seen) { positions.push_back (eng.lastStepSample()); seen = c; }
            }
        };

        std::vector<long long> pos;
        renderSteps (bpm, 0.0, 6.0, pos);
        const double expectedStep = 60.0 / bpm * 0.25 * sr;
        int good = 0, bad = 0;
        double worst = 0.0;
        for (size_t i = 1; i < pos.size(); ++i)
        {
            const double d = (double) (pos[i] - pos[i - 1]);
            const double err = std::fabs (d - expectedStep);
            worst = std::max (worst, err);
            if (err < 2.0) ++good; else ++bad;
        }
        char b2[220];
        std::snprintf (b2, sizeof b2, "%d steps, %d exactly on the grid, worst error %.2f samples (%.3f ms)",
                       (int) pos.size(), good, worst, worst / sr * 1000.0);
        check (pos.size() >= 8 && bad == 0, "16th notes land exactly on the tempo grid", b2);

        // swing: on-beat 4/3, off-beat 2/3 of a step at full swing
        std::vector<long long> sp;
        renderSteps (bpm, 1.0, 6.0, sp);
        double onBeat = 0, offBeat = 0;
        int nOn = 0, nOff = 0;
        for (size_t i = 1; i < sp.size() && i < 9; ++i)
        {
            const double d = (double) (sp[i] - sp[i - 1]);
            if (i % 2 == 1) { onBeat += d; ++nOn; } else { offBeat += d; ++nOff; }
        }
        onBeat /= std::max (1, nOn); offBeat /= std::max (1, nOff);
        char b3[220];
        std::snprintf (b3, sizeof b3, "full swing: on-beat %.1f, off-beat %.1f samples (expect %.1f / %.1f), avg %.1f",
                       onBeat, offBeat, expectedStep * 4.0 / 3.0, expectedStep * 2.0 / 3.0, 0.5 * (onBeat + offBeat));
        check (std::fabs (onBeat - expectedStep * 4.0 / 3.0) < 2.0
               && std::fabs (offBeat - expectedStep * 2.0 / 3.0) < 2.0
               && std::fabs (0.5 * (onBeat + offBeat) - expectedStep) < 2.0, "swing is 2:1 and stays in time", b3);
    }

    // ---- 5. oscillator aliasing -----------------------------------------
    std::printf ("\n-- VCO --\n");
    for (double morph : { 0.0, 1.0 })
    {
        const double f0 = 1046.5; // C6, high for the 303 but worst case
        const int nfft = 32768;
        Osc osc;
        osc.prepare (sr);
        osc.setFrequency (f0);
        osc.setMorph (morph);
        std::vector<double> x ((size_t) nfft, 0.0);
        for (int i = 0; i < 4096; ++i) osc.process(); // settle
        for (int i = 0; i < nfft; ++i) x[(size_t) i] = osc.process();
        auto spec = magnitudeSpectrum (x, nfft);

        // energy that is not within +/- 60 Hz of a harmonic
        double harmonicE = 0.0, otherE = 0.0;
        for (int b = 2; b < (int) spec.size(); ++b)
        {
            const double hz = binToHz (b, nfft, sr);
            const double k = std::round (hz / f0);
            const double dist = std::fabs (hz - k * f0);
            const double e = spec[(size_t) b] * spec[(size_t) b];
            if (k >= 1.0 && dist < 60.0) harmonicE += e; else otherE += e;
        }
        char b2[160];
        std::snprintf (b2, sizeof b2, "%s: non-harmonic energy %.1f dB below harmonics",
                       morph > 0.5 ? "saw" : "square", db (std::sqrt (otherE / std::max (harmonicE, 1e-12))));
        check (db (std::sqrt (otherE / std::max (harmonicE, 1e-12))) < -20.0, "band limited oscillator", b2);
    }

    // ---- 6. output sanity ------------------------------------------------
    std::printf ("\n-- Output --\n");
    {
        Engine eng;
        eng.prepare (sr);
        Params p;
        eng.setParams (p);
        eng.setPattern (patternByName ("acid"));
        eng.setPlaying (true);
        std::vector<float> buf ((size_t) (sr * 4.0));
        for (size_t i = 0; i < buf.size(); i += 128)
        {
            const int n = (int) std::min<size_t> (128, buf.size() - i);
            float* ch[2] = { buf.data() + i, buf.data() + i };
            eng.processBlock (ch, 2, n);
        }
        double peak = 0.0, sum = 0.0;
        for (auto v : buf) { peak = std::max (peak, (double) std::fabs (v)); sum += v * v; }
        const double rms = std::sqrt (sum / buf.size());
        char b2[160];
        std::snprintf (b2, sizeof b2, "peak %.3f, rms %.4f (%.1f dBFS)", peak, rms, db (rms));
        check (peak < 0.999 && rms > 0.01, "healthy output level", b2);
    }

    std::printf ("\n%s (%d failure%s)\n\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------- main
int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], "selftest") == 0)
            return selfTest();

    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--out")      o.out = next();
        else if (a == "--pattern") o.pattern = next();
        else if (a == "--bpm")     o.bpm = std::atof (next().c_str());
        else if (a == "--seconds") o.seconds = std::atof (next().c_str());
        else if (a == "--cutoff")  o.params.cutoff = (float) std::atof (next().c_str());
        else if (a == "--reso")    o.params.resonance = (float) std::atof (next().c_str());
        else if (a == "--envmod")  o.params.envMod = (float) std::atof (next().c_str());
        else if (a == "--decay")   o.params.decay = (float) std::atof (next().c_str());
        else if (a == "--accent")  o.params.accent = (float) std::atof (next().c_str());
        else if (a == "--wave")    o.params.waveform = (float) std::atof (next().c_str());
        else if (a == "--drive")   o.params.drive = (float) std::atof (next().c_str());
        else if (a == "--volume")  o.params.volume = (float) std::atof (next().c_str());
        else if (a == "--analyze") o.analyze = true;
        else if (a == "--help")
        {
            std::printf ("usage: %s selftest | render [options]\n", argv[0]);
            return 0;
        }
        else if (! a.empty() && a[0] != '-') o.out = a;
    }

    const double sr = 48000.0;
    Engine eng;
    eng.prepare (sr);

    o.params.tempo = (float) bpmToTempoKnob (o.bpm);
    eng.setParams (o.params);
    eng.setPattern (patternByName (o.pattern));
    eng.setTranspose (transposeKnobToSemis (o.params.transpose));
    eng.setPlaying (true);

    const int total = (int) (sr * o.seconds);
    std::vector<float> buf ((size_t) total, 0.0);

    // small tail so the last note is not cut off
    for (int i = 0; i < total; i += 128)
    {
        const int n = std::min (128, total - i);
        float* ch[2] = { buf.data() + i, buf.data() + i };
        eng.processBlock (ch, 2, n);
    }

    if (! writeWav (o.out, buf, sr))
    {
        std::fprintf (stderr, "could not write %s\n", o.out.c_str());
        return 1;
    }

    double peak = 0.0, sum = 0.0;
    for (auto v : buf) { peak = std::max (peak, (double) std::fabs (v)); sum += v * v; }
    std::printf ("wrote %s  (%.1fs, %.0f bpm, pattern '%s')  peak %.3f  rms %.4f\n",
                 o.out.c_str(), o.seconds, o.bpm, o.pattern.c_str(), peak, std::sqrt (sum / buf.size()));
    return 0;
}
