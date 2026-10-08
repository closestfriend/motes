#pragma once
#include <juce_graphics/juce_graphics.h>

// Tokens from design_handoff_motes_editor/README.md. Band rectangles are the
// measured border-box values (docs/design-reference/1e-layout.json): the
// handoff's 44/136/40/118/~118/38 are content heights without the 1px hairlines.
namespace theme {
inline const juce::Colour kPaper{0xFFF4F2ED}, kInk{0xFF1A1A18}, kInk2{0xFF605D57},
    kHairline{0xFFD8D4CB}, kTrack{0xFFDDD9D0}, kWave{0xFFCFCAC0}, kWaveActive{0xFF8E8A82};
inline const juce::Colour kAccent = kInk;   // the ONE accent constant (alternative tried: 0xFFD9472B)

constexpr int kW = 640, kH = 496;
constexpr int headerY = 0,   headerH = 45;   // hairline at y = 44
constexpr int scopeY  = 45,  scopeH  = 137;  // hairline at y = 181
constexpr int stripY  = 182, stripH  = 41;   // hairline at y = 222
constexpr int primY   = 223, primH   = 119;  // hairline at y = 341
constexpr int secY    = 342, secH    = 115;  // no hairline of its own
constexpr int footY   = 457, footH   = 39;   // hairline at y = 457 (top)
}
