#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "grain_event.h"   // plain struct, safe outside extern "C"

// The 40px waveform strip: cached peak waveform, spread band, Position line.
// Dragging horizontally sets the Position parameter (x / width).
class SourceStrip : public juce::Component {
public:
    static constexpr int kBins = 900;
    explicit SourceStrip(juce::AudioProcessorValueTreeState&);
    std::function<void()> onDragStart;     // a click/drag on the strip starts here (the editor resets drift)
    void setPeaks(const std::array<float, kBins>&);
    // From the editor's timer: real grains (flash for 250 ms) and the engine's per-voice playheads
    // (-1 = inactive). With no active voice the Position line sits at the parameter value.
    void addGrainEvent(const GrainEvent&);
    void setPlayheads(const float* ph, int n);
    void paint(juce::Graphics&) override;
    void resized() override { cacheScale = 0.f; }     // force a rebuild at the new size
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void rebuildCache(float scale);
    void setFromMouse(const juce::MouseEvent&);
    void drawBar(juce::Graphics&, float x, float by, float am) const;
    struct Flash { float pos = 0.f, span = 0.f; double startMs = 0.0; };
    static constexpr int kMaxFlashes = 128, kMaxPlayheads = 16;
    std::array<Flash, kMaxFlashes> flashes {};      // fixed ring: the oldest is overwritten, nothing allocates
    int nextFlash = 0;
    std::array<float, kMaxPlayheads> playheads {};
    int numPlayheads = 0;
    std::array<float, kBins> peaks {};
    juce::Image cache;                 // waveform at physical-pixel resolution; rebuilt only on new peaks or scale change
    float cacheScale = 0.f;
    float position = 0.5f, spread = 0.05f;
    juce::RangedAudioParameter& posParam;
    juce::ParameterAttachment posAttach, spreadAttach;
};
