#pragma once

// Which parameters get a knob, their on-screen labels (the handoff renames "Macro Speed" to DRIFT,
// "Grain Size" to SIZE) and whether the value arc grows from 12 o'clock (bipolar) or from the start.
struct KnobSpec { const char* id; const char* label; bool bipolar; };

inline constexpr KnobSpec kPrimary[6] = {
    { "density",   "DENSITY",  false },
    { "grain",     "SIZE",     false },
    { "spread",    "SPREAD",   false },
    { "position",  "POSITION", false },
    { "pitchsemi", "PITCH",    true  },
    { "master",    "MASTER",   false } };
inline constexpr KnobSpec kPanGroup[3] = {
    { "panspread", "PAN",   false },
    { "macro",     "DRIFT", true  },
    { "pitchcent", "FINE",  true  } };
inline constexpr KnobSpec kEnvGroup[4] = {
    { "attack",  "ATTACK",  false },
    { "decay",   "DECAY",   false },
    { "sustain", "SUSTAIN", false },
    { "release", "RELEASE", false } };
