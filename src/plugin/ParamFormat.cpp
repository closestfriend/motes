#include "ParamFormat.h"
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>

namespace fmt {

// JS Math.round rounds halves toward +infinity; std::round rounds them away from zero.
static long jsRound(double x) { return (long) std::floor(x + 0.5); }

static std::string sgn(double v, double eps) {
    return v > eps ? "+" : v < -eps ? "−" : "±";
}

static std::string num(double v, int dp) {
    char b[48];
    std::snprintf(b, sizeof b, "%.*f", dp, v);
    return b;
}

std::string formatParam(std::string_view id, float v) {
    if (id == "density") return num(v, 0) + " g/s";
    if (id == "grain")   return num(v, 0) + " ms";
    if (id == "position" || id == "spread" || id == "panspread" || id == "sustain")
        return std::to_string(jsRound(v * 100.0)) + "%";
    if (id == "pitchsemi" || id == "pitchcent") {
        const long r = jsRound(v);
        return sgn((double) r, 0) + std::to_string(std::labs(r)) + (id == "pitchsemi" ? " st" : " ct");
    }
    if (id == "macro") return sgn(v, .005) + num(std::fabs(v), 2);
    if (id == "attack" || id == "decay" || id == "release") {
        const long ms = jsRound(v * 1000.0);
        return ms < 1000 ? std::to_string(ms) + " ms" : num(v < 1.0f ? 1.0 : v, 2) + " s";
    }
    if (id == "master") {
        if (v <= .0005f) return "−∞ dB";
        std::string s = num(20.0 * std::log10((double) v), 1);
        if (s == "-0.0") s = "0.0";
        for (auto p = s.find('-'); p != std::string::npos; p = s.find('-')) s.replace(p, 1, "−");
        return s + " dB";
    }
    return {};
}

float parseParam(std::string_view id, std::string_view text) {
    // Normalise: Unicode minus -> '-', "±" -> nothing, drop spaces, lower-case.
    std::string t;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = (unsigned char) text[i];
        if (text.compare(i, 3, "\u2212") == 0) { t += '-'; i += 2; }
        else if (text.compare(i, 2, "\u00b1") == 0) { i += 1; }
        else if (text.compare(i, 3, "\u221e") == 0) { t += "inf"; i += 2; }
        else if (c != ' ') t += (char) std::tolower(c);
    }
    if (t.find("inf") != std::string::npos) return 0.f;      // "-inf dB" or the infinity glyph: silence
    const char* b = t.c_str();
    char* end = nullptr;
    const double n = std::strtod(b, &end);
    if (end == b) return 0.f;                                                        // no number at all
    const std::string unit(end);
    if (id == "position" || id == "spread" || id == "panspread" || id == "sustain") return (float) (n / 100.0);
    if (id == "attack" || id == "decay" || id == "release") {
        if (unit.rfind("ms", 0) == 0) return (float) (n / 1000.0);
        if (unit.rfind("s", 0) == 0) return (float) n;
        return (float) (n > 5.0 ? n / 1000.0 : n);
    }
    if (id == "master") return (float) std::pow(10.0, n / 20.0);
    return (float) n;
}

std::string formatSampleMeta(double seconds, double sampleRate, int channels) {
    std::string khz = num(sampleRate / 1000.0, 1);
    if (khz.size() > 2 && khz.compare(khz.size() - 2, 2, ".0") == 0) khz.resize(khz.size() - 2);
    const std::string ch = channels == 1 ? "mono" : channels == 2 ? "stereo" : std::to_string(channels) + " ch";
    return num(seconds, 1) + " s · " + khz + " kHz · " + ch;
}

}  // namespace fmt
