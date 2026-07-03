#include "PluginProcessor.h"
#include "PluginEditor.h"

KineticGranularProcessor::KineticGranularProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true)) {}
KineticGranularProcessor::~KineticGranularProcessor() {}

void KineticGranularProcessor::prepareToPlay(double, int) {}

void KineticGranularProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    buffer.clear();
}

juce::AudioProcessorEditor* KineticGranularProcessor::createEditor()
{
    return new KineticGranularEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KineticGranularProcessor();
}
