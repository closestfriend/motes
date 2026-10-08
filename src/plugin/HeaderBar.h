#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MotesLookAndFeel.h"

// Wordmark on the left; ‹ preset › and the HOLD pill on the right.
class HeaderBar : public juce::Component {
public:
    explicit HeaderBar(const MotesLookAndFeel& l) : lnf(l) {}
    std::function<void()> onPrev, onNext;
    std::function<void(bool)> onHold;
    void setPresetName(const juce::String& s) { preset = s; repaint(); }
    void setHold(bool h) { hold = h; repaint(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool hitTest(int x, int y) override;
private:
    struct Geo { juce::Rectangle<float> prev, name, next, pill; };
    Geo geo() const;
    const MotesLookAndFeel& lnf;
    juce::String preset { "Init" };
    bool hold = false;
};
