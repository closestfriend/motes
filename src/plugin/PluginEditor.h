#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MotesLookAndFeel.h"
#include "HeaderBar.h"
#include "FooterBar.h"
#include "MotesKnob.h"
#include "SourceStrip.h"

class MotesEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MotesEditor(MotesProcessor&);
    ~MotesEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void refreshSample();
    void launchLoad();
    void addKnob(const KnobSpec&, bool compact);
    MotesProcessor& proc;
    MotesLookAndFeel lnf;          // declared before the children that borrow its fonts
    HeaderBar header { lnf };
    FooterBar footer { lnf };
    SourceStrip strip { proc.apvts };
    int seenSampleVersion = -1;
    std::vector<std::unique_ptr<MotesKnob>> knobs;   // primary 6, then pan group 3, then envelope 4
    int dividerX = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotesEditor)
};
