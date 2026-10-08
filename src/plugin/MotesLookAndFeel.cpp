#include "MotesLookAndFeel.h"
#include "MotesTheme.h"
#include "BinaryData.h"

MotesLookAndFeel::MotesLookAndFeel() {
    barlowMed  = juce::Typeface::createSystemTypefaceFor(MotesFonts::BarlowCondensedMedium_ttf,
                                                         (size_t) MotesFonts::BarlowCondensedMedium_ttfSize);
    barlowSemi = juce::Typeface::createSystemTypefaceFor(MotesFonts::BarlowCondensedSemiBold_ttf,
                                                         (size_t) MotesFonts::BarlowCondensedSemiBold_ttfSize);
    plexMono   = juce::Typeface::createSystemTypefaceFor(MotesFonts::IBMPlexMonoRegular_ttf,
                                                         (size_t) MotesFonts::IBMPlexMonoRegular_ttfSize);
    setColour(juce::Label::textColourId, theme::kInk);
}

juce::Font MotesLookAndFeel::barlow(float px, bool semibold) const {
    auto tf = semibold ? barlowSemi : barlowMed;
    juce::Font f = tf != nullptr ? juce::Font(tf)
                                 : juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.f,
                                              semibold ? juce::Font::bold : juce::Font::plain);
    return f.withPointHeight(px);
}

juce::Font MotesLookAndFeel::mono(float px) const {
    juce::Font f = plexMono != nullptr ? juce::Font(plexMono)
                                       : juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.f,
                                                    juce::Font::plain);
    return f.withPointHeight(px);
}
