// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 artcode49-prog  <https://github.com/artcode49-prog/artcode303>
// ACID-303 is free software licensed under the GNU AGPL v3 or later; see LICENSE.

// host_test.cpp - loads the built Acid303 VST3 with JUCE's plugin host code and
// renders it, proving that the packaged plugin loads and makes sound.
//
//   tb303_hosttest <path-to-Acid303.vst3> <out.wav> [seconds]
#include <JuceHeader.h>

static void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr) return;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (stream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));
    if (writer != nullptr)
    {
        stream.release();
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 3)
    {
        std::printf ("usage: %s <plugin.vst3> <out.wav> [seconds] [bpm]\n", argv[0]);
        return 1;
    }

    const juce::File pluginFile { juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])) };
    const juce::File outFile { juce::String (argv[2]) };
    const double seconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0;
    const double bpm = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 130.0;

    const double sampleRate = 48000.0;
    const int    blockSize  = 256;

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, pluginFile.getFullPathName());

    if (types.isEmpty())
    {
        std::printf ("FAILED: no plugin found in %s\n", pluginFile.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("found: %s  (%s)\n",
                 types[0]->name.toRawUTF8(),
                 types[0]->pluginFormatName.toRawUTF8());

    juce::String error;
    auto instance = format.createInstanceFromDescription (*types[0], sampleRate, blockSize, error);

    if (instance == nullptr)
    {
        std::printf ("FAILED to instantiate: %s\n", error.toRawUTF8());
        return 1;
    }

    instance->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);

    std::printf ("plugin: %s  params: %d\n",
                 instance->getName().toRawUTF8(), instance->getParameters().size());



    // Start the host transport and switch the plugin to host sync, so the
    // internal sequencer follows the DAW clock.
    struct TestPlayHead : juce::AudioPlayHead
    {
        double bpm = 130.0;
        bool   playing = true;
        double ppq = 0.0;
        juce::int64 samples = 0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (playing);
            info.setPpqPosition (ppq);
            info.setTimeInSamples (samples);
            info.setTimeInSeconds ((double) samples / 48000.0);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    TestPlayHead playHead;
    playHead.bpm = bpm;
    instance->setPlayHead (&playHead);

    for (auto* p : instance->getParameters())
        if (p->getName (60).containsIgnoreCase ("clock"))
            p->setValueNotifyingHost (1.0f);   // "Host"

    const int totalSamples = (int) (seconds * sampleRate);
    juce::AudioBuffer<float> output (2, totalSamples);
    output.clear();

    juce::AudioBuffer<float> block (2, blockSize);
    juce::MidiBuffer midi;

    int written = 0;
    while (written < totalSamples)
    {
        const int n = juce::jmin (blockSize, totalSamples - written);
        block.setSize (2, n, false, false, true);
        block.clear();
        midi.clear();

        instance->processBlock (block, midi);

        for (int c = 0; c < 2; ++c)
            output.copyFrom (c, written, block, c, 0, n);

        written += n;
    }

    writeWav (outFile, output, sampleRate);

    const auto magnitude = output.getMagnitude (0, totalSamples);
    const auto rms = output.getRMSLevel (0, 0, totalSamples);
    std::printf ("wrote %s  peak %.3f  rms %.4f (%.1f dBFS)\n",
                 outFile.getFullPathName().toRawUTF8(), magnitude, rms,
                 juce::Decibels::gainToDecibels (rms, -100.0f));

    std::printf ("OK: the VST3 loads and renders audio\n");

    //--------------------------------------------------------------- state round trip
    {
        auto valueOf = [&] (const juce::String& name) -> float
        {
            for (auto* p : instance->getParameters())
                if (p->getName (60) == name)
                    return p->getValue();
            return -1.0f;
        };

        auto setParam = [&] (const juce::String& name, float v)
        {
            for (auto* p : instance->getParameters())
                if (p->getName (60) == name)
                    p->setValueNotifyingHost (v);
        };

        const float originalCutoff = valueOf ("Cut Off");
        const float originalReso   = valueOf ("Resonance");

        juce::MemoryBlock saved;
        instance->getStateInformation (saved);

        setParam ("Cut Off", 0.95f);
        setParam ("Resonance", 0.05f);

        const bool changed = std::abs (valueOf ("Cut Off") - originalCutoff) > 0.1f;

        instance->setStateInformation (saved.getData(), (int) saved.getSize());

        const float restoredCutoff = valueOf ("Cut Off");
        const float restoredReso   = valueOf ("Resonance");

        std::printf ("state: %d bytes, cut off %.3f, reso %.3f%s -> cut off %.3f, reso %.3f restored\n",
                     (int) saved.getSize(), originalCutoff, originalReso,
                     changed ? ", modified" : ", (modification did not stick)",
                     restoredCutoff, restoredReso);

        if (std::abs (restoredCutoff - originalCutoff) > 0.01f
            || std::abs (restoredReso - originalReso) > 0.01f)
        {
            std::printf ("FAILED: state was not restored exactly\n");
            return 1;
        }

        std::printf ("OK: state saves and restores exactly\n");
    }

    //--------------------------------------------------------------- host transport
    // The sequencer must follow the host transport in both directions.
    {
        auto renderSeconds = [&] (double secs) -> juce::AudioBuffer<float>
        {
            const int total = (int) (secs * sampleRate);
            juce::AudioBuffer<float> out (2, total);
            out.clear();

            juce::AudioBuffer<float> blk (2, blockSize);
            juce::MidiBuffer midi;

            int done = 0;
            while (done < total)
            {
                const int n = juce::jmin (blockSize, total - done);
                blk.setSize (2, n, false, false, true);
                blk.clear();
                midi.clear();
                instance->processBlock (blk, midi);
                for (int c = 0; c < 2; ++c)
                    out.copyFrom (c, done, blk, c, 0, n);
                done += n;
            }
            return out;
        };

        playHead.playing = false;                     // host transport stops
        auto stopped = renderSeconds (3.0);
        const float tail = stopped.getMagnitude (0, (int) (2.0 * sampleRate), (int) (0.5 * sampleRate));

        playHead.playing = true;                      // host transport rolls again
        auto restarted = renderSeconds (1.0);
        const float again = restarted.getMagnitude (0, 0, restarted.getNumSamples());

        std::printf ("host stop -> tail peak %.5f, host restart -> peak %.3f\n", tail, again);

        if (tail > 0.005f || again < 0.01f)
        {
            std::printf ("FAILED: host transport stop/start did not silence/resume the sequencer\n");
            return 1;
        }

        std::printf ("OK: host transport stop and restart control the sequencer\n");
    }

    instance->releaseResources();

    if (magnitude < 0.001f)
    {
        std::printf ("FAILED: the plugin produced silence\n");
        return 1;
    }

    std::printf ("ALL CHECKS PASSED\n");
    return 0;
}
