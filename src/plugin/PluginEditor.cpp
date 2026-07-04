#include "PluginEditor.h"
KineticGranularEditor::KineticGranularEditor(KineticGranularProcessor& p)
  : AudioProcessorEditor(p), proc(p) {
    addAndMakeVisible(loadBtn); addAndMakeVisible(generic);
    loadBtn.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Load WAV", juce::File(), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto f=fc.getResult(); if(f.existsAsFile()) proc.loadWavFile(f); });
    };
    setSize(440,420);
}
void KineticGranularEditor::resized(){ auto r=getLocalBounds();
    loadBtn.setBounds(r.removeFromTop(32).reduced(4)); generic.setBounds(r); }
