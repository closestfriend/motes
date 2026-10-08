// JUCE-free tests for the editor's value formatters (src/plugin/ParamFormat.*).
// -UNDEBUG is forced in CMake so these asserts survive Release builds.
#include "ParamFormat.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using fmt::formatParam;


// Typed values in a host's parameter field must read back as what the display strings mean.
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static void testParse() {
    using fmt::parseParam;
    assert(near(parseParam("spread", "50"), .5f, 1e-4f));            // shown as "22%": typing 50 means 50%
    assert(near(parseParam("spread", "50%"), .5f, 1e-4f));
    assert(near(parseParam("attack", "350 ms"), .35f, 1e-4f));
    assert(near(parseParam("attack", "350"), .35f, 1e-4f));          // > 5 can't be seconds (max 5 s): read as ms
    assert(near(parseParam("attack", "0.35"), .35f, 1e-4f));
    assert(near(parseParam("release", "1.80 s"), 1.8f, 1e-4f));
    assert(near(parseParam("release", "2"), 2.f, 1e-4f));
    assert(near(parseParam("master", "-6"), std::pow(10.f, -6.f / 20.f), 1e-4f));
    assert(near(parseParam("master", "\u22121.9 dB"), std::pow(10.f, -1.9f / 20.f), 1e-4f));   // pasting the shown text back
    assert(parseParam("master", "\u2212\u221e dB") == 0.f);
    assert(parseParam("master", "-inf") == 0.f);
    assert(near(parseParam("pitchsemi", "+12 st"), 12.f, 1e-4f));
    assert(near(parseParam("pitchsemi", "\u221212 st"), -12.f, 1e-4f));
    assert(parseParam("pitchsemi", "\u00b10 st") == 0.f);
    assert(near(parseParam("macro", "\u22120.30"), -.3f, 1e-4f));
    assert(near(parseParam("density", "42 g/s"), 42.f, 1e-4f));
    assert(parseParam("density", "banana") == 0.f);                   // garbage is 0 (JUCE clamps it into range)
    // round trip: what we display, we can parse back (within the display's own rounding)
    struct { const char* id; float v; float tol; } rt[] = {
        {"density", 42, .5f}, {"grain", 180, .5f}, {"spread", .22f, .005f}, {"position", .38f, .005f}, {"sustain", .7f, .005f},
        {"pitchsemi", -12, .01f}, {"pitchcent", -7, .01f}, {"macro", .12f, .005f}, {"attack", .35f, .0005f},
        {"decay", .6f, .0005f}, {"release", 1.8f, .005f}, {"master", .8f, .01f}, {"panspread", .45f, .005f} };
    for (auto& r : rt) assert(near(parseParam(r.id, formatParam(r.id, r.v)), r.v, r.tol));
}

int main() {
    assert(formatParam("density", 42) == "42 g/s");
    assert(formatParam("grain", 180) == "180 ms");
    assert(formatParam("spread", .22f) == "22%");
    assert(formatParam("position", .38f) == "38%");
    assert(formatParam("sustain", .7f) == "70%");
    assert(formatParam("pitchsemi", 12) == "+12 st");
    assert(formatParam("pitchsemi", -12) == "−12 st");
    assert(formatParam("pitchsemi", 0) == "±0 st");
    assert(formatParam("pitchcent", -7) == "−7 ct");
    assert(formatParam("pitchcent", 0) == "±0 ct");
    assert(formatParam("macro", .12f) == "+0.12");
    assert(formatParam("macro", -.3f) == "−0.30");
    assert(formatParam("macro", 0) == "±0.00");
    assert(formatParam("master", .8f) == "−1.9 dB");
    assert(formatParam("master", 0) == "−∞ dB");
    assert(formatParam("master", 1.f) == "0.0 dB");
    assert(formatParam("master", .9995f) == "0.0 dB");      // would print "-0.0" naively
    assert(formatParam("attack", .35f) == "350 ms");
    assert(formatParam("decay", .6f) == "600 ms");
    assert(formatParam("release", 1.8f) == "1.80 s");
    assert(formatParam("decay", .9996f) == "1.00 s");       // would print "1000 ms" naively
    assert(fmt::formatSampleMeta(6.2, 48000, 2) == "6.2 s · 48 kHz · stereo");
    assert(fmt::formatSampleMeta(1.0, 44100, 1) == "1.0 s · 44.1 kHz · mono");
    assert(fmt::formatSampleMeta(3.0, 48000, 6) == "3.0 s · 48 kHz · 6 ch");
    testParse();
    std::puts("test_format OK");
    return 0;
}
