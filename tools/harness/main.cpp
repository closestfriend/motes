// Dev-only headless harness (build with -DMOTES_BUILD_HARNESS=ON). Never part of the plugin.
//   motes_harness [--set id=value ...] [--load file.wav] [--snapshot out.png] [--scale 2] [--selftest]
// A PROXY for the real editor in a host: JUCE software renderer, no host, no real mouse or
// drag-and-drop. Anti-aliasing differs from CoreGraphics, so pixel diffs are indicative only.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <cmath>

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

// Writes a 16-bit mono WAV of `frames` samples produced by gen(i); frames may be 0.
static juce::File writeWav(const juce::String& name, int frames, const std::function<float(int)>& gen) {
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("motes_harness_" + name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(f), 44100.0, 1, 16, {}, 0));
    juce::AudioBuffer<float> b(1, juce::jmax(1, frames));
    for (int i = 0; i < frames; ++i) b.setSample(0, i, gen(i));
    if (frames > 0) w->writeFromAudioSampleBuffer(b, 0, frames);
    return f;
}

// Degenerate samples: must not crash, must keep the old sample when undecodable, peaks stay finite.
static void selftestSamples(MotesProcessor& p) {
    const int v0 = p.sampleVersion.load();
    CHECK(!p.loadWavFile(writeWav("empty.wav", 0, [](int) { return 0.f; })));       // 0 frames: refused, nothing changes
    CHECK(p.sampleVersion.load() == v0);
    CHECK(p.getSampleInfo().name == "built-in tone");
    CHECK(p.loadWavFile(writeWav("short.wav", 100, [](int i) { return i == 50 ? 0.9f : 0.1f; }), "short.wav"));
    auto si = p.getSampleInfo();
    CHECK(si.name == "short.wav" && si.channels == 1 && p.sampleVersion.load() == v0 + 1);
    float mx = 0; bool finite = true; int nz = 0;
    for (float v : si.peaks) { mx = std::max(mx, v); finite = finite && std::isfinite(v); nz += v > 0; }
    CHECK(finite && mx == 1.f && nz == MotesProcessor::kPeakBins);                  // 100 frames stretched over 900 bins
    CHECK(p.loadWavFile(writeWav("silent.wav", 4410, [](int) { return 0.f; }), "silent.wav"));
    for (float v : p.getSampleInfo().peaks) CHECK(v == 0.f);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::String snapshot;
    float scale = 1.f;
    bool selftest = false;
    juce::String loadPath;
    std::vector<std::pair<juce::String, float>> sets;
    for (int i = 1; i < argc; ++i) {
        juce::String a(argv[i]);
        if (a == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (a == "--scale" && i + 1 < argc) scale = juce::String(argv[++i]).getFloatValue();
        else if (a == "--selftest") selftest = true;
        else if (a == "--load" && i + 1 < argc) loadPath = argv[++i];
        else if (a == "--set" && i + 1 < argc) {
            juce::String kv(argv[++i]);
            sets.emplace_back(kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
    }

    MotesProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    for (auto& [id, v] : sets) setParam(proc, id, v);
    if (loadPath.isNotEmpty() && !proc.loadWavFile(juce::File::getCurrentWorkingDirectory().getChildFile(loadPath))) {
        std::fprintf(stderr, "could not load %s\n", loadPath.toRawUTF8());
        return 1;
    }
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
        selftestSamples(proc);
        std::printf(failures == 0 ? "selftest OK\n" : "selftest FAILED (%d)\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
