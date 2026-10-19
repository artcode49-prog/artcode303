// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// Tb303Engine.h - the complete instrument: internal sequencer + voice.
//
// Everything above the sample rate runs at an oversampled internal rate so the
// VCF's non-linear feedback path and the VCA do not fold aliases back into the
// audible band. Only the decimated result leaves the engine.
#pragma once

#include <atomic>
#include <random>

#include "Tb303Voice.h"
#include "Tb303Pattern.h"
#include "Tb303Params.h"

namespace tb303
{

class Engine
{
public:
    static constexpr int oversample = 4;

    void prepare (double sampleRate)
    {
        outRate      = sampleRate;
        internalRate = sampleRate * oversample;

        voice.prepare (internalRate);
        decimator.setup (0.42 * outRate, internalRate);
        gainSmooth.prepare (internalRate, 15.0);
        gainSmooth.reset (volumeToGain());
        rng.seed (0x303);

        reset();
    }

    void reset()
    {
        voice.reset();
        decimator.reset();
        gainSmooth.reset (volumeToGain());
        currentStep = 0;
        stepCounter = 0;
        pingPongDir = 0;
        samplesToNextStep = 0.0;
        playing = false;
        fade = 0.0;
        stepIndexOut.store (-1);
        lastStepSamplePos.store (-1);
        stepFireCount.store (0);
        totalOutputSamples = 0;
    }

    void setParams (const Params& p)
    {
        params = p;
        voice.setParams (p);
    }

    void setPattern (const Pattern& p) { pattern = p; }
    Pattern getPattern() const { return pattern; }
    void setTranspose (int semis) { transposeSemis = semis; }

    void setPlaying (bool shouldPlay)
    {
        if (shouldPlay && ! playing)
        {
            currentStep = 0;
            stepCounter = 0;
            pingPongDir = 0;
            samplesToNextStep = 0.0;      // fire the first step immediately
        }
        playing = shouldPlay;
    }

    bool isPlaying() const { return playing; }

    void noteOn (int midiNote, bool accent) { voice.noteOn (midiNote, accent, false); }
    void noteOff() { voice.noteOff(); }
    void allNotesOff() { voice.allNotesOff(); }

    int getCurrentStep() const { return stepIndexOut.load(); }

    // Exact output-sample position of the most recent step (test/analysis hook).
    long long lastStepSample() const { return lastStepSamplePos.load(); }
    long long stepCount() const { return stepFireCount.load(); }

    void setHostTempo (double bpm) { hostBpmOverride = bpm; }
    void clearHostTempo() { hostBpmOverride = -1.0; }
    double internalSampleRate() const { return internalRate; }

    // Renders numSamples mono frames into each of the numChannels channels.
    void processBlock (float* const* out, int numChannels, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const double targetGain = volumeToGain();
            double acc = 0.0;

            for (int k = 0; k < oversample; ++k)
            {
                if (playing)
                    advanceSequencer (1.0);

                const double v = voice.process();
                const double d = decimator.process (v);

                if (k == oversample - 1)
                    acc = d;
            }

            // transport fade in / out keeps start and stop click free
            const double target = (playing || voice.envActive()) ? 1.0 : 0.0;
            fade += (target - fade) * (target > fade ? 0.0025 : 0.0008);

            const float y = (float) softLimit (acc * fade * gainSmooth.process (targetGain), 0.82);

            for (int c = 0; c < numChannels; ++c)
                out[c][i] = y;

            ++totalOutputSamples;
        }
    }

private:
    double volumeToGain() const
    {
        const double v = clampd (params.volume, 0.0, 1.0);
        return std::pow (v, 1.5) * 1.35;
    }
    void advanceSequencer (double bySamples)
    {
        samplesToNextStep -= bySamples;
        if (samplesToNextStep > 0.0)
            return;

        const int len = clampi (pattern.length, 1, Pattern::maxSteps);
        if (currentStep >= len)
            currentStep = 0;

        const Step& s = pattern.steps[currentStep];

        lastStepSamplePos.store (totalOutputSamples);
        stepFireCount.fetch_add (1);

        if (! s.rest)
        {
            const int midi  = s.midiNote (pattern.rootNote, pattern.transpose + transposeSemis);
            const bool slide = s.slide && voice.isSounding();
            voice.noteOn (midi, s.accent, slide);
        }

        stepIndexOut.store (currentStep);

        // --- schedule the next step (swing lengthens the on-beat and shortens
        //     the off-beat by the same amount, so the pair stays in time)
        const double bpm = hostBpmOverride > 0.0 ? hostBpmOverride : tempoKnobToBpm (params.tempo);
        const double stepLen = (60.0 / bpm) * internalRate * 0.25;   // 16th notes
        const double swing = (double) params.swing;
        const double swingMul = (stepCounter % 2) == 0 ? (1.0 + swing / 3.0) : (1.0 - swing / 3.0);
        samplesToNextStep += stepLen * swingMul;

        // --- advance the step pointer
        ++stepCounter;
        switch (pattern.direction)
        {
            case Direction::forward: currentStep = (currentStep + 1) % len; break;
            case Direction::reverse: currentStep = (currentStep + len - 1) % len; break;
            case Direction::pingPong:
                if (pingPongDir == 0) { if (currentStep + 1 >= len) pingPongDir = 1; else ++currentStep; }
                else                  { if (currentStep - 1 < 0)   pingPongDir = 0; else --currentStep; }
                break;
            case Direction::random:
                currentStep = (int) (rng() % (unsigned) std::max (1, len));
                break;
        }
    }

    double outRate = 44100.0, internalRate = 176400.0;
    Params params;
    Pattern pattern;
    Voice voice;
    Butterworth6 decimator;
    Smoother gainSmooth;

    bool playing = false;
    int currentStep = 0, stepCounter = 0, pingPongDir = 0;
    double samplesToNextStep = 0.0;
    double fade = 0.0;
    double hostBpmOverride = -1.0;
    int transposeSemis = 0;
    std::minstd_rand rng { 0x303 };
    std::atomic<int> stepIndexOut { -1 };
    std::atomic<long long> lastStepSamplePos { -1 };
    std::atomic<long long> stepFireCount { 0 };
    long long totalOutputSamples = 0;
};

} // namespace tb303
