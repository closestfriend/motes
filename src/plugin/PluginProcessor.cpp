#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>

KineticGranularProcessor::KineticGranularProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true))
{
    engine = ge_create(44100.0, 16);
    // seed a 1s 220Hz sine so the plugin makes sound before a sample is loaded
    const int N = 44100; std::vector<float> buf((size_t)N);
    for (int i=0;i<N;i++) buf[(size_t)i] = 0.5f*std::sin(2.0*M_PI*220.0*i/44100.0);
    ge_set_sample(engine, buf.data(), N, 1, 44100.0);
}
KineticGranularProcessor::~KineticGranularProcessor() { ge_destroy(engine); }

void KineticGranularProcessor::prepareToPlay(double sampleRate, int) {
    ge_prepare(engine, sampleRate);
}

void KineticGranularProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    buffer.clear();
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isNoteOn())  ge_note_on(engine, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) ge_note_off(engine, m.getNoteNumber());
        else if (m.isAllNotesOff()) ge_all_notes_off(engine);
    }
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    ge_render(engine, L, R, buffer.getNumSamples());
}

juce::AudioProcessorEditor* KineticGranularProcessor::createEditor()
{
    return new KineticGranularEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KineticGranularProcessor();
}
