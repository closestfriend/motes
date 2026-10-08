#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MotesLookAndFeel.h"

// File name + "Load…" link on the left, sample meta on the right.
class FooterBar : public juce::Component {
public:
    explicit FooterBar(const MotesLookAndFeel& l) : lnf(l) {}
    std::function<void()> onLoad;
    void setFile(const juce::String& s) { file = s; repaint(); }
    void setMeta(const juce::String& s) { meta = s; repaint(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool hitTest(int x, int y) override { return loadBounds().contains((float) x, (float) y); }
    juce::Rectangle<float> loadBounds() const;
private:
    juce::String displayedFile() const;      // the name, elided so it never reaches the Load… link or the meta
    const MotesLookAndFeel& lnf;
    juce::String file { "built-in tone" }, meta { juce::String::fromUTF8("1.0 s \xc2\xb7 44.1 kHz \xc2\xb7 mono") };
};
