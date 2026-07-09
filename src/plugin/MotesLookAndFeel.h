#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class MotesLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MotesLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};
