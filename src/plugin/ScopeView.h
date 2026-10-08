#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MotesLookAndFeel.h"

// Stereo phase scope (goniometer). x = R - L, y = L + R, so mono is a vertical line and hard-left
// the upper-left diagonal. The figure persists in an offscreen image that fades 30% per frame.
class ScopeView : public juce::Component {
public:
    explicit ScopeView(const MotesLookAndFeel&);
    // Draws one frame from the newest samples. Call once per timer tick, with n == 0 to just fade.
    void addBlock(const float* l, const float* r, int n);
    float currentGain() const { return gain; }
    void paint(juce::Graphics&) override;
    void resized() override { ensureImage(scale); }
private:
    void ensureImage(float newScale);
    const MotesLookAndFeel& lnf;
    juce::Image image;       // physical-pixel resolution, so the persistence is crisp on Retina
    float scale = 1.f;
    float gain = 1.f;        // slow-decay auto-gain follower
};
