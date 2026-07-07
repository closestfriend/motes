#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <vector>

using APVTS = juce::AudioProcessorValueTreeState;
using PF = juce::AudioParameterFloat;
static juce::NormalisableRange<float> rng(float a,float b){ return {a,b}; }

APVTS::ParameterLayout MotesProcessor::makeLayout(){
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

MotesProcessor::MotesProcessor()
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
MotesProcessor::~MotesProcessor() { ge_destroy(engine); }

void MotesProcessor::prepareToPlay(double sampleRate, int) {
    ge_prepare(engine, sampleRate);
}

void MotesProcessor::processBlock(juce::AudioBuffer<float>& buffer,
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

bool MotesProcessor::loadWavFile(const juce::File& f){
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
    if(!rd) return false;
    juce::AudioBuffer<float> tmp((int)rd->numChannels,(int)rd->lengthInSamples);
    rd->read(&tmp,0,(int)rd->lengthInSamples,0,true,true);
    // interleave
    int ch=tmp.getNumChannels(), n=tmp.getNumSamples();
    std::vector<float> inter((size_t)n*ch);
    for(int i=0;i<n;i++) for(int c=0;c<ch;c++) inter[(size_t)i*ch+c]=tmp.getSample(c,i);
    // Decode is done above with no lock held. Only the fast buffer swap needs
    // to be serialized against processBlock: this path is also reachable from
    // the editor's "Load WAV" button on the message thread with the audio
    // callback running concurrently (unlike state-load, where the host has
    // suspended processing). Without this lock, grains already spawned in a
    // live callback could keep a raw pointer into the buffer ge_set_sample is
    // about to free -- a use-after-free on the next grain_tick.
    {
        const juce::ScopedLock sl(getCallbackLock());
        ge_set_sample(engine, inter.data(), n, ch, rd->sampleRate);
        f.loadFileAsData(currentSampleWav);   // keep raw bytes for presets
    }
    return true;
}

void MotesProcessor::getStateInformation(juce::MemoryBlock& dest){
    juce::MemoryOutputStream mos(dest,false);
    auto state=apvts.copyState();
    juce::MemoryBlock xml; { juce::MemoryOutputStream x(xml,false);
        state.createXml()->writeTo(x); }
    mos.writeInt((int)xml.getSize()); mos.write(xml.getData(),xml.getSize());
    mos.writeInt((int)currentSampleWav.getSize());
    mos.write(currentSampleWav.getData(),currentSampleWav.getSize());
}

void MotesProcessor::setStateInformation(const void* data,int size){
    juce::MemoryInputStream mis(data,(size_t)size,false);
    int xmlLen=mis.readInt();
    if(xmlLen<0 || (size_t)xmlLen>(size_t)mis.getNumBytesRemaining()) return; // corrupt preset: bail without applying partial state
    juce::MemoryBlock xml; xml.setSize((size_t)xmlLen);
    mis.read(xml.getData(),xmlLen);
    if(auto x=juce::XmlDocument::parse(xml.toString()))
        apvts.replaceState(juce::ValueTree::fromXml(*x));
    int wavLen=mis.readInt();
    if(wavLen>0){
        if((size_t)wavLen>(size_t)mis.getNumBytesRemaining()) return; // corrupt length prefix: bail
        currentSampleWav.setSize((size_t)wavLen); mis.read(currentSampleWav.getData(),wavLen);
        auto tmp=juce::File::createTempFile(".wav"); tmp.replaceWithData(currentSampleWav.getData(),currentSampleWav.getSize());
        if(!loadWavFile(tmp)) currentSampleWav.setSize(0); // decode failed: don't claim a valid sample
        tmp.deleteFile();
    }
}

juce::AudioProcessorEditor* MotesProcessor::createEditor()
{
    return new MotesEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotesProcessor();
}
