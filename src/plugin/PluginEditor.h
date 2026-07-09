#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MotesLookAndFeel.h"

class MotesEditor : public juce::AudioProcessorEditor {
public:
    explicit MotesEditor(MotesProcessor&);
    ~MotesEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void addKnob(const juce::String& paramId, const juce::String& text);
    MotesProcessor& proc;
    MotesLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;                 // in param order
    juce::TextButton loadBtn { "Load WAV..." };
    juce::Label fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> fieldArea;                            // top hero area (Task 3 fills it)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotesEditor)
};
