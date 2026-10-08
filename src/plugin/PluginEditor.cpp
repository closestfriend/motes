#include "PluginEditor.h"
#include "MotesTheme.h"
#include "ParamFormat.h"
#include "MotesPresets.h"

MotesEditor::MotesEditor(MotesProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&lnf);
    addAndMakeVisible(header);
    addAndMakeVisible(footer);
    addAndMakeVisible(scope);
    addAndMakeVisible(strip);
    footer.onLoad = [this] { launchLoad(); };
    for (const auto& k : kPrimary)  addKnob(k, false);
    for (const auto& k : kPanGroup) addKnob(k, true);
    for (const auto& k : kEnvGroup) addKnob(k, true);
    setResizable(false, false);
    setSize(theme::kW, theme::kH);
    header.onPrev = [this] { stepPreset(-1); };
    header.onNext = [this] { stepPreset(+1); };
    header.onHold = [this](bool on) { proc.setHold(on); };
    header.setHold(proc.isHoldOn());                         // reopening the editor shows the true state
    refreshPreset();
    refreshSample();
    GrainEvent stale;                                        // a ring filled while no editor was open must not replay as a flash storm
    for (int i = 0; i < 1024 && proc.popGrainEvent(stale); ++i) {}
    startTimerHz(60);
}

MotesEditor::~MotesEditor() { setLookAndFeel(nullptr); }

void MotesEditor::addKnob(const KnobSpec& spec, bool compact) {
    auto* param = proc.apvts.getParameter(spec.id);
    jassert(param != nullptr);
    knobs.push_back(std::make_unique<MotesKnob>(*param, spec, compact, lnf));
    addAndMakeVisible(*knobs.back());
}

void MotesEditor::launchLoad() {
    chooser = std::make_unique<juce::FileChooser>("Load WAV", juce::File(), "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f != juce::File()) loadFile(f);              // an empty result means the dialog was cancelled
        });
}

// Shared by Load… and drag-and-drop. On success the sample version changes and the timer refreshes name,
// meta and waveform; on failure the previous sample stays and the footer says so.
void MotesEditor::loadFile(const juce::File& f) {
    if (f.existsAsFile() && proc.loadWavFile(f))
        proc.apvts.state.setProperty("sampleName", f.getFileName(), nullptr);   // so a restored session shows the name
    else
        footer.setMeta("could not decode that file");
}

void MotesEditor::filesDropped(const juce::StringArray& files, int, int y) {
    // The scope and strip bands are the drop target; the first file wins.
    if (files.size() > 0 && y >= theme::scopeY && y < theme::stripY + theme::stripH) loadFile(juce::File(files[0]));
}

void MotesEditor::stepPreset(int delta) {
    proc.applyPreset((proc.currentPreset() + delta + kNumPresets) % kNumPresets);
    refreshPreset();
}

void MotesEditor::refreshPreset() {
    shownPreset = proc.currentPreset();
    header.setPresetName(kPresets[shownPreset].name);
}

// Pull name, meta and waveform from the processor whenever the sample changed (Load, drop, or state restore).
void MotesEditor::refreshSample() {
    seenSampleVersion = proc.sampleVersion.load();
    const auto si = proc.getSampleInfo();
    strip.setPeaks(si.peaks);
    footer.setFile(si.name);
    footer.setMeta(juce::String::fromUTF8(fmt::formatSampleMeta(si.seconds, si.sampleRate, si.channels).c_str()));
}

void MotesEditor::timerCallback() {
    const int n = proc.pullScope(scopeL.data(), scopeR.data(), (int) scopeL.size());
    scope.addBlock(scopeL.data(), scopeR.data(), n);      // also with n == 0, so the figure keeps fading
    scope.repaint();

    GrainEvent ev;
    for (int budget = 256; budget > 0 && proc.popGrainEvent(ev); --budget) strip.addGrainEvent(ev);
    float ph[16];
    strip.setPlayheads(ph, proc.getPlayheads(ph, 16));
    strip.repaint();                                         // flashes animate, so repaint every tick
    if (proc.sampleVersion.load() != seenSampleVersion) refreshSample();
    if (proc.currentPreset() != shownPreset) refreshPreset();     // e.g. a state restore while the editor is open
}

void MotesEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kPaper);
    g.setColour(theme::kHairline);
    for (int y : { 44, 181, 222, 341, 457 }) g.fillRect(0, y, theme::kW, 1);
    g.fillRect(dividerX, theme::secY + 18, 1, theme::secH - 36);   // between the Pan/Drift/Fine and ADSR clusters
}

void MotesEditor::resized() {
    header.setBounds(0, theme::headerY, theme::kW, theme::headerH - 1);
    scope.setBounds(0, theme::scopeY, theme::kW, theme::scopeH - 1);
    strip.setBounds(0, theme::stripY, theme::kW, theme::stripH - 1);
    footer.setBounds(0, theme::footY + 1, theme::kW, theme::footH - 1);

    // Primary band: 6 slots of 64px, space-between inside padding 28.
    const float inner = theme::kW - 56.f;
    const float pw = (float) MotesKnob::slotWidth(false), sw = (float) MotesKnob::slotWidth(true);
    const float pgap = (inner - 6 * pw) / 5.f;
    for (int i = 0; i < 6; ++i)
        knobs[(size_t) i]->setBounds(juce::roundToInt(28.f + i * (pw + pgap)), 236, (int) pw, MotesKnob::slotHeight(false));

    // Secondary band: two flex clusters (3:4) split by a 1px divider, 20px gaps, 56px slots, space-between.
    const float flexW = inner - 40.f - 1.f;
    const float leftW = flexW * 3.f / 7.f, rightW = flexW * 4.f / 7.f;
    const float lgap = (leftW - 3 * sw) / 2.f, rgap = (rightW - 4 * sw) / 3.f;
    const float rightX = 28.f + leftW + 20.f + 1.f + 20.f;
    dividerX = juce::roundToInt(28.f + leftW + 20.f);
    for (int i = 0; i < 3; ++i)
        knobs[(size_t) (6 + i)]->setBounds(juce::roundToInt(28.f + i * (sw + lgap)), 362, (int) sw, MotesKnob::slotHeight(true));
    for (int i = 0; i < 4; ++i)
        knobs[(size_t) (9 + i)]->setBounds(juce::roundToInt(rightX + i * (sw + rgap)), 362, (int) sw, MotesKnob::slotHeight(true));
}
