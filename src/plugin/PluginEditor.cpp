#include "PluginEditor.h"
#include "MotesTheme.h"

MotesEditor::MotesEditor(MotesProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&lnf);
    addAndMakeVisible(header);
    addAndMakeVisible(footer);
    footer.onLoad = [this] { launchLoad(); };
    for (const auto& k : kPrimary)  addKnob(k, false);
    for (const auto& k : kPanGroup) addKnob(k, true);
    for (const auto& k : kEnvGroup) addKnob(k, true);
    setResizable(false, false);
    setSize(theme::kW, theme::kH);
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
            if (f.existsAsFile() && proc.loadWavFile(f)) footer.setFile(f.getFileName());
        });
}

void MotesEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kPaper);
    g.setColour(theme::kHairline);
    for (int y : { 44, 181, 222, 341, 457 }) g.fillRect(0, y, theme::kW, 1);
    g.fillRect(dividerX, theme::secY + 18, 1, theme::secH - 36);   // between the Pan/Drift/Fine and ADSR clusters
}

void MotesEditor::resized() {
    header.setBounds(0, theme::headerY, theme::kW, theme::headerH - 1);
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
