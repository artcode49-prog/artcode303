// screenshot.cpp - instantiates the plugin, opens its editor and saves a PNG,
// so the front panel can be checked without a DAW.
#include <JuceHeader.h>
#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out (argc > 1 ? juce::File (juce::String (argv[1]))
                                   : juce::File::getCurrentWorkingDirectory().getChildFile ("panel.png"));
    const int w = argc > 2 ? juce::String (argv[2]).getIntValue() : 900;
    const int h = argc > 3 ? juce::String (argv[3]).getIntValue() : 560;

    Tb303AudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        if (editor == nullptr)
        {
            std::printf ("no editor\n");
            return 1;
        }

        editor->setSize (w, h);

        // let the panel lay itself out and draw once
        editor->resized();

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false);
        juce::PNGImageFormat png;
        out.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
            png.writeImageToStream (image, *stream);

        std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    }

    processor.releaseResources();
    return 0;
}
