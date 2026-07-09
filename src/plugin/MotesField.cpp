#include "MotesField.h"

MotesField::MotesField(MotesProcessor& p) : proc(p) { startTimerHz(60); }

void MotesField::spawnAmbient(int n) {
    for (int i=0;i<n;i++) {
        Mote& m = motes[cursor]; cursor = (cursor+1) % MAX_MOTES;
        m.active = true;
        m.x = rng.nextFloat() * (float)getWidth();
        m.y = (float)getHeight() * (0.4f + 0.6f*rng.nextFloat());
        m.vx = (rng.nextFloat()-0.5f) * 6.0f;
        m.vy = -(4.0f + rng.nextFloat()*10.0f);          // slow upward buoyancy
        m.age = 0.0f; m.life = 2.0f + rng.nextFloat()*3.0f;
        m.size = 1.5f + rng.nextFloat()*2.0f;
        m.hue = rng.nextFloat();                          // 0=cyan .. 1=violet
    }
}

void MotesField::spawnBloom(int n, float gx) {
    float spread   = proc.apvts.getRawParameterValue("spread")->load();
    float panSpread= proc.apvts.getRawParameterValue("panspread")->load();
    for (int i=0;i<n;i++) {
        Mote& m = motes[cursor]; cursor = (cursor+1) % MAX_MOTES;
        m.active = true;
        float jitter = (rng.nextFloat()-0.5f) * (0.15f + panSpread*0.85f) * (float)getWidth();
        m.x = juce::jlimit(0.0f, (float)getWidth(), gx*(float)getWidth() + jitter);
        m.y = (float)getHeight() * (0.55f + 0.4f*rng.nextFloat());
        m.vx = (rng.nextFloat()-0.5f) * (10.0f + spread*40.0f);
        m.vy = -(12.0f + rng.nextFloat()*22.0f);          // blooms rise faster
        m.age = 0.0f; m.life = 0.5f + rng.nextFloat()*0.9f;  // short, punchy
        m.size = 2.5f + rng.nextFloat()*3.5f;              // bigger/brighter
        m.hue = rng.nextFloat();
    }
}

void MotesField::timerCallback() {
    const float dt = 1.0f/60.0f;
    float rms = proc.getOutRms();
    // ambient spawn rate scales with loudness (gentle)
    float rate = juce::jlimit(0.0f, 12.0f, rms * 60.0f);
    int n = (int)rate + ((rng.nextFloat() < (rate - (int)rate)) ? 1 : 0);
    spawnAmbient(n);
    // smooth the source-glow toward the Position knob
    float targetX = proc.apvts.getRawParameterValue("position")->load();
    glowX += (targetX - glowX) * 0.15f;
    // grain-fired delta -> blooms (cap so a huge burst can't flood the pool)
    unsigned fired = proc.getGrainsFired();
    unsigned delta = fired - lastFired; lastFired = fired;
    int blooms = (int)juce::jmin<unsigned>(delta, 24u);
    if (blooms > 0) spawnBloom(blooms, glowX);
    for (auto& m : motes) {
        if (!m.active) continue;
        m.age += dt;
        if (m.age >= m.life) { m.active = false; continue; }
        m.x += m.vx*dt; m.y += m.vy*dt;
        m.vx += (rng.nextFloat()-0.5f) * 4.0f * dt;       // subtle drift noise
    }
    repaint();
}

void MotesField::paint(juce::Graphics& g) {
    juce::ColourGradient bg(juce::Colour(0xff0a0d12), 0, 0,
                            juce::Colour(0xff0e1420), 0, (float)getHeight(), false);
    g.setGradientFill(bg); g.fillAll();
    float gx = glowX * (float)getWidth();
    float gy = (float)getHeight() * 0.7f;
    juce::ColourGradient glow(juce::Colour(0x3334c7de), gx, gy,
                              juce::Colours::transparentBlack, gx, gy-70.0f, true);
    g.setGradientFill(glow);
    g.fillEllipse(gx-70.0f, gy-70.0f, 140.0f, 140.0f);
    juce::Colour cyan(0xff34c7de), violet(0xffa07cff);
    for (auto& m : motes) {
        if (!m.active) continue;
        float t = m.age / m.life;
        float alpha = juce::jlimit(0.0f, 1.0f, std::sin(t * juce::MathConstants<float>::pi)); // fade in/out
        auto col = cyan.interpolatedWith(violet, m.hue).withAlpha(alpha * 0.5f);
        float r = m.size * (1.0f + t*0.5f);
        g.setColour(col);
        g.fillEllipse(m.x-r, m.y-r, r*2, r*2);            // soft dot
    }
    g.setColour(juce::Colour(0x66c6d4e6));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("motes", getLocalBounds().reduced(10), juce::Justification::topLeft);
}
