#include "MotesKnob.h"
#include "MotesTheme.h"
#include "ParamFormat.h"

MotesKnob::MotesKnob(juce::RangedAudioParameter& p, const KnobSpec& s, bool c, const MotesLookAndFeel& l)
    : param(p), spec(s), compact(c), lnf(l),
      attach(p, [this](float v) { value = v; repaint(); })
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    attach.sendInitialUpdate();
}

juce::Rectangle<float> MotesKnob::dialBounds() const {
    const float d = compact ? 40.f : 52.f;
    return { ((float) getWidth() - d) / 2.f, compact ? 0.f : 0.5f, d, d };   // primary dials sit on a half pixel in the reference
}

bool MotesKnob::hitTest(int x, int y) { return dialBounds().contains((float) x, (float) y); }

void MotesKnob::mouseDown(const juce::MouseEvent& e) {
    acc = param.getValue();
    last = e.position;
    attach.beginGesture();
}

void MotesKnob::mouseDrag(const juce::MouseEvent& e) {
    const auto d = e.position - last;
    last = e.position;
    // 200px = full range; right or up raises it. Incremental, so toggling Shift mid-drag doesn't jump.
    acc += ((d.x - d.y) / 200.f) * (e.mods.isShiftDown() ? 0.2f : 1.f);
    attach.setValueAsPartOfGesture(param.convertFrom0to1(juce::jlimit(0.f, 1.f, acc)));
}

void MotesKnob::mouseUp(const juce::MouseEvent&) { attach.endGesture(); }

void MotesKnob::mouseDoubleClick(const juce::MouseEvent&) {
    attach.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
}

void MotesKnob::paintDial(juce::Graphics& g, juce::Rectangle<float> r) const {
    const float s = r.getWidth() / 100.f;                      // strokes are defined in a 100-unit box and scale with it
    const float n = juce::jlimit(0.f, 1.f, param.convertTo0to1(value));
    const float a  = juce::degreesToRadians(-135.f + 270.f * n);
    const float a0 = spec.bipolar ? 0.f : juce::degreesToRadians(-135.f);
    const float trackW = compact ? 6.f : 5.f, bodyW = compact ? 2.5f : 2.f, ptrW = compact ? 4.f : 3.f;
    const auto stroke = [&](float w) {
        return juce::PathStrokeType(w * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    };
    const auto c = r.getCentre();
    const float R = 42.f * s, lim = juce::degreesToRadians(135.f);

    juce::Path track;
    track.addCentredArc(c.x, c.y, R, R, 0.f, -lim, lim, true);
    g.setColour(theme::kTrack);
    g.strokePath(track, stroke(trackW));

    if (std::abs(a - a0) > juce::degreesToRadians(0.5f)) {     // the mock skips < 0.5deg: a round cap would draw a dot
        juce::Path v;
        v.addCentredArc(c.x, c.y, R, R, 0.f, juce::jmin(a0, a), juce::jmax(a0, a), true);
        g.setColour(theme::kInk);
        g.strokePath(v, stroke(trackW));
    }

    g.setColour(theme::kInk);
    g.drawEllipse(c.x - 30.f * s, c.y - 30.f * s, 60.f * s, 60.f * s, bodyW * s);
    juce::Path ptr;
    ptr.startNewSubPath(c.x + 12.f * s * std::sin(a), c.y - 12.f * s * std::cos(a));
    ptr.lineTo(c.x + 30.f * s * std::sin(a), c.y - 30.f * s * std::cos(a));
    g.strokePath(ptr, stroke(ptrW));
}

void MotesKnob::paint(juce::Graphics& g) {
    const auto dial = dialBounds();
    paintDial(g, dial);

    // text rows measured from the reference: value then label, centred under the dial
    const float vy = dial.getY() + (compact ? 51.5f : 65.f), ly = dial.getY() + (compact ? 69.5f : 85.f);
    const float w = (float) getWidth();
    g.setColour(theme::kInk);
    g.setFont(lnf.mono(compact ? 10.f : 11.f));
    g.drawText(juce::String::fromUTF8(fmt::formatParam(spec.id, value).c_str()),
               juce::Rectangle<float>(0.f, vy - 7.f, w, 14.f), juce::Justification::centred, false);

    auto lf = lnf.barlow(compact ? 11.f : 12.f, false);
    lf.setExtraKerningFactor(0.10f);
    g.setColour(theme::kInk2);
    g.setFont(lf);
    g.drawText(spec.label, juce::Rectangle<float>(0.f, ly - 7.f, w, 14.f), juce::Justification::centred, false);
}
