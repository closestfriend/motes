# KINETIC GRANULAR — Native Plugin (Design)

**Status:** approved 2026-06-30
**Author:** Hunter + Claude (brainstorming session)
**Reference implementation:** `~/Projects/cf audio pack/kinetic-granular/src/lib/audio.ts` (working Web Audio granular engine — the sonic spec; this native port was moved out of that AI Studio bundle to `~/Projects/kinetic-granular-native/`)

## 1. Purpose

A polyphonic granular sample-as-synth that loads inside Ableton Live as a **native arm64 AU/VST3** on Apple Silicon (M4). This is the in-DAW instrument that the browser app `kinetic-granular` proves out but can never be — and a spiritual, native-Apple-Silicon approximation of iZotope Iris (which was Intel-only, discontinued Oct 2022).

Scope is deliberately a *rudimentary approximation*: granular synthesis of a loaded sample with real MIDI polyphony. Not spectral carving, not a sample library, not Iris feature-parity.

## 2. Non-goals (v1)

- No spectral / FFT editing (Iris's spectrogram-carving is explicitly out).
- No multi-layer sample stack (Iris had 3 + sub-osc). v1 = **one** sample layer.
- No custom waveform GUI — generic host/JUCE slider UI for v1.
- No AIFF/FLAC — **WAV only** for v1 load.
- No sample-from-image luminance trick (deferred; it's the most Iris-adjacent v2 idea).

## 3. Locked decisions (from brainstorming)

| Decision | Choice |
|----------|--------|
| Core goal | Sample-as-synth (playable instrument voice) |
| Pitch engine | Granular (overlapping windowed grains; pitch independent of grain rate) |
| Runtime | Real-time plugin; `processBlock` is the pull interface |
| Note input | MIDI from host (Ableton) |
| Toolchain | **JUCE** → builds native arm64 **AU + VST3** (both load in Ableton/Mac) |
| Language split | **DSP core in portable C**; thin **C++ JUCE shell** for the plugin ABI |
| Voice model | **Per-note polyphony** — each held note = own voice (pitch, grain cloud, playhead, ADSR); voice pool + note-stealing |

## 4. Architecture

Two layers with a small C ABI seam:

```
┌─ C++ JUCE shell (talks to Ableton) ───────────────┐
│  PluginProcessor (juce::AudioProcessor):           │
│    - MIDI in  -> engine note on/off                │
│    - AudioProcessorValueTreeState params           │
│    - state/preset load+save (embeds sample bytes)  │
│    - file chooser (WAV)                            │
│    - processBlock() -> ge_render(...)              │
│  PluginEditor: generic parameter GUI (v1)          │
└───────────────┬────────────────────────────────────┘
                │  C ABI (granular_engine.h)
┌───────────────▼─── C DSP core (portable, no JUCE) ─┐
│  granular_engine.{h,c}  voice pool + param state    │
│  voice.c                per-note grain cloud + ADSR │
│  grain.c                one windowed, resampled grain│
│  window.c               Hann/Tukey window lookup     │
│  wav.c                  WAV decode -> float buffer   │
└─────────────────────────────────────────────────────┘
```

The C core has **zero JUCE dependency** — it could be linked into a CLI renderer or unit-test harness unchanged. The seam is a handful of C functions (see §7).

## 5. The sound (engine spec)

Port of `audio.ts::playGrain` + `animate`, made real-time-safe and polyphonic.

- **Voice:** fixed pool, **16 voices** (v1 ceiling). Note-on allocates a free/stolen voice; oldest-note stealing. Each voice holds: MIDI note, pitch ratio `2^((note-60)/12)`, position playhead (0..1 into source), its own grain scheduler phase, amp **ADSR** state.
- **Grain:** slice of the source buffer starting at `(position + spray) * duration`; **linear-interpolated resample** by voice pitch × global pitch; windowed by a **Hann** window (Tukey optional); trapezoidal fallback matches current browser behavior. Per-grain **position spray** and **pan spread**.
- **Scheduler:** per-voice grain onsets at `1/density` spacing, placed **sample-accurately within the block**.
- **Macro-Speed:** slow drift of the position playhead across the buffer (from `animate`), looping at buffer bounds.

### Parameters (all automatable)

| Param | Range (nominal) | Notes |
|-------|-----------------|-------|
| Density | 1–100 grains/s | grain onset rate |
| Grain Size | 5–500 ms | grain duration |
| Position | 0–1 | normalized playhead into source |
| Spread (spray) | 0–1 | position randomization per grain |
| Pan Spread | 0–1 | per-grain stereo spread |
| Pitch Coarse | ±24 semitones | global, on top of MIDI note |
| Pitch Fine | ±100 cents | |
| Macro-Speed | −1..+1 | position drift velocity |
| Amp A/D/S/R | standard | per-voice envelope |
| Master Gain | 0–1 | |

## 6. Real-time discipline (primary engineering risk)

The browser engine allocates a `BufferSource` per grain; the browser pools it. A plugin audio thread **must not allocate or lock**. Therefore:

- **Pre-allocated grain pool** and fixed voice array — no `malloc`/`free`, no locks in `ge_render`.
- Sample loading, param smoothing setup, and buffer (re)allocation happen on the **message thread**; the audio thread reads via a lock-free hand-off (double-buffer / atomic pointer swap) so a sample swap never tears mid-block.
- `ge_render` does arithmetic + buffer reads only.

## 7. C ABI (draft — `granular_engine.h`)

```c
typedef struct GranularEngine GranularEngine;

GranularEngine* ge_create(double sample_rate, int max_voices);
void ge_destroy(GranularEngine*);
void ge_prepare(GranularEngine*, double sample_rate, int max_block);

/* sample hand-off (message thread builds, audio thread swaps) */
void ge_set_sample(GranularEngine*, const float* interleaved,
                   int num_frames, int num_channels, double src_rate);

/* MIDI */
void ge_note_on (GranularEngine*, int note, float velocity);
void ge_note_off(GranularEngine*, int note);
void ge_all_notes_off(GranularEngine*);

/* params (enum id + normalized or real value) */
void ge_set_param(GranularEngine*, int param_id, float value);

/* the pull interface — fill N frames of stereo out */
void ge_render(GranularEngine*, float* left, float* right, int num_frames);
```

## 8. Sample loading & state

- v1: one WAV via JUCE `FileChooser`. Decoded to float on the message thread, handed to the core via `ge_set_sample`.
- Preset/state (`getStateInformation`) stores **all params + the sample bytes embedded**, so presets are self-contained and portable.

## 9. Repo & build

- Location: `~/Projects/kinetic-granular-native/` (own git repo → `origin` = github.com/closestfriend/kinetic-granular-native, never Memetica). Ported from the AI Studio app at `~/Projects/cf audio pack/kinetic-granular`.
- **CMake + JUCE as a git submodule** (source dependency; nothing to system-install). `clang` already present. Builds native arm64 AU + VST3.
- Layout:
  ```
  kinetic-granular-native/
    CMakeLists.txt
    JUCE/                (submodule)
    src/
      core/              granular_engine.h, *.c   (portable C)
      plugin/            PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
    docs/DESIGN.md
    tests/               (optional CLI harness linking core/)
  ```

## 10. Milestones

1. **Toolchain proof:** JUCE + CMake skeleton → empty plugin builds + loads in Ableton (AU & VST3).
2. **First sound:** C core with a single voice, single grain path → audible tone from a loaded WAV.
3. **Polyphony:** voice pool + note-stealing + per-voice ADSR.
4. **Params:** full parameter set wired through `AudioProcessorValueTreeState` → generic GUI.
5. **Sample + state:** file chooser load + preset save/load with embedded sample. → **v1 complete.**

## 11. Risks

- **RT-safety bugs** (allocation/locking on audio thread) — mitigated by pre-allocated pools + message-thread hand-off; the main thing to get right.
- **JUCE build friction** on first setup (CMake/JUCE versions, codesigning for AU validation) — front-loaded into Milestone 1 so it fails fast.
- **AU validation** (`auval`) can be picky — validated at Milestone 1, not at the end.

## 12. v2 backlog / deferred ideas

### Marquee: Sample-from-image (luminance → audio)
Port of the browser reference `~/Projects/cf audio pack/kinetic-granular/src/lib/audio.ts::loadSampleFromImage`. **Turns an image into the granular source** — the most Iris-adjacent idea in the lineage (Iris = image-as-sound), and it dovetails with the plugin's identity (esp. if named around film grain / light).

**Mechanic (as implemented in the browser app):**
1. Load image, downscale to a max dimension (~512px) so the buffer stays sane (512×512 ≈ 262k samples ≈ 5.9 s @ 44.1k).
2. Raster-scan every pixel (L→R, top→bottom); per pixel compute perceptual luminance `lum = (0.2126·R + 0.7152·G + 0.0722·B) / 255`.
3. Map to a mono sample: `sample = lum·2 − 1`. That stream of samples IS the source waveform.
4. Feed it to the existing `ge_set_sample()` and granulate normally — Position scans left-to-right across the picture; bright = loud, dark = ~silence.

**Native port effort: small (~1 hr).** JUCE decodes images via `ImageFileFormat::loadFrom` and gives pixel access (`Image::BitmapData`); it's a luminance loop → `float*` → `ge_set_sample` — the exact same engine path as WAV load. UI = a second "Load Image…" button beside "Load WAV…". No core/DSP changes.

**Honest caveat:** this is *time-domain* brightness→amplitude (crude), NOT Iris's *frequency-domain* spectrogram resynthesis. Raw, it's buzzy/DC-heavy with pitched artifacts tied to image width; the grain cloud (windowing, spray, drift) is what turns that into usable evolving texture. A true spectral image-resynth (FFT/iFFT) is a much bigger lift and remains out of scope.

### Other deferred items (from whole-branch + per-task reviews — all non-blocking)
- **Grain length uses source rate, not host rate** — grains run ~`src_rate/host_sr` off when they differ (e.g. a 44.1k sample in a 48k session → grains ~8% short). `voice_render` has `sr` in scope; pass it into `spawn_grain` and use it instead of `smp->sample_rate`.
- **No headroom/limiter at the summing stage** — 16 voices × up to 64 overlapping grains sum unbounded; extreme Density × Grain Size (e.g. 100 g/s × 500 ms) can hard-clip. Consider a gentle master-stage soft clip.
- **Linear (not equal-power) pan law** — per-grain pan is a linear crossfade (`lg+rg=2`), giving a ~+3 dB center bump across pan sweeps. Switch to equal-power if per-grain pan loudness feels off.
- **Per-block string-keyed param fetch** — `processBlock` does 13 `getRawParameterValue(id)` hash lookups every block; RT-safe but wasteful. Cache the 13 `std::atomic<float>*` pointers once.
- **Grain source-tail goes silent past buffer end** — once `read_pos` crosses `src_frames-1` (high pitch near the buffer end) `sample_at` returns 0 rather than clamping to the last sample; an abrupt silence rather than a graceful tail.
- **Multi-layer samples / custom waveform GUI / AIFF-FLAC** — larger v2+ scope items from the original non-goals list.
- **RT-safe live sample hot-swap (v2 hand-off)** — v1 hard-resets voices + serializes the swap under `getCallbackLock`; a proper lock-free double-buffer (atomic pointer swap on `e->sample` + grain-side buffer-generation tagging) would allow glitch-free swaps while sounding.
