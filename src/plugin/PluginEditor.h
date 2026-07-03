#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class KineticGranularEditor : public juce::AudioProcessorEditor
{
public:
    explicit KineticGranularEditor(KineticGranularProcessor& p)
      : AudioProcessorEditor(p) { setSize(420, 260); }
    void paint(juce::Graphics& g) override
    { g.fillAll(juce::Colours::black);
      g.setColour(juce::Colours::white);
      g.drawText("Kinetic Granular", getLocalBounds(),
                 juce::Justification::centred); }
    void resized() override {}
};
