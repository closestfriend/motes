#include "FooterBar.h"
#include "MotesTheme.h"

static const char* kLoad = "Load\xe2\x80\xa6";   // "Load…"

// The name gets whatever is left between x=20 and "Load…" + the right-aligned meta, with 12px gaps.
juce::String FooterBar::displayedFile() const {
    auto m = lnf.mono(11.f);
    const float loadW = m.getStringWidthFloat(juce::String::fromUTF8(kLoad));
    const float maxW = (620.f - m.getStringWidthFloat(meta)) - 12.f - loadW - 12.f - 20.f;
    if (m.getStringWidthFloat(file) <= maxW) return file;
    const juce::String ell = juce::String::fromUTF8("\xe2\x80\xa6");
    juce::String s = file;
    while (s.isNotEmpty() && m.getStringWidthFloat(s + ell) > maxW) s = s.dropLastCharacters(1);
    return s + ell;
}

juce::Rectangle<float> FooterBar::loadBounds() const {
    auto m = lnf.mono(11.f);
    const float x = 20.f + m.getStringWidthFloat(displayedFile()) + 12.f;
    return { x, 0.f, m.getStringWidthFloat(juce::String::fromUTF8(kLoad)), (float) getHeight() };
}

void FooterBar::paint(juce::Graphics& g) {
    const float h = (float) getHeight();
    auto m = lnf.mono(11.f);
    g.setFont(m);

    g.setColour(theme::kInk);
    const auto shown = displayedFile();
    g.drawText(shown, juce::Rectangle<float>(20.f, 0.f, m.getStringWidthFloat(shown) + 2.f, h),
               juce::Justification::centredLeft, false);

    const auto lr = loadBounds();
    g.setColour(theme::kInk2);
    g.drawText(juce::String::fromUTF8(kLoad), lr, juce::Justification::centredLeft, false);
    const float baseline = h / 2.f - (m.getAscent() + m.getDescent()) / 2.f + m.getAscent();
    g.fillRect(lr.getX(), baseline + 3.f, lr.getWidth(), 1.f);      // underline, 3px below the baseline

    g.drawText(meta, juce::Rectangle<float>(0.f, 0.f, 620.f, h), juce::Justification::centredRight, false);
}

void FooterBar::mouseUp(const juce::MouseEvent& e) {
    if (loadBounds().contains(e.position) && onLoad) onLoad();
}
