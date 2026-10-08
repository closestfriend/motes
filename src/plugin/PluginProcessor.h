#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

extern "C" {
#include "granular_engine.h"
}

class MotesProcessor : public juce::AudioProcessor
{
public:
    MotesProcessor();
    ~MotesProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Motes"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // message thread only; false if decode fails. displayName defaults to the file name.
    bool loadWavFile(const juce::File& f, const juce::String& displayName = {});
    juce::MemoryBlock currentSampleWav;      // raw bytes for state

    // Output samples for the phase scope. The audio thread writes (drops whole blocks when full,
    // never blocks); the editor calls pullScope on the message thread. Returns frames written to
    // l/r: the newest `maxFrames` if more are queued (older ones are discarded).
    int pullScope(float* l, float* r, int maxFrames);

    // Real grains and playheads from the engine, for the waveform strip (both lock-free).
    bool popGrainEvent(GrainEvent& ev)            { return ge_pop_grain_event(engine, &ev) != 0; }
    int  getPlayheads(float* out, int max) const  { return ge_get_playheads(engine, out, max); }

    float    getOutRms() const;
    unsigned getGrainsFired() const;

    // What the editor shows about the loaded sample. Written by whoever loads a sample, read on the
    // message thread; the audio thread never touches it (so the lock below is not on the audio path).
    static constexpr int kPeakBins = 900;
    struct SampleInfo {
        juce::String name;
        double seconds = 0.0, sampleRate = 0.0;
        int channels = 0;
        std::array<float, kPeakBins> peaks {};
    };
    SampleInfo getSampleInfo() const;
    std::atomic<int> sampleVersion { 0 };     // bumped after every successful sample change

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

private:
    void publishSampleInfo(const juce::String& name, const float* interleaved, int frames, int channels, double rate);
    static constexpr int kScopeFifo = 4096;      // ~85 ms at 48 kHz
    juce::AbstractFifo scopeFifo { kScopeFifo };
    std::array<float, kScopeFifo> scopeL {}, scopeR {};
    mutable juce::CriticalSection infoLock;
    SampleInfo info;
    GranularEngine* engine = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotesProcessor)
};
