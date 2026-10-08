#pragma once
#include <string>
#include <string_view>

// JUCE-free value formatters for the editor and for host parameter displays.
// All output is UTF-8 and uses the Unicode minus (U+2212), as the design handoff specifies.
namespace fmt {
std::string formatParam(std::string_view id, float v);
// Inverse of formatParam, for typed entry in a host's parameter field. Returns the real (denormalised)
// value; garbage reads as 0 (the caller's range clamps it). Percent parameters mean percent with or without
// a "%", master means dB, envelope times with no unit are seconds unless > 5 (then milliseconds).
float parseParam(std::string_view id, std::string_view text);
std::string formatSampleMeta(double seconds, double sampleRate, int channels);
}
