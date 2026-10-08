#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamFormat.h"
#include "MotesPresets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <algorithm>
#include <vector>
extern "C" {
#include "wav.h"
}

using APVTS = juce::AudioProcessorValueTreeState;
using PF = juce::AudioParameterFloat;
static juce::NormalisableRange<float> rng(float a,float b){ return {a,b}; }

// True exponential mapping (min*(max/min)^n), not JUCE's power-law skew: matches the design
// handoff's knob feel. Host automation stores the normalised value, so this changes how an
// automation lane maps to the knob for the four parameters that use it; saved values are unchanged.
static juce::NormalisableRange<float> logRng(float a,float b){
    return { a, b,
        [](float s,float e,float n){ return s*std::pow(e/s,n); },
        [](float s,float e,float v){ return std::log(v/s)/std::log(e/s); },
        [](float s,float e,float v){ return juce::jlimit(s,e,v); } };
}

// Value text shared with the editor's knobs, so hosts also show "42 g/s", "-1.9 dB", ...
static juce::AudioParameterFloatAttributes attrs(const std::string& id){
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [id](float v,int){ return juce::String::fromUTF8(fmt::formatParam(id,v).c_str()); })
        .withValueFromStringFunction(
        [id](const juce::String& s){ return fmt::parseParam(id, s.toRawUTF8()); });
}

APVTS::ParameterLayout MotesProcessor::makeLayout(){
    APVTS::ParameterLayout l;
    l.add(std::make_unique<PF>("density","Density", rng(1,100), 30.f, attrs("density")));
    l.add(std::make_unique<PF>("grain","Grain Size (ms)", logRng(5,500), 120.f, attrs("grain")));
    l.add(std::make_unique<PF>("position","Position", rng(0,1), 0.5f, attrs("position")));
    l.add(std::make_unique<PF>("spread","Spread", rng(0,1), 0.05f, attrs("spread")));
    l.add(std::make_unique<PF>("panspread","Pan Spread", rng(0,1), 0.2f, attrs("panspread")));
    l.add(std::make_unique<PF>("pitchsemi","Pitch (semi)", juce::NormalisableRange<float>(-24,24,1.f), 0.f, attrs("pitchsemi")));
    l.add(std::make_unique<PF>("pitchcent","Pitch (cent)", rng(-100,100), 0.f, attrs("pitchcent")));
    l.add(std::make_unique<PF>("macro","Macro Speed", rng(-1,1), 0.f, attrs("macro")));
    l.add(std::make_unique<PF>("attack","Attack", logRng(0.001f,5), 0.01f, attrs("attack")));
    l.add(std::make_unique<PF>("decay","Decay", logRng(0.001f,5), 0.1f, attrs("decay")));
    l.add(std::make_unique<PF>("sustain","Sustain", rng(0,1), 0.8f, attrs("sustain")));
    l.add(std::make_unique<PF>("release","Release", logRng(0.001f,5), 0.3f, attrs("release")));
    l.add(std::make_unique<PF>("master","Master", rng(0,1), 0.8f, attrs("master")));
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
    publishSampleInfo("built-in tone", buf.data(), N, 1, 44100.0);
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
    if (driftResetRequested.exchange(false, std::memory_order_relaxed)) ge_reset_drift(engine);
    const bool hold = holdRequested.load(std::memory_order_relaxed);
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        // While HOLD is on the latch already sounds C4: ignore MIDI C4 so it can't stack voices or release the latch.
        if (hold && (m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() == kHoldNote) continue;
        if (m.isNoteOn())  ge_note_on(engine, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) ge_note_off(engine, m.getNoteNumber());
        else if (m.isAllNotesOff()) ge_all_notes_off(engine);
    }
    // Latch: (re)start C4 whenever it is not sounding. This also recovers from a sample load (which resets
    // every voice), a host all-notes-off, and voice stealing.
    if (hold) { if (!ge_voice_playing_note(engine, kHoldNote)) ge_note_on(engine, kHoldNote, 1.0f); }
    else if (holdWasOn) ge_note_off(engine, kHoldNote);
    holdWasOn = hold;
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    ge_render(engine, L, R, buffer.getNumSamples());

    // Feed the scope. Preallocated SPSC FIFO; if the GUI has stalled and the block doesn't fit we drop
    // the whole block (keeps blocks contiguous) instead of waiting.
    const int n = buffer.getNumSamples();
    if (n > 0 && n <= scopeFifo.getFreeSpace()) {
        int s1, n1, s2, n2;
        scopeFifo.prepareToWrite(n, s1, n1, s2, n2);
        std::copy(L, L + n1, scopeL.begin() + s1);
        std::copy(R, R + n1, scopeR.begin() + s1);
        if (n2 > 0) {
            std::copy(L + n1, L + n, scopeL.begin() + s2);
            std::copy(R + n1, R + n, scopeR.begin() + s2);
        }
        scopeFifo.finishedWrite(n1 + n2);
    }
}

int MotesProcessor::pullScope(float* l, float* r, int maxFrames) {
    const int ready = scopeFifo.getNumReady();
    if (ready <= 0 || maxFrames <= 0) return 0;
    const int take = juce::jmin(ready, maxFrames), skip = ready - take;
    int s1, n1, s2, n2;
    scopeFifo.prepareToRead(ready, s1, n1, s2, n2);
    for (int k = 0; k < take; ++k) {
        const int j = skip + k;
        const int idx = j < n1 ? s1 + j : s2 + (j - n1);
        l[k] = scopeL[(size_t) idx];
        r[k] = scopeR[(size_t) idx];
    }
    scopeFifo.finishedRead(n1 + n2);
    return take;
}

void MotesProcessor::publishSampleInfo(const juce::String& name, const float* d, int frames, int ch, double rate){
    SampleInfo si;
    si.name = name; si.sampleRate = rate; si.channels = ch;
    si.seconds = rate > 0.0 ? frames / rate : 0.0;
    wav_peaks(d, frames, ch, si.peaks.data(), kPeakBins);
    { const juce::ScopedLock sl(infoLock); info = std::move(si); }
    sampleVersion.fetch_add(1);
}

MotesProcessor::SampleInfo MotesProcessor::getSampleInfo() const {
    const juce::ScopedLock sl(infoLock);
    return info;
}

bool MotesProcessor::loadWavFile(const juce::File& f, const juce::String& displayName){
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
    if(!rd || rd->numChannels < 1 || rd->lengthInSamples < 1) return false;   // nothing to granulate: keep the current sample
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
    // Peaks + info are for the editor only: computed outside the callback lock.
    publishSampleInfo(displayName.isNotEmpty() ? displayName : f.getFileName(), inter.data(), n, ch, rd->sampleRate);
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
        // the temp file's name is meaningless: show the name the Load path stored in the state
        const juce::String savedName = apvts.state.getProperty("sampleName").toString();
        if(!loadWavFile(tmp, savedName.isNotEmpty() ? savedName : juce::String("restored sample"))) currentSampleWav.setSize(0); // decode failed: don't claim a valid sample
        tmp.deleteFile();
    }
}

void MotesProcessor::applyPreset(int index) {
    index = juce::jlimit(0, kNumPresets - 1, index);
    for (int k = 0; k < 13; ++k) {
        if (auto* prm = apvts.getParameter(kPresetIds[k])) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(kPresets[index].v[k]));
            prm->endChangeGesture();
        }
    }
    apvts.state.setProperty("preset", index, nullptr);
    requestDriftReset();
}

int MotesProcessor::currentPreset() const {
    return juce::jlimit(0, kNumPresets - 1, (int) apvts.state.getProperty("preset", kInitPreset));
}

juce::AudioProcessorEditor* MotesProcessor::createEditor()
{
    return new MotesEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotesProcessor();
}
