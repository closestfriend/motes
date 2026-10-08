#pragma once

// Factory presets from the design handoff. They set the 13 parameters only, never the sample.
// Values are denormalised (real units), in kPresetIds order.
struct Preset { const char* name; float v[13]; };

inline constexpr const char* kPresetIds[13] = {
    "density", "grain", "position", "spread", "panspread", "pitchsemi", "pitchcent",
    "macro", "attack", "decay", "sustain", "release", "master" };

inline constexpr int kNumPresets = 4;
inline constexpr int kInitPreset = 3;   // its values equal the parameter defaults, so a fresh instance shows "Init"

inline constexpr Preset kPresets[kNumPresets] = {
    { "Isolation Drift", { 42, 180, .38f, .22f, .45f,   0,  0,  .12f, .35f, .6f,  .7f, 1.8f, .8f  } },
    { "Glass Rain",      { 88,  24, .70f, .50f, .90f,  12, -7, -.30f, .005f, .25f, .40f, .60f, .70f } },
    { "Low Tide",        {  9, 460, .15f, .08f, .25f, -12,  4,  .04f, 1.6f, 1.0f, .90f, 3.5f, .85f } },
    { "Init",            { 30, 120, .50f, .05f, .20f,   0,  0,  0.f,  .01f, .1f,  .8f,  .3f,  .8f  } } };
