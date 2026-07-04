#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>

using APVTS = juce::AudioProcessorValueTreeState;
using PF = juce::AudioParameterFloat;
static juce::NormalisableRange<float> rng(float a,float b){ return {a,b}; }

APVTS::ParameterLayout KineticGranularProcessor::makeLayout(){
    APVTS::ParameterLayout l;
    l.add(std::make_unique<PF>("density","Density", rng(1,100), 30.f));
    l.add(std::make_unique<PF>("grain","Grain Size (ms)", rng(5,500), 120.f));
    l.add(std::make_unique<PF>("position","Position", rng(0,1), 0.5f));
    l.add(std::make_unique<PF>("spread","Spread", rng(0,1), 0.05f));
    l.add(std::make_unique<PF>("panspread","Pan Spread", rng(0,1), 0.2f));
    l.add(std::make_unique<PF>("pitchsemi","Pitch (semi)", rng(-24,24), 0.f));
    l.add(std::make_unique<PF>("pitchcent","Pitch (cent)", rng(-100,100), 0.f));
    l.add(std::make_unique<PF>("macro","Macro Speed", rng(-1,1), 0.f));
    l.add(std::make_unique<PF>("attack","Attack", rng(0.001f,5), 0.01f));
    l.add(std::make_unique<PF>("decay","Decay", rng(0.001f,5), 0.1f));
    l.add(std::make_unique<PF>("sustain","Sustain", rng(0,1), 0.8f));
    l.add(std::make_unique<PF>("release","Release", rng(0.001f,5), 0.3f));
    l.add(std::make_unique<PF>("master","Master", rng(0,1), 0.8f));
    return l;
}

KineticGranularProcessor::KineticGranularProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true))
  , apvts(*this, nullptr, "PARAMS", makeLayout())
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
    auto g=[&](const char* id){ return apvts.getRawParameterValue(id)->load(); };
    ge_set_param(engine,P_DENSITY,g("density"));
    ge_set_param(engine,P_GRAIN_MS,g("grain"));
    ge_set_param(engine,P_POSITION,g("position"));
    ge_set_param(engine,P_SPREAD,g("spread"));
    ge_set_param(engine,P_PAN_SPREAD,g("panspread"));
    ge_set_param(engine,P_PITCH_SEMI,g("pitchsemi"));
    ge_set_param(engine,P_PITCH_CENT,g("pitchcent"));
    ge_set_param(engine,P_MACRO_SPEED,g("macro"));
    ge_set_param(engine,P_ATTACK,g("attack"));
    ge_set_param(engine,P_DECAY,g("decay"));
    ge_set_param(engine,P_SUSTAIN,g("sustain"));
    ge_set_param(engine,P_RELEASE,g("release"));
    ge_set_param(engine,P_MASTER,g("master"));

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
    return new juce::GenericAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KineticGranularProcessor();
}
