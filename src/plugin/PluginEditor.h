#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MotesLookAndFeel.h"
#include "HeaderBar.h"
#include "FooterBar.h"
#include "MotesKnob.h"
#include "SourceStrip.h"
#include "ScopeView.h"

class MotesEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit MotesEditor(MotesProcessor&);
    ~MotesEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override { return files.size() > 0; }
    void filesDropped(const juce::StringArray& files, int x, int y) override;
private:
    void loadFile(const juce::File&);
    void stepPreset(int delta);
    void refreshPreset();
    void timerCallback() override;
    void refreshSample();
    void launchLoad();
    void addKnob(const KnobSpec&, bool compact);
    MotesProcessor& proc;
    MotesLookAndFeel lnf;          // declared before the children that borrow its fonts
    HeaderBar header { lnf };
    FooterBar footer { lnf };
    ScopeView scope { lnf };
    SourceStrip strip { proc.apvts };
    std::array<float, 1600> scopeL {}, scopeR {};   // newest-frames scratch for the scope, one tick's worth
    int seenSampleVersion = -1;
    int shownPreset = -1;
    std::vector<std::unique_ptr<MotesKnob>> knobs;   // primary 6, then pan group 3, then envelope 4
    int dividerX = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotesEditor)
};
