#include "ScopeView.h"
#include "MotesTheme.h"

// Auto-gain values straight from the design handoff. Observed with this engine: output peaks sit
// around 0.1, so the figure fills roughly 60% of the box once the follower has settled (~1 s).
static constexpr float kGainFloor = .3f;

ScopeView::ScopeView(const MotesLookAndFeel& l) : lnf(l) {}

// (Re)allocate at the given scale, carrying the existing figure over so a scale change doesn't blank it.
void ScopeView::ensureImage(float newScale) {
    const int pw = juce::jmax(1, juce::roundToInt(getWidth() * newScale));
    const int ph = juce::jmax(1, juce::roundToInt(getHeight() * newScale));
    if (image.isValid() && image.getWidth() == pw && image.getHeight() == ph) { scale = newScale; return; }
    juce::Image fresh(juce::Image::ARGB, pw, ph, true);
    juce::Graphics g(fresh);
    g.fillAll(theme::kPaper);
    if (image.isValid()) g.drawImage(image, fresh.getBounds().toFloat());
    image = fresh;
    scale = newScale;
}

void ScopeView::addBlock(const float* l, const float* r, int n) {
    if (getWidth() <= 0 || getHeight() <= 0) return;
    ensureImage(scale);
    juce::Graphics g(image);
    g.addTransform(juce::AffineTransform::scale(scale));

    const float w = (float) getWidth(), h = (float) getHeight();
    const float cx = w / 2.f, cy = h / 2.f, ry = h * .4f, rx = juce::jmin(w * .42f, ry * 2.6f);

    // Order matches the reference: fade, guides, labels, then the new trace.
    g.setColour(theme::kPaper.withAlpha(.3f));
    g.fillRect(0.f, 0.f, w, h);

    juce::Path guides;
    guides.startNewSubPath(cx - rx, cy + ry); guides.lineTo(cx + rx, cy - ry);
    guides.startNewSubPath(cx + rx, cy + ry); guides.lineTo(cx - rx, cy - ry);
    guides.startNewSubPath(cx, cy - ry);      guides.lineTo(cx, cy + ry);
    g.setColour(theme::kTrack);
    g.strokePath(guides, juce::PathStrokeType(1.f));

    g.setColour(theme::kWaveActive);
    g.setFont(lnf.mono(10.f));
    const auto label = [&](const char* t, float x, float y) {
        g.drawText(t, juce::Rectangle<float>(x - 10.f, y - 6.f, 20.f, 12.f), juce::Justification::centred, false);
    };
    label("L", cx - rx - 10.f, cy - ry);
    label("R", cx + rx + 10.f, cy - ry);
    label("M", cx, cy - ry - 9.f);

    // Auto-gain on the block's peak (floor 0.3). Non-finite samples are skipped everywhere.
    float pv = 0.f;
    for (int i = 0; i < n; ++i) {
        const float x = r[i] - l[i], y = l[i] + r[i];
        if (!std::isfinite(x) || !std::isfinite(y)) continue;
        pv = juce::jmax(pv, std::abs(x), std::abs(y));
    }
    const float p = juce::jmax(kGainFloor, pv);
    gain = p > gain ? p : gain * .97f + p * .03f;
    const float k = .9f / gain;

    if (pv > 1e-6f) {                                   // silence draws nothing (a zero-length path leaves a stray dot)
        juce::Path trace;
        bool first = true;
        for (int i = 0; i < n; ++i) {
            const float x = r[i] - l[i], y = l[i] + r[i];
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            const juce::Point<float> pt { juce::jlimit(cx - rx, cx + rx, cx + x * k * rx),
                                          juce::jlimit(cy - ry, cy + ry, cy - y * k * ry) };
            if (first) trace.startNewSubPath(pt); else trace.lineTo(pt);
            first = false;
        }
        g.setColour(theme::kInk.withAlpha(.8f));
        g.strokePath(trace, juce::PathStrokeType(1.f, juce::PathStrokeType::curved));
    }
}

void ScopeView::paint(juce::Graphics& g) {
    const float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (sc != scale || !image.isValid()) ensureImage(sc);
    g.drawImage(image, getLocalBounds().toFloat());
}
