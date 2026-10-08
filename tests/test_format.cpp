// JUCE-free tests for the editor's value formatters (src/plugin/ParamFormat.*).
// -UNDEBUG is forced in CMake so these asserts survive Release builds.
#include "ParamFormat.h"
#include <cassert>
#include <cstdio>
using fmt::formatParam;

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
    std::puts("test_format OK");
    return 0;
}
