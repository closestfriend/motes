#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MotesLookAndFeel.h"
#include "MotesParams.h"

// Dial + value + label for one parameter. Bound with juce::ParameterAttachment (gestures and
// host automation are JUCE's); only the mouse math is ours: juce::Slider can't do shift-fine on a
// rotary (its shift handling is for two-value min/max sliders) nor the handoff's dx - dy drag.
class MotesKnob : public juce::Component {
public:
    MotesKnob(juce::RangedAudioParameter&, const KnobSpec&, bool compact, const MotesLookAndFeel&);
    static int slotWidth(bool compact)  { return compact ? 56 : 64; }
    static int slotHeight(bool compact) { return compact ? 82 : 96; }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool hitTest(int x, int y) override;
private:
    juce::Rectangle<float> dialBounds() const;
    void paintDial(juce::Graphics&, juce::Rectangle<float>) const;
    juce::RangedAudioParameter& param;
    KnobSpec spec;
    bool compact;
    const MotesLookAndFeel& lnf;
    float value = 0.f;                       // denormalised, kept current by the attachment
    juce::ParameterAttachment attach;
    float acc = 0.f;                         // unclamped normalised drag accumulator
    juce::Point<float> last;
};
