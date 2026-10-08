#include "PluginEditor.h"
#include "MotesTheme.h"

MotesEditor::MotesEditor(MotesProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&lnf);
    addAndMakeVisible(header);
    addAndMakeVisible(footer);
    footer.onLoad = [this] { launchLoad(); };
    setResizable(false, false);
    setSize(theme::kW, theme::kH);
}

MotesEditor::~MotesEditor() { setLookAndFeel(nullptr); }

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
}

void MotesEditor::resized() {
    header.setBounds(0, theme::headerY, theme::kW, theme::headerH - 1);
    footer.setBounds(0, theme::footY + 1, theme::kW, theme::footH - 1);
}
