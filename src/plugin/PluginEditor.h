#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MotesLookAndFeel.h"
#include "HeaderBar.h"
#include "FooterBar.h"

class MotesEditor : public juce::AudioProcessorEditor {
public:
    explicit MotesEditor(MotesProcessor&);
    ~MotesEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void launchLoad();
    MotesProcessor& proc;
    MotesLookAndFeel lnf;          // declared before the children that borrow its fonts
    HeaderBar header { lnf };
    FooterBar footer { lnf };
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotesEditor)
};
