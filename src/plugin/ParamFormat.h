#pragma once
#include <string>
#include <string_view>

// JUCE-free value formatters for the editor and for host parameter displays.
// All output is UTF-8 and uses the Unicode minus (U+2212), as the design handoff specifies.
namespace fmt {
std::string formatParam(std::string_view id, float v);
std::string formatSampleMeta(double seconds, double sampleRate, int channels);
}
