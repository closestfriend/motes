#include "FooterBar.h"
#include "MotesTheme.h"

static const char* kLoad = "Load\xe2\x80\xa6";   // "Load…"

juce::Rectangle<float> FooterBar::loadRect() const {
    auto m = lnf.mono(11.f);
    const float x = 20.f + m.getStringWidthFloat(file) + 12.f;
    return { x, 0.f, m.getStringWidthFloat(juce::String::fromUTF8(kLoad)), (float) getHeight() };
}

void FooterBar::paint(juce::Graphics& g) {
    const float h = (float) getHeight();
    auto m = lnf.mono(11.f);
    g.setFont(m);

    g.setColour(theme::kInk);
    g.drawText(file, juce::Rectangle<float>(20.f, 0.f, m.getStringWidthFloat(file) + 2.f, h),
               juce::Justification::centredLeft, false);

    const auto lr = loadRect();
    g.setColour(theme::kInk2);
    g.drawText(juce::String::fromUTF8(kLoad), lr, juce::Justification::centredLeft, false);
    const float baseline = h / 2.f - (m.getAscent() + m.getDescent()) / 2.f + m.getAscent();
    g.fillRect(lr.getX(), baseline + 3.f, lr.getWidth(), 1.f);      // underline, 3px below the baseline

    g.drawText(meta, juce::Rectangle<float>(0.f, 0.f, 620.f, h), juce::Justification::centredRight, false);
}

void FooterBar::mouseUp(const juce::MouseEvent& e) {
    if (loadRect().contains(e.position) && onLoad) onLoad();
}
