// Dev-only headless harness (build with -DMOTES_BUILD_HARNESS=ON). Never part of the plugin.
//   motes_harness [--set id=value ...] [--load file.wav] [--preset N] [--hold] [--play-ms N] [--snapshot out.png] [--scale 2] [--selftest]
// A PROXY for the real editor in a host: JUCE software renderer, no host, no real mouse or
// drag-and-drop. Anti-aliasing differs from CoreGraphics, so pixel diffs are indicative only.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <cmath>
#include "ScopeView.h"
#include "MotesPresets.h"

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

// Output FIFO behaviour at the audio/GUI seam.
static void feedBlocks(MotesProcessor& p, int blocks, bool noteOn) {
    juce::AudioBuffer<float> buf(2, 512);
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        if (noteOn && b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        p.processBlock(buf, midi);
    }
}

static void selftestScope(MotesProcessor& p) {
    CHECK(p.loadWavFile(writeWav("tone.wav", 44100, [](int i) { return 0.5f * std::sin(2.f * 3.14159265f * 220.f * (float) i / 44100.f); }), "tone.wav"));
    std::vector<float> l(4096), r(4096);
    p.pullScope(l.data(), r.data(), 4096);                                  // drain whatever the editor session left
    feedBlocks(p, 4, true);                                                 // 2048 frames queued
    int n = p.pullScope(l.data(), r.data(), 1600);
    CHECK(n == 1600);                                                       // newest 1600 of 2048
    float peak = 0; for (int i = 0; i < n; ++i) peak = std::max(peak, std::abs(l[(size_t) i]) + std::abs(r[(size_t) i]));
    CHECK(peak > 0.f);                                                      // real output, not zeros
    CHECK(p.pullScope(l.data(), r.data(), 1600) == 0);                      // the older 448 were discarded, not replayed

    feedBlocks(p, 200, false);                                              // GUI stalled: FIFO full, extra blocks dropped, never blocks
    n = p.pullScope(l.data(), r.data(), 1600);
    CHECK(n == 1600);
    CHECK(p.pullScope(l.data(), r.data(), 1600) == 0);
    feedBlocks(p, 1, false);                                                // recovers cleanly after the drop
    CHECK(p.pullScope(l.data(), r.data(), 1600) == 512);

    // Real grain events and playheads reach the processor API the editor polls.
    GrainEvent ge;
    while (p.popGrainEvent(ge)) {}                                           // drain
    p.setCurrentProgram(0);
    feedBlocks(p, 40, false);                                               // the held note from above is still sounding
    int events = 0; bool sane = true;
    while (p.popGrainEvent(ge)) { ++events; sane = sane && ge.pos >= 0.f && ge.pos <= 1.f && ge.span > 0.f && ge.span <= 1.f; }
    CHECK(events > 0 && sane);
    float ph[16];
    CHECK(p.getPlayheads(ph, 16) == 16);
    int live = 0; for (float v : ph) if (v >= 0.f) ++live;
    CHECK(live >= 1);

    // Non-finite samples must not poison the auto-gain or the path.
    MotesLookAndFeel lnf;
    ScopeView sv(lnf);
    sv.setBounds(0, 0, 640, 136);
    std::vector<float> bad(512, 0.5f), good(512, 0.25f);
    bad[3] = NAN; bad[9] = INFINITY;
    sv.addBlock(bad.data(), good.data(), 512);
    CHECK(std::isfinite(sv.currentGain()) && sv.currentGain() >= .3f);
    sv.addBlock(good.data(), good.data(), 512);
    CHECK(std::isfinite(sv.currentGain()));
    auto snap = sv.createComponentSnapshot(sv.getLocalBounds());
    CHECK(snap.isValid());

    // A mono signal (L == R) must draw a vertical line through the centre: x = R - L = 0.
    ScopeView mono(lnf);
    mono.setBounds(0, 0, 640, 136);
    std::vector<float> sine(512);
    for (int i = 0; i < 512; ++i) sine[(size_t) i] = 0.5f * std::sin(2.f * 3.14159265f * (float) i / 128.f);
    mono.addBlock(sine.data(), sine.data(), 512);
    auto ms = mono.createComponentSnapshot(mono.getLocalBounds());
    int dark = 0;
    for (int y = 20; y < 116; ++y) if (ms.getPixelAt(320, y).getBrightness() < 0.6f) ++dark;   // guides are lighter than this
    CHECK(dark > 20);
}

static void blocks(MotesProcessor& p, int n, const juce::MidiBuffer& first = {}) {
    juce::AudioBuffer<float> buf(2, 512);
    for (int b = 0; b < n; ++b) { juce::MidiBuffer empty; juce::MidiBuffer m = b == 0 ? first : empty; p.processBlock(buf, m); }
}

// HOLD latch: it must survive everything that kills or steals voices, and never stack duplicates.
static void selftestHold(MotesProcessor& p) {
    juce::MidiBuffer allOff;  allOff.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    p.setHold(false);
    blocks(p, 120, allOff);
    CHECK(p.getActiveVoiceCount() == 0);

    p.setHold(true);
    blocks(p, 1);
    CHECK(p.getActiveVoiceCount() == 1 && p.isHoldVoiceSounding());

    juce::MidiBuffer c4;                                                    // a MIDI C4 while HOLD is on is ignored
    c4.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    c4.addEvent(juce::MidiMessage::noteOff(1, 60), 10);
    blocks(p, 1, c4);
    CHECK(p.getActiveVoiceCount() == 1 && p.isHoldVoiceSounding());

    blocks(p, 1, allOff);                                                   // host all-notes-off: re-latched at once
    CHECK(p.isHoldVoiceSounding() && p.getActiveVoiceCount() <= 2);
    blocks(p, 120);
    CHECK(p.getActiveVoiceCount() == 1);                                    // the old voice finished releasing: no stacking

    CHECK(p.loadWavFile(writeWav("hold.wav", 22050, [](int i) { return 0.3f * std::sin(0.05f * (float) i); }), "hold.wav"));
    blocks(p, 1);                                                           // sample swap resets every voice: latch comes back
    CHECK(p.isHoldVoiceSounding() && p.getActiveVoiceCount() == 1);

    juce::MidiBuffer many;                                                  // 16 more notes steal the oldest (the latch)
    for (int n = 61; n <= 76; ++n) many.addEvent(juce::MidiMessage::noteOn(1, n, 1.0f), n - 61);
    blocks(p, 1, many);
    blocks(p, 1);
    CHECK(p.isHoldVoiceSounding() && p.getActiveVoiceCount() == 16);

    p.setHold(false);
    blocks(p, 1);
    CHECK(!p.isHoldVoiceSounding());
    blocks(p, 1, allOff);
    blocks(p, 120);
    CHECK(p.getActiveVoiceCount() == 0);
}

// Factory presets set the 13 parameters (not the sample); the state round-trips name, preset and sample but not HOLD.
static void selftestPresetsAndState(MotesProcessor& p) {
    auto val = [&](const char* id) { return p.apvts.getRawParameterValue(id)->load(); };
    CHECK(p.currentPreset() == 3);                                          // starts on "Init", whose values are the parameter defaults
    p.applyPreset(1);                                                       // Glass Rain
    CHECK(p.currentPreset() == 1);
    CHECK(std::abs(val("density") - 88.f) < .01f && std::abs(val("grain") - 24.f) < .05f);
    CHECK(val("pitchsemi") == 12.f && std::abs(val("pitchcent") + 7.f) < .01f && std::abs(val("macro") + .3f) < .001f);
    CHECK(std::abs(val("attack") - .005f) < 1e-4f && std::abs(val("release") - .6f) < .002f && std::abs(val("master") - .7f) < .001f);
    p.applyPreset(0);                                                       // Isolation Drift
    CHECK(std::abs(val("release") - 1.8f) < .005f && std::abs(val("position") - .38f) < .001f);
    CHECK(p.getSampleInfo().name == "hold.wav");                            // presets never touch the sample

    p.setHold(true);
    p.apvts.state.setProperty("sampleName", "hold.wav", nullptr);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    MotesProcessor q;
    const int v0 = q.sampleVersion.load();
    q.setStateInformation(state.getData(), (int) state.getSize());
    CHECK(q.getSampleInfo().name == "hold.wav");                            // not the temp file's name
    CHECK(q.sampleVersion.load() > v0);                                     // so an open editor refreshes
    float mx = 0; for (float v : q.getSampleInfo().peaks) mx = std::max(mx, v);
    CHECK(mx == 1.f);
    CHECK(q.currentPreset() == 0);
    CHECK(std::abs(q.apvts.getRawParameterValue("release")->load() - 1.8f) < .005f);
    CHECK(!q.isHoldOn());                                                   // a project must not start droning on load
    p.setHold(false);
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::String snapshot;
    float scale = 1.f;
    bool selftest = false;
    int playMs = 0, preset = -1;
    bool hold = false;
    juce::String loadPath;
    std::vector<std::pair<juce::String, float>> sets;
    for (int i = 1; i < argc; ++i) {
        juce::String a(argv[i]);
        if (a == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (a == "--scale" && i + 1 < argc) scale = juce::String(argv[++i]).getFloatValue();
        else if (a == "--selftest") selftest = true;
        else if (a == "--hold") hold = true;
        else if (a == "--preset" && i + 1 < argc) preset = juce::String(argv[++i]).getIntValue();
        else if (a == "--play-ms" && i + 1 < argc) playMs = juce::String(argv[++i]).getIntValue();   // hold C4 and run audio in step with the editor timer
        else if (a == "--load" && i + 1 < argc) loadPath = argv[++i];
        else if (a == "--set" && i + 1 < argc) {
            juce::String kv(argv[++i]);
            sets.emplace_back(kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getFloatValue());
        }
    }

    MotesProcessor proc;
    proc.prepareToPlay(48000.0, 512);
    if (preset >= 0) proc.applyPreset(preset);
    for (auto& [id, v] : sets) setParam(proc, id, v);
    proc.setHold(hold);
    if (loadPath.isNotEmpty() && !proc.loadWavFile(juce::File::getCurrentWorkingDirectory().getChildFile(loadPath))) {
        std::fprintf(stderr, "could not load %s\n", loadPath.toRawUTF8());
        return 1;
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setBounds(0, 0, 640, 496);
    if (playMs > 0) {
        for (int i = 0; i < playMs / 16; ++i) {       // 2 blocks (~21 ms of audio) per ~16 ms editor tick
            feedBlocks(proc, 2, i == 0);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(16);
        }
    } else {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(300);   // (not after --play-ms: the scope would fade out)
    }

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
        selftestScope(proc);
        selftestHold(proc);
        selftestPresetsAndState(proc);
        std::printf(failures == 0 ? "selftest OK\n" : "selftest FAILED (%d)\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
