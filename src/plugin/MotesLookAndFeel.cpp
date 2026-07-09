#include "MotesLookAndFeel.h"

MotesLookAndFeel::MotesLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffc6d4e6));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour(0xff7c8aa0));
}

void MotesLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(5.0f);
    float radius = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    float cx = b.getCentreX(), cy = b.getCentreY();
    float toAngle = startAngle + pos * (endAngle - startAngle);
    float arcR = radius - 3.0f;

    // body + outer ring
    g.setColour(juce::Colour(0xff141a24));
    g.fillEllipse(cx-radius, cy-radius, radius*2, radius*2);
    g.setColour(juce::Colour(0xff2a3444));
    g.drawEllipse(cx-radius, cy-radius, radius*2, radius*2, 1.5f);

    // faint full track
    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff222b38));
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc, cyan->violet by position
    juce::Path arc;
    arc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, toAngle, true);
    juce::Colour lo(0xff34c7de), hi(0xffa07cff);
    g.setColour(lo.interpolatedWith(hi, pos));
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer thumb
    juce::Point<float> thumb(cx + arcR * std::sin(toAngle), cy - arcR * std::cos(toAngle));
    g.setColour(juce::Colour(0xffeaf6ff));
    g.fillEllipse(thumb.x-2.0f, thumb.y-2.0f, 4.0f, 4.0f);
}
