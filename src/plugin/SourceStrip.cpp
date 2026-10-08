#include "SourceStrip.h"
#include "MotesTheme.h"

SourceStrip::SourceStrip(juce::AudioProcessorValueTreeState& apvts)
    : posParam(*apvts.getParameter("position")),
      posAttach(*apvts.getParameter("position"), [this](float v) { position = v; repaint(); }),
      spreadAttach(*apvts.getParameter("spread"), [this](float v) { spread = v; repaint(); })
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    posAttach.sendInitialUpdate();
    spreadAttach.sendInitialUpdate();
}

void SourceStrip::setPeaks(const std::array<float, kBins>& p) {
    peaks = p;
    cacheScale = 0.f;
    repaint();
}

// One mirrored 1px bar around the baseline, height = peak * half-height.
void SourceStrip::drawBar(juce::Graphics& g, float x, float by, float am) const {
    const float w = (float) getWidth();
    const float a = peaks[(size_t) juce::jlimit(0, kBins - 1, (int) (x / w * kBins))] * am;
    g.fillRect(x, by - a, 1.f, a * 2.f + .5f);
}

void SourceStrip::addGrainEvent(const GrainEvent& e) {
    flashes[(size_t) nextFlash] = { e.pos, e.span, juce::Time::getMillisecondCounterHiRes() };
    nextFlash = (nextFlash + 1) % kMaxFlashes;
}

void SourceStrip::setPlayheads(const float* ph, int n) {
    numPlayheads = juce::jmin(n, kMaxPlayheads);
    for (int i = 0; i < numPlayheads; ++i) playheads[(size_t) i] = ph[i];
}

void SourceStrip::rebuildCache(float sc) {
    cacheScale = sc;
    cache = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt(getWidth() * sc)),
                        juce::jmax(1, juce::roundToInt(getHeight() * sc)), true);
    juce::Graphics c(cache);
    c.addTransform(juce::AffineTransform::scale(sc));
    const float w = (float) getWidth(), h = (float) getHeight();
    c.setColour(theme::kWave);
    for (float x = 0.f; x < w; x += 2.f) drawBar(c, x, h * .5f, h * .38f);
}

void SourceStrip::paint(juce::Graphics& g) {
    const float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!cache.isValid() || sc != cacheScale) rebuildCache(sc);
    g.drawImage(cache, getLocalBounds().toFloat());

    const float w = (float) getWidth(), h = (float) getHeight();
    const float by = h * .5f, am = h * .38f;
    const float bottom = by + am;                         // the mock stops the band and line here, not at the hairline
    const float sw = juce::jmax(2.f, spread * w);

    // One line + spread band per sounding voice (the engine's real playhead, which moves with Drift);
    // if nothing is sounding, a single one at the Position parameter, which is also the drag handle.
    float heads[kMaxPlayheads];
    int nh = 0;
    for (int i = 0; i < numPlayheads; ++i) if (playheads[(size_t) i] >= 0.f) heads[nh++] = playheads[(size_t) i];
    if (nh == 0) heads[nh++] = position;

    for (int i = 0; i < nh; ++i) {
        const float px = heads[i] * w;
        g.setColour(theme::kAccent.withAlpha(.06f));
        g.fillRect(px - sw / 2.f, 0.f, sw, bottom);
        g.setColour(theme::kWaveActive);                  // bars inside the spread band
        for (float x = juce::jmax(0.f, std::floor((px - sw / 2.f) / 2.f) * 2.f); x < juce::jmin(w, px + sw / 2.f); x += 2.f)
            drawBar(g, x, by, am);
    }
    g.setColour(theme::kAccent);
    for (int i = 0; i < nh; ++i) g.fillRect(heads[i] * w - .5f, 0.f, 1.f, bottom);

    // Grain-cut flashes: the slice of waveform each real grain reads, fading over 250 ms.
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (const auto& f : flashes) {
        const float age = (float) (now - f.startMs);
        if (f.startMs <= 0.0 || age >= 250.f) continue;
        const float fw = juce::jlimit(3.f, .25f * w, f.span * w), gx = f.pos * w;
        g.setColour(theme::kAccent.withAlpha((1.f - age / 250.f) * .9f));
        for (float i = 0.f; i < fw; i += 1.5f) {
            const float u = i / fw, win = .5f - .5f * std::cos(juce::MathConstants<float>::twoPi * u);
            const int bin = juce::jlimit(0, kBins - 1, (int) ((gx - fw / 2.f + u * fw) / w * kBins));
            const float s = peaks[(size_t) bin] * am * win;
            g.fillRect(gx + i - fw / 2.f, by - s, .9f, s * 2.f + .3f);
        }
    }
}

void SourceStrip::setFromMouse(const juce::MouseEvent& e) {
    const float n = juce::jlimit(0.f, 1.f, e.position.x / (float) juce::jmax(1, getWidth()));
    posAttach.setValueAsPartOfGesture(posParam.convertFrom0to1(n));
}
void SourceStrip::mouseDown(const juce::MouseEvent& e) { posAttach.beginGesture(); setFromMouse(e); }
void SourceStrip::mouseDrag(const juce::MouseEvent& e) { setFromMouse(e); }
void SourceStrip::mouseUp(const juce::MouseEvent&)     { posAttach.endGesture(); }
