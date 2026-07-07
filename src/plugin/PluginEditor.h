#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
class MotesEditor : public juce::AudioProcessorEditor {
public:
    explicit MotesEditor(MotesProcessor& p);
    void resized() override;
private:
    MotesProcessor& proc;
    juce::TextButton loadBtn { "Load WAV..." };
    juce::GenericAudioProcessorEditor generic { proc };
    std::unique_ptr<juce::FileChooser> chooser;
};
