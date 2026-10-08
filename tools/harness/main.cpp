// Dev-only headless harness (build with -DMOTES_BUILD_HARNESS=ON). Never part of the plugin.
//   motes_harness [--set id=value ...] [--snapshot out.png] [--scale 2] [--selftest]
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

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void setParam(MotesProcessor& p, const juce::String& id, float v) {
    auto* prm = p.apvts.getParameter(id);
    jassert(prm != nullptr);
    prm->setValueNotifyingHost(prm->convertTo0to1(v));
}

// Parameter plumbing: log ranges, semitone interval, host-visible value text.
static void selftestParams(MotesProcessor& p) {
    auto range = [&](const char* id) { return p.apvts.getParameter(id)->getNormalisableRange(); };
    auto* grain = p.apvts.getParameter("grain");
    CHECK(std::abs(grain->convertFrom0to1(0.5f) - 50.f) < 0.01f);               // sqrt(5*500): true log, not a power-law skew
    CHECK(std::abs(grain->convertTo0to1(50.f) - 0.5f) < 1e-4f);
    CHECK(std::abs(p.apvts.getParameter("attack")->convertFrom0to1(0.f) - 0.001f) < 1e-6f);
    CHECK(std::abs(p.apvts.getParameter("release")->convertFrom0to1(1.f) - 5.f) < 1e-4f);
    auto* semi = p.apvts.getParameter("pitchsemi");
    CHECK(semi->convertFrom0to1(0.5123f) == std::round(semi->convertFrom0to1(0.5123f)));   // whole semitones
    CHECK(semi->convertFrom0to1(0.5f) == 0.f);
    CHECK(range("density").start == 1.f && range("density").end == 100.f);
    CHECK(p.apvts.getParameter("master")->getText(p.apvts.getParameter("master")->convertTo0to1(0.8f), 0)
              == juce::String::fromUTF8("\xe2\x88\x92" "1.9 dB"));
    CHECK(p.apvts.getParameter("density")->getText(p.apvts.getParameter("density")->convertTo0to1(42.f), 0) == "42 g/s");
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::String snapshot;
    float scale = 1.f;
    bool selftest = false;
    std::vector<std::pair<juce::String, float>> sets;
    for (int i = 1; i < argc; ++i) {
        juce::String a(argv[i]);
        if (a == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (a == "--scale" && i + 1 < argc) scale = juce::String(argv[++i]).getFloatValue();
        else if (a == "--selftest") selftest = true;
        else if (a == "--set" && i + 1 < argc) {
            juce::String kv(argv[++i]);
            sets.emplace_back(kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
    }

    MotesProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    for (auto& [id, v] : sets) setParam(proc, id, v);
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
    if (selftest) {
        selftestParams(proc);
        std::printf(failures == 0 ? "selftest OK\n" : "selftest FAILED (%d)\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
