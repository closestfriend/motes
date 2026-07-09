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

void MotesField::timerCallback() {
    const float dt = 1.0f/60.0f;
    float rms = proc.getOutRms();
    // ambient spawn rate scales with loudness (gentle)
    float rate = juce::jlimit(0.0f, 12.0f, rms * 60.0f);
    int n = (int)rate + ((rng.nextFloat() < (rate - (int)rate)) ? 1 : 0);
    spawnAmbient(n);
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
}
