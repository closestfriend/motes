// Dev-only headless harness (build with -DMOTES_BUILD_HARNESS=ON). Never part of the plugin.
//   motes_harness --snapshot out.png [--scale 2]
// A PROXY for the real editor in a host: JUCE software renderer, no host, no real mouse or
// drag-and-drop. Anti-aliasing differs from CoreGraphics, so pixel diffs are indicative only.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

static bool writePng(const juce::Image& img, const juce::File& f) {
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream out(f);
    if (!out.openedOk()) return false;
    return juce::PNGImageFormat().writeImageToStream(img, out);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::String snapshot;
    float scale = 1.f;
    for (int i = 1; i < argc; ++i) {
        juce::String a(argv[i]);
        if (a == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (a == "--scale" && i + 1 < argc) scale = juce::String(argv[++i]).getFloatValue();
    }

    MotesProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setBounds(0, 0, 640, 496);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(300);

    if (snapshot.isNotEmpty()) {
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, scale);
        if (!writePng(img, juce::File::getCurrentWorkingDirectory().getChildFile(snapshot))) {
            std::fprintf(stderr, "could not write %s\n", snapshot.toRawUTF8());
            return 1;
        }
        std::printf("wrote %s (%dx%d)\n", snapshot.toRawUTF8(), img.getWidth(), img.getHeight());
    }
    ed.reset();
    return 0;
}
