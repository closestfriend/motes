#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
class KineticGranularEditor : public juce::AudioProcessorEditor {
public:
    explicit KineticGranularEditor(KineticGranularProcessor& p);
    void resized() override;
private:
    KineticGranularProcessor& proc;
    juce::TextButton loadBtn { "Load WAV..." };
    juce::GenericAudioProcessorEditor generic { proc };
    std::unique_ptr<juce::FileChooser> chooser;
};
