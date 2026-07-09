# Motes — Custom UI Design (v1)

**Status:** approved-in-advance 2026-07-09 (Hunter delegated the remaining calls + said "go ahead with the build"; design open to redirect on review)
**Feature:** replace the generic `AudioProcessorEditor` with a bespoke, audio-reactive editor.
**Prereq:** builds on the completed Motes plugin (`~/Projects/motes/`) — C DSP core + engine ABI + JUCE shell with 13 APVTS params.

## 1. Goal & mood

A bespoke, **code-drawn** editor whose hero is a **bioluminescent mote field** that reacts to the live audio — grains bloom as flecks of light in a dark void. Dark water, cool light. Tonally a sibling of "Soot." The particle field is a *true visual readout of the granular engine*, which is uniquely possible because we own the DSP.

## 2. Scope (v1) & non-goals

**In v1:**
- Custom `MotesEditor` replacing the `GenericAudioProcessorEditor`.
- A **reactive particle field** (the `MotesField` component) as the hero.
- Custom-drawn knobs for all **13 params** via a `MotesLookAndFeel`, laid out in groups, bound to the existing APVTS.
- Keep the existing **Load WAV** button + filename display.
- Small engine additions: two lock-free meters (output RMS, grain-spawn count) + getters.
- Fixed window **768×512**.
- Procedural dark-gradient background (so nothing blocks on art); optional embedded background plate slots in behind the motes when provided.

**Explicit non-goals (fast-follows, separate specs):**
- Waveform + position-playhead display.
- Resizable window (the vector/procedural approach makes this easy to add later).
- Load-Image / luminance source (separate *engine* feature on the v2 backlog).
- Custom embedded font (v1 uses JUCE's default typeface; a `.ttf` embed is a later polish once an asset exists).

## 3. Architecture

```
┌ MotesEditor (juce::AudioProcessorEditor, 768×512, fixed) ───────────┐
│  owns: MotesLookAndFeel, MotesField, 13 Slider + SliderAttachment,   │
│        Load-WAV TextButton + filename Label, a 60fps VBlank/Timer     │
│  layout: resized() → field (top ~60%), sample bar, knob panel (~40%)  │
│                                                                       │
│  ┌ MotesField (juce::Component) ──────────────────────────────────┐  │
│  │  procedural background gradient (or embedded plate underlay)     │  │
│  │  particle sim: fixed pool of Mote structs, advanced per frame    │  │
│  │  reads engine meters (atomics) + APVTS params each tick          │  │
│  └──────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
        │ reads (GUI thread, ~60fps)                ▲ writes (audio thread)
        ▼                                           │
┌ GranularEngine (C core) — NEW meters ──────────────────────────────┐
│  _Atomic float  out_rms;         // set in ge_render each block      │
│  _Atomic unsigned grains_fired;  // += voice_render() return value   │
│  float    ge_get_out_rms(GranularEngine*);                          │
│  unsigned ge_get_grains_fired(GranularEngine*);                     │
└────────────────────────────────────────────────────────────────────┘
```

### Thread boundary (the critical correctness point)
The audio thread (`ge_render` → `voice_render`) and the GUI thread (the editor's ~60fps callback) are separate. The bridge is **lock-free**: the audio thread writes two relaxed atomics; the GUI reads them, advances the sim, calls `repaint()`. No locks, no allocation on the audio thread — consistent with the engine's existing RT-safety contract. A slightly stale/torn meter read is visually harmless.

## 4. Engine additions (C core)

- `voice_render(...)` returns `int` = number of grains spawned during the call (was `void`). `spawn_grain` success increments a local counter.
- `granular_engine.c`: add `_Atomic float out_rms` and `_Atomic unsigned grains_fired` (via `<stdatomic.h>`, `memory_order_relaxed`).
  - In `ge_render`: sum `voice_render` return values into `grains_fired` (atomic add); after summing voices, compute block RMS from the L/R output and `atomic_store` it.
- New ABI: `float ge_get_out_rms(GranularEngine*)`, `unsigned ge_get_grains_fired(GranularEngine*)` (monotonic counter; the editor tracks deltas between polls).
- **RT-safety preserved:** atomics only, no alloc/lock. **TDD-able:** in `test_core`, assert `grains_fired` increases after rendering a held note at nonzero density, and `out_rms > 0` while sounding vs `~0` after full release.

## 5. Signal → visual mapping

| Signal | Source | Visual |
|--------|--------|--------|
| Grain fired (event) | `grains_fired` delta since last frame | spawn N bright **mote-blooms** (flare→fade); Density ⇒ literal mote density |
| Output level | `out_rms` | **ambient bed** brightness/count + drift energy (calm↔lively) |
| Position | APVTS `position` | horizontal x of a soft **source-glow**; blooms emanate from it |
| Macro-Speed | APVTS `macro` | slow horizontal drift of the source-glow |
| Spread / Pan Spread | APVTS `spread` / `panspread` | positional jitter / horizontal spread of bloom spawn points |
| (subtle) active voices | `ge_active_voice_count` | gentle overall field density |

Blooms are capped (pool of ~400 motes; if grains fire faster than the pool, spawn a representative subset — never unbounded). Additive-ish blending via layered translucent fills.

## 6. Particle sim (`MotesField`)

- Fixed array `Mote motes[MAX_MOTES]` (MAX_MOTES ≈ 400): `x, y, vx, vy, age, life, size, hue, active`.
- Per frame (dt from the timer): advance position, age; recycle dead motes; apply slow drift + gentle upward buoyancy + subtle noise.
- Ambient spawns proportional to `out_rms`; **event** spawns from `grains_fired` delta (brighter, larger, shorter life) near the source-glow x with `spread`/`panspread` jitter.
- Paint: background (gradient or plate) → soft radial gradients per mote (cyan↔violet by hue) → optional faint source-glow. Keep counts sane for message-thread paint.
- Determinism/animation caveat: JUCE timer supplies real dt; the sim uses a local PRNG seeded once (no reliance on wall-clock in a way that breaks).

## 7. LookAndFeel & layout (`MotesLookAndFeel`)

- `drawRotarySlider`: dark knob body, thin outer ring, a **glowing cyan→violet indicator arc** from min to current value; small tick at the pointer. Label above, value readout below (JUCE default font v1).
- Layout (`resized()`), fixed 768×512:
  - **Field** — top, full width, ~300px tall (hero).
  - **Sample bar** — thin strip: "Load WAV…" button + filename label.
  - **Knob panel** — bottom ~190px, four labeled groups left→right:
    - **Grain:** Density, Grain Size, Spread, Pan Spread
    - **Pitch/Pos:** Position, Macro-Speed, Pitch Coarse, Pitch Fine
    - **Amp:** Attack, Decay, Sustain, Release
    - **Master:** Master (set slightly apart)
  - Small "motes" wordmark in a field corner (text, not asset).

## 8. Asset integration (optional, non-blocking)

- If/when Hunter provides a background plate: add `assets/` + `juce_add_binary_data` in CMake; `MotesField` uses `ImageCache::getFromMemory(BinaryData::...)` as the background underlay, drawn beneath the live motes, scaled to fill. Absent an asset, a procedural vertical gradient (near-black `#0A0D12` → deep indigo `#0E1420`) is the background. **No code path depends on the asset existing.**
- Palette: mote cores cool-white `#EAF6FF`; glow cyan `#34C7DE` + violet `#A07CFF`; labels dim slate `#7C8AA0`, values `#C6D4E6`.

## 9. Testing

- **C core meters:** TDD in `test_core` (grain-fired increments; RMS>0 sounding vs ~0 silent) — clean under ASan/UBSan like the rest of the core.
- **Plugin:** `auval -v aumu Mote Hntr` must still SUCCEED after the editor swap.
- **Visual/interaction:** Hunter verifies in Ableton — motes react to playing, knobs move sound, Load WAV works, no UI stutter. (Particle *look* is inherently human-judged.)

## 10. Milestones

1. **Engine meters** (C, TDD): `voice_render` returns spawn count; `out_rms` + `grains_fired` atomics + getters; tests green.
2. **MotesLookAndFeel + static layout**: custom knobs for all 13 params on a procedural gradient bg; builds, auval passes, knobs control sound. (No particles yet.)
3. **MotesField (ambient)**: particle pool + drift + RMS-driven ambient bed, on a 60fps timer.
4. **MotesField (reactive)**: grain-fired blooms + Position source-glow + spread/macro mapping. → **v1 UI complete.**
5. (fast-follow, separate) background-plate embed when the asset lands.

## 11. Risks

- **Paint performance** on the message thread — mitigate with a bounded pool (~400), VBlank sync, cheap per-mote gradients; profile if it stutters.
- **Meter thread-safety** — relaxed atomics only; no locks; benign staleness.
- **auval regression** from the editor swap — validated at Milestone 2, not the end.
