#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// The 40px waveform strip: cached peak waveform, spread band, Position line.
// Dragging horizontally sets the Position parameter (x / width).
class SourceStrip : public juce::Component {
public:
    static constexpr int kBins = 900;
    explicit SourceStrip(juce::AudioProcessorValueTreeState&);
    void setPeaks(const std::array<float, kBins>&);
    void paint(juce::Graphics&) override;
    void resized() override { cacheScale = 0.f; }     // force a rebuild at the new size
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void rebuildCache(float scale);
    void setFromMouse(const juce::MouseEvent&);
    void drawBar(juce::Graphics&, float x, float by, float am) const;
    std::array<float, kBins> peaks {};
    juce::Image cache;                 // waveform at physical-pixel resolution; rebuilt only on new peaks or scale change
    float cacheScale = 0.f;
    float position = 0.5f, spread = 0.05f;
    juce::RangedAudioParameter& posParam;
    juce::ParameterAttachment posAttach, spreadAttach;
};
