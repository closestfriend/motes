#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Owns the embedded typefaces. Every px size below is a CSS em size (the unit the
// design handoff uses), not a JUCE font height. If a face fails to load, the
// system sans/monospace is used instead, so nothing depends on the assets existing.
class MotesLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MotesLookAndFeel();
    juce::Font barlow(float px, bool semibold) const;   // Barlow Condensed 500 / 600
    juce::Font mono(float px) const;                    // IBM Plex Mono 400
private:
    juce::Typeface::Ptr barlowMed, barlowSemi, plexMono;
};
