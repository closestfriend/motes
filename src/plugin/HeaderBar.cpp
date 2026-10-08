#include "HeaderBar.h"
#include "MotesTheme.h"

static juce::Font holdFont(const MotesLookAndFeel& l) {
    auto f = l.barlow(11.f, false);
    f.setExtraKerningFactor(0.14f);
    return f;
}

HeaderBar::Geo HeaderBar::geo() const {
    const float h = (float) getHeight(), right = 620.f;
    auto m = lnf.mono(11.f);
    const float arrowW = m.getStringWidthFloat(juce::String::fromUTF8("\xe2\x80\xb9")) + 8.f;   // glyph + 4px hit padding each side
    const float nameW = m.getStringWidthFloat(preset);
    const float pillW = holdFont(lnf).getStringWidthFloat("HOLD") + 22.f;                         // padding 10 + border 1, each side
    Geo g;
    g.pill = { right - pillW, (h - 23.f) / 2.f, pillW, 23.f };
    g.next = { g.pill.getX() - 16.f - arrowW, 0.f, arrowW, h };
    g.name = { g.next.getX() - 8.f - nameW, 0.f, nameW, h };
    g.prev = { g.name.getX() - 8.f - arrowW, 0.f, arrowW, h };
    return g;
}

void HeaderBar::paint(juce::Graphics& g) {
    const auto geo = this->geo();
    const float h = (float) getHeight();

    g.setColour(theme::kInk);
    auto wm = lnf.barlow(20.f, true);
    wm.setExtraKerningFactor(0.06f);
    g.setFont(wm);
    g.drawText("MOTES", juce::Rectangle<float>(20.f, 0.f, 120.f, h), juce::Justification::centredLeft, false);

    g.setFont(lnf.mono(11.f));
    g.setColour(theme::kInk2);
    g.drawText(juce::String::fromUTF8("\xe2\x80\xb9"), geo.prev, juce::Justification::centred, false);
    g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), geo.next, juce::Justification::centred, false);
    g.setColour(theme::kInk);
    g.drawText(preset, geo.name, juce::Justification::centred, false);

    if (hold) {
        g.setColour(theme::kInk);
        g.fillRoundedRectangle(geo.pill, geo.pill.getHeight() / 2.f);
    } else {
        g.setColour(theme::kInk);
        g.drawRoundedRectangle(geo.pill.reduced(.5f), geo.pill.getHeight() / 2.f, 1.f);
    }
    g.setColour(hold ? theme::kPaper : theme::kInk);
    g.setFont(holdFont(lnf));
    g.drawText("HOLD", geo.pill, juce::Justification::centred, false);
}

bool HeaderBar::hitTest(int x, int y) {
    const auto geo = this->geo();
    const juce::Point<float> p((float) x, (float) y);
    return geo.prev.contains(p) || geo.next.contains(p) || geo.pill.contains(p);
}

void HeaderBar::mouseUp(const juce::MouseEvent& e) {
    if (!getLocalBounds().contains(e.getPosition()) || e.mouseWasDraggedSinceMouseDown()) return;
    const auto geo = this->geo();
    const auto p = e.position;
    if (geo.prev.contains(p)) { if (onPrev) onPrev(); }
    else if (geo.next.contains(p)) { if (onNext) onNext(); }
    else if (geo.pill.contains(p)) { hold = !hold; repaint(); if (onHold) onHold(hold); }
}
