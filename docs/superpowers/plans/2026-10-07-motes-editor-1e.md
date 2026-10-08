# Motes Editor 1e (paper-and-ink) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the dark 768×512 bioluminescent editor with the light 640×496 "1e" editor from the Claude Design handoff: 13 knobs, a live stereo phase scope of the real output, and a waveform strip showing real grains and playheads.

**Architecture:** Two small lock-free hooks in the C core (a single-producer/single-consumer grain-event ring and per-voice playhead atomics) plus a preallocated `AbstractFifo` for output samples in the processor. One 60 Hz editor timer drains them. Controls are custom components bound with `juce::ParameterAttachment`; everything is drawn in code with two embedded OFL fonts.

**Tech Stack:** C11 core (`<stdatomic.h>`), C++17 + JUCE 7.0.12, CMake + Unix Makefiles, `auval`, Barlow Condensed + IBM Plex Mono (SIL OFL).

**Spec:** `design_handoff_motes_editor/README.md` (wins) + `Motes Editor.dc.html` option **1e** (behaviour), measured geometry in `docs/design-reference/1e-layout.json`, target image `docs/design-reference/1e.png`. **Supersedes** `docs/superpowers/specs/2026-07-09-motes-ui-design.md` and `docs/superpowers/plans/2026-07-09-motes-ui.md` (do not edit those; Hunter updates them in the main folder).

## Global Constraints

- **Audio thread lock-free:** no locks, no allocation, no blocking in `processBlock` / `ge_render`'s call chain. Every new FIFO/ring is preallocated SPSC and **drops on overflow**. (Pre-existing, out of scope: JUCE's VST3/AU wrappers take `getCallbackLock()` around `processBlock`, and `loadWavFile` holds it during the sample swap. Verified in `JUCE/modules/juce_audio_plugin_client/*VST3.cpp:3685`, `*AU_1.mm:734`.)
- **C core stays JUCE-free**, portable C11, C ABI. `granular_engine.h` is included from C++ inside `extern "C"`, and C++17 has no `<stdatomic.h>`, so atomics live only in `.c`/private headers, never in `granular_engine.h`.
- **Every new `ge_*` hook has a test in `tests/test_core.c`.** `test_core` keeps `-UNDEBUG`.
- **HOLD never calls the engine from the message thread**: editor sets an atomic on the processor; `processBlock` acts on it.
- **Window:** fixed 640×496, not resizable. Square corners (the mock's 10 px radius and shadow belong to the host window).
- **Accent colour is one constant** (`theme::kAccent`, default = ink).
- **No code path depends on an asset existing:** font load failure falls back to system sans/monospace.
- **Fonts:** Barlow Condensed Medium(500)+SemiBold(600), IBM Plex Mono Regular(400) from `google/fonts` (`ofl/barlowcondensed`, `ofl/ibmplexmono`), embedded via `juce_add_binary_data`, OFL texts committed beside them. Plex Mono 500 is not used by 1e; do not embed it.
- **Build:** `cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build`. ` m JUCE` is the intentional patch and is never committed.
- **Builds auto-install** to `~/Library/Audio/Plug-Ins/{Components,VST3}/` and **overwrite the copy built from the main `motes/` folder.** Say so whenever Hunter will load a build in Ableton.
- **Git:** branch `feat/ui-makeover` only. Plain conventional-commit messages, **no Co-Authored-By, no "Generated with"**. Stage files **by name**; never `git add .`/`-A`. `CLAUDE.md` and `design_handoff_motes_editor/` stay untracked. No `Memetica-Corporation` remote, ever.
- **Do not touch** `JUCE_DISPLAY_SPLASH_SCREEN`; do not design around the splash.

## Decisions pending (defaults assumed below; see "Open questions" in the hand-off message)

| # | Question | Default used in this plan |
|---|---|---|
| Q1 | True-log mapping for Size/Attack/Decay/Release (changes host-automation curve; saved *values* unchanged) | **Yes**, true exponential (not JUCE's power-law skew; the mock is `min·(max/min)^n`) |
| Q2 | Pitch semitone snap as parameter `interval=1` (host sees integers) vs UI-only snap | **Parameter interval 1** |
| Q3 | Engine clamps `position+drift` at 1.0 but the mock wraps; the real playhead will visibly stick at the right edge | **Task 5 shows the truth and does not change DSP**; wrap is a separate opt-in task 5b |
| Q4 | Opt-in `motes_harness` console target (snapshot PNG + audio/GUI-seam assertions) and tiny `test_format` target | **Needs approval before Task 1**; if declined, Tasks 1/2/4/6 fall back to "Hunter loads it in Ableton" for visuals and manual checks for the seam |

## Review Focus

Failure modes the spec implies but a happy-path build won't exercise (each has a test in the owning task):

1. **Degenerate samples** (0 frames, < 900 frames, all-zero, NaN/Inf-bearing float WAV) → peaks finite, no divide-by-zero in grain-event `span`, strip draws nothing odd (Task 3, Task 5).
2. **Retina/HiDPI:** offscreen caches must be allocated at physical-pixel size and rebuilt on scale change; 1 px hairlines must land on whole physical pixels (Tasks 1, 3, 4).
3. **HOLD survival:** sample load, host all-notes-off, voice stealing (17th note), and a MIDI C4 while HOLD is on must neither drop the held note nor stack duplicate C4 voices (Task 6).
4. **Scope FIFO:** stalled GUI (FIFO full → drop, then resume without a spike), editor closed (no consumer), non-finite output samples never reaching `Path` (Task 4).
5. **Editor lifetime / state restore:** reopening the editor must not replay stale grain events; `setStateInformation` while the editor is open must refresh name, meta and waveform (Task 6).

---

## File Structure

```
assets/fonts/BarlowCondensed-Medium.ttf, BarlowCondensed-SemiBold.ttf, IBMPlexMono-Regular.ttf   CREATE
assets/fonts/OFL-BarlowCondensed.txt, OFL-IBMPlexMono.txt, README.md (source URLs + SHA-256)       CREATE
src/core/grain_event.h        CREATE  plain struct GrainEvent {pos, span} (C++-safe, no atomics)
src/core/event_ring.h         CREATE  private C-only SPSC ring (stdatomic)
src/core/wav.h/.c             MODIFY  + wav_peaks()
src/core/voice.h/.c           MODIFY  Voice.events ptr, spawn pushes event, voice_playhead()
src/core/granular_engine.h/.c MODIFY  + ge_pop_grain_event, ge_get_playheads, ge_voice_playing_note
tests/test_core.c             MODIFY  + peaks, ring, events, playheads, playing_note tests
src/plugin/MotesTheme.h       CREATE  colours + band rectangles (measured)
src/plugin/ParamFormat.h/.cpp CREATE  JUCE-free value formatters + sample meta (std::string)
src/plugin/MotesParams.h      CREATE  knob table (id, label, compact, bipolar)
src/plugin/MotesPresets.h     CREATE  4 factory presets
src/plugin/MotesLookAndFeel.* MODIFY  fonts + (later) nothing else; knob drawing lives in MotesKnob
src/plugin/MotesKnob.h/.cpp   CREATE  dial + value + label, ParameterAttachment, drag/shift/dbl-click
src/plugin/SourceStrip.h/.cpp CREATE  waveform cache, spread band, playhead lines, grain flashes, drag=Position
src/plugin/ScopeView.h/.cpp   CREATE  goniometer with persistent offscreen image
src/plugin/HeaderBar.h/.cpp   CREATE  wordmark, ‹ preset ›, HOLD pill
src/plugin/FooterBar.h/.cpp   CREATE  file name, Load… link, meta
src/plugin/PluginProcessor.*  MODIFY  ranges/formatters, scope FIFO, HOLD, peaks+SampleInfo, presets, event/playhead pass-through
src/plugin/PluginEditor.*     MODIFY  rewritten: composes the above, 60 Hz timer, file drop
src/plugin/MotesField.*       DELETE  (Task 7)
tools/harness/main.cpp        CREATE  (Q4) snapshot + seam assertions
tests/test_format.cpp         CREATE  (Q4) formatter tests
CMakeLists.txt                MODIFY  binary data, sources, options
```

Measured band rectangles (border-box, from `1e-layout.json`; the handoff's 44/136/40/118/~118/38 are *content* heights without the 1 px hairlines):

| Band | y | h | hairline |
|---|---|---|---|
| Header | 0 | 45 | y=44 |
| Scope | 45 | 137 | y=181 |
| Strip | 182 | 41 | y=222 |
| Primary knobs | 223 | 119 | y=341 |
| Secondary knobs | 342 | 115 | none |
| Footer | 457 | 39 | y=457 (top) |

Knob x (left edge of the dial): primary 34,138,242,346,450,554 @ y=236.5, 52 px; secondary 36,124.4,212.7 | 309.7,394.5,479.2,564 @ y=362, 40 px; the 1 px divider sits between the clusters (inset 18 px top/bottom; take its x from the DOM dump in `1e-layout.json`, the `bands[4]` children).

---

## Task 0 — Reference + plan (done before this plan is approved)

- [x] `docs/design-reference/1e.png` (640×496, captured from the HTML at native size) and `1e-layout.json` (DOM rects of every band, dial and text run).
- [ ] Commit (held until Hunter approves the plan): `git add docs/design-reference/1e.png docs/design-reference/1e-layout.json docs/superpowers/plans/2026-10-07-motes-editor-1e.md && git commit -m "docs: add 1e reference capture and editor makeover plan"`

---

## Milestone 1 — layout + LookAndFeel + fonts

### Task 1: Fonts, theme, static editor chrome (+ harness if Q4 approved)

**Files:** Create `assets/fonts/*`, `src/plugin/MotesTheme.h`, `src/plugin/HeaderBar.*`, `src/plugin/FooterBar.*`, (Q4) `tools/harness/main.cpp`; Modify `src/plugin/MotesLookAndFeel.*`, `src/plugin/PluginEditor.*`, `CMakeLists.txt`.

**Interfaces:**
- Produces `namespace theme { kPaper,kInk,kInk2,kHairline,kTrack,kWave,kWaveActive,kAccent; constexpr ints kW,kH, and <band>Y/<band>H }`.
- Produces `MotesLookAndFeel::barlow(float px, bool semibold) const` and `::mono(float px) const` returning `juce::Font` sized so **px = CSS em size** (uses `withPointHeight`), with system fallback.
- Produces `HeaderBar` (callbacks `onPrev, onNext, onHold(bool)`; `setPresetName(String)`, `setHold(bool)`), `FooterBar` (`onLoad`; `setFile(String name)`, `setMeta(String)`).

- [ ] **Step 1: Fetch fonts into a fresh directory, verify, then copy in.**

```bash
D=$(mktemp -d) && cd "$D"
B=https://raw.githubusercontent.com/google/fonts/main/ofl
curl -fsSLO $B/barlowcondensed/BarlowCondensed-Medium.ttf -O $B/barlowcondensed/BarlowCondensed-SemiBold.ttf -O $B/barlowcondensed/OFL.txt
mv OFL.txt OFL-BarlowCondensed.txt
curl -fsSLO $B/ibmplexmono/IBMPlexMono-Regular.ttf && curl -fsSL $B/ibmplexmono/OFL.txt -o OFL-IBMPlexMono.txt
file *.ttf; shasum -a 256 *
```
Expected: three `TrueType Font data`. Copy into `<worktree>/assets/fonts/`, write `assets/fonts/README.md` listing each URL + SHA-256 + "SIL OFL 1.1, unmodified".

- [ ] **Step 2: CMake.** Add after `add_library(granular_core ...)`:

```cmake
juce_add_binary_data(MotesFonts NAMESPACE MotesFonts SOURCES
    assets/fonts/BarlowCondensed-Medium.ttf
    assets/fonts/BarlowCondensed-SemiBold.ttf
    assets/fonts/IBMPlexMono-Regular.ttf)
```
and add `MotesFonts` to `target_link_libraries(Motes PRIVATE ...)`. Add the new `.cpp` files to `target_sources`. Keep `MotesField.cpp` until Task 7.

- [ ] **Step 3: `MotesTheme.h`.**

```cpp
#pragma once
#include <juce_graphics/juce_graphics.h>
namespace theme {
inline const juce::Colour kPaper{0xFFF4F2ED}, kInk{0xFF1A1A18}, kInk2{0xFF605D57},
    kHairline{0xFFD8D4CB}, kTrack{0xFFDDD9D0}, kWave{0xFFCFCAC0}, kWaveActive{0xFF8E8A82};
inline const juce::Colour kAccent = kInk;          // the ONE accent constant (alt tried: 0xFFD9472B)
constexpr int kW = 640, kH = 496;
constexpr int headerY = 0,   headerH = 45;
constexpr int scopeY  = 45,  scopeH  = 137;
constexpr int stripY  = 182, stripH  = 41;
constexpr int primY   = 223, primH   = 119;
constexpr int secY    = 342, secH    = 115;
constexpr int footY   = 457, footH   = 39;
}
```

- [ ] **Step 4: LookAndFeel fonts with fallback.** In `MotesLookAndFeel` hold three `juce::Typeface::Ptr` created in the constructor from `MotesFonts::BarlowCondensedMedium_ttf` etc. (`Typeface::createSystemTypefaceFor(data,size)`; may return null).

```cpp
juce::Font MotesLookAndFeel::barlow(float px, bool semi) const {
    auto tf = semi ? barlowSemi : barlowMed;
    juce::Font f = tf ? juce::Font(tf) : juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.f, juce::Font::bold);
    return f.withPointHeight(px);
}
juce::Font MotesLookAndFeel::mono(float px) const {
    juce::Font f = plexMono ? juce::Font(plexMono)
                            : juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::plain);
    return f.withPointHeight(px);
}
```
Remove the old dark colours and the old `drawRotarySlider` (knob drawing moves to `MotesKnob` in Task 2). Tracking is applied at the call site with `font.setExtraKerningFactor(0.10f)` (labels), `0.06f` (wordmark), `0.14f` (HOLD).

- [ ] **Step 5: `HeaderBar` / `FooterBar` / editor skeleton.** Editor: `setSize(theme::kW, theme::kH); setResizable(false,false);` `paint` fills paper and draws the six hairlines from the table above with `g.fillRect(0, y, kW, 1)` in `kHairline`. Header paints "MOTES" (Barlow 20 semibold, tracking .06) at x=20, vertically centred in 0..44; preset text `‹ Init ›` (mono 11, arrows `kInk2`, 8 px gaps) and the HOLD pill (Barlow 11 medium upper, tracking .14, padding 5×10, 1 px ink border, fully rounded; on = ink fill + paper text). Footer: file name (ink) at x=20, "Load…" underlined (`g.drawLine` 3 px below baseline) 12 px after it, right-aligned meta (`kInk2`) ending at x=620; all mono 11. Placeholders: file "built-in tone", meta "1.0 s · 44.1 kHz · mono". Scope/strip/knob bands are empty paper for now.

- [ ] **Step 6: (Q4) Harness.** Add to `CMakeLists.txt`:

```cmake
option(MOTES_BUILD_HARNESS "Headless snapshot + seam-assertion tool (dev only)" OFF)
if(MOTES_BUILD_HARNESS)
    juce_add_console_app(motes_harness PRODUCT_NAME "motes_harness")
    target_sources(motes_harness PRIVATE tools/harness/main.cpp ${MOTES_PLUGIN_SOURCES})
    target_link_libraries(motes_harness PRIVATE granular_core MotesFonts juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(motes_harness PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()
```
(refactor the plugin's source list into `set(MOTES_PLUGIN_SOURCES ...)` used by both targets; the harness is built only with `-DMOTES_BUILD_HARNESS=ON`, never by default or by the plugin build). `main.cpp`: `juce::ScopedJuceInitialiser_GUI init;` construct `MotesProcessor`, `auto* ed = proc.createEditor();` `ed->setBounds(0,0,640,496)`, pump `juce::MessageManager::getInstance()->runDispatchLoopUntil(300)`, then `ed->createComponentSnapshot(ed->getLocalBounds(), true, scale)` → `juce::PNGImageFormat` → `argv[1]`. Run with scale 1 and 2. **Flag in the report:** this is a *proxy* (JUCE software renderer, no host, no real mouse/DnD); anti-aliasing differs from CoreGraphics in-host, so pixel diffs are indicative, not exact, and it can't verify host drag-and-drop or Ableton behaviour.

- [ ] **Step 7: Build, verify, compare.**

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DMOTES_BUILD_HARNESS=ON && cmake --build build -j8
cmake --build build --target test_core && ./build/test_core      # expect: ALL CORE TESTS PASSED
auval -v aumu Mote Hntr                                          # expect: AU VALIDATION SUCCEEDED
./build/motes_harness_artefacts/Release/motes_harness docs/design-reference/out/m1.png   # path per JUCE layout; find with `find build -name motes_harness -type f`
python3 -I - <<'EOF'
from PIL import Image, ImageChops
a=Image.open('docs/design-reference/1e.png').convert('RGB'); b=Image.open('docs/design-reference/out/m1.png').convert('RGB')
Image.blend(a,b,0.5).save('docs/design-reference/out/m1-blend.png'); ImageChops.difference(a,b).save('docs/design-reference/out/m1-diff.png')
EOF
```
Look at `m1.png` next to `1e.png`: hairlines on the same rows, header/footer text positions within ~1 px, no tofu glyphs for `‹ › … −`. `docs/design-reference/out/` is git-ignored scratch (add the line to `.gitignore`).

- [ ] **Step 8: Commit.**

```bash
git add assets/fonts CMakeLists.txt .gitignore src/plugin/MotesTheme.h src/plugin/HeaderBar.h src/plugin/HeaderBar.cpp src/plugin/FooterBar.h src/plugin/FooterBar.cpp src/plugin/MotesLookAndFeel.h src/plugin/MotesLookAndFeel.cpp src/plugin/PluginEditor.h src/plugin/PluginEditor.cpp tools/harness/main.cpp
git commit -m "feat(ui): 1e chrome — embedded OFL fonts, paper/ink theme, fixed 640x496 layout"
```
(Between milestones the editor is non-interactive; all 13 params remain reachable via the host's generic parameter list, and audio is unaffected.)

---

## Milestone 2 — knobs

### Task 2: Parameter metadata, formatters, log ranges, `MotesKnob`

**Files:** Create `src/plugin/ParamFormat.h/.cpp`, `src/plugin/MotesParams.h`, `src/plugin/MotesKnob.h/.cpp`, (Q4) `tests/test_format.cpp`; Modify `src/plugin/PluginProcessor.cpp` (`makeLayout`), `PluginEditor.*`, `CMakeLists.txt`.

**Interfaces:**
- Produces `std::string fmt::formatParam(std::string_view id, float v)` (UTF-8, Unicode minus U+2212), `std::string fmt::formatSampleMeta(double seconds, double rate, int channels)`.
- Produces `struct KnobSpec { const char* id; const char* label; bool bipolar; }` and `kPrimary[6]`, `kPanGroup[3]` (panspread, macro, pitchcent), `kEnvGroup[4]`.
- Produces `MotesKnob(juce::RangedAudioParameter&, const KnobSpec&, bool compact)`; slot bounds = 64×? primary / 56×? secondary, `hitTest` limited to the dial square.

- [ ] **Step 1: Formatter tests first (Q4: `tests/test_format.cpp`; otherwise run the same asserts inside the harness).**

```cpp
#include "ParamFormat.h"
#include <cassert>
#include <cstdio>
using fmt::formatParam;
int main() {
    assert(formatParam("density", 42) == "42 g/s");
    assert(formatParam("grain", 180) == "180 ms");
    assert(formatParam("spread", .22f) == "22%");
    assert(formatParam("pitchsemi", 12) == "+12 st");
    assert(formatParam("pitchsemi", -12) == "−12 st");
    assert(formatParam("pitchsemi", 0) == "±0 st");
    assert(formatParam("pitchcent", -7) == "−7 ct");
    assert(formatParam("macro", .12f) == "+0.12");
    assert(formatParam("macro", 0) == "±0.00");
    assert(formatParam("master", .8f) == "−1.9 dB");
    assert(formatParam("master", 0) == "−∞ dB");
    assert(formatParam("master", 1.f) == "0.0 dB");
    assert(formatParam("master", .9995f) == "0.0 dB");      // would print "-0.0" naively
    assert(formatParam("attack", .35f) == "350 ms");
    assert(formatParam("release", 1.8f) == "1.80 s");
    assert(formatParam("decay", .9996f) == "1.00 s");       // would print "1000 ms" naively
    assert(fmt::formatSampleMeta(6.2, 48000, 2) == "6.2 s · 48 kHz · stereo");
    assert(fmt::formatSampleMeta(1.0, 44100, 1) == "1.0 s · 44.1 kHz · mono");
    assert(fmt::formatSampleMeta(3.0, 48000, 6) == "3.0 s · 48 kHz · 6 ch");
    std::puts("test_format OK");
}
```
Run: expect FAIL (no implementation).

- [ ] **Step 2: Implement `ParamFormat.cpp`.** Rounding must match JS `Math.round` (half toward +∞), so use `std::floor(x + 0.5)`, not `std::round`.

```cpp
namespace fmt {
static long jsRound(double x) { return (long) std::floor(x + 0.5); }
static std::string sgn(double v, double eps) { return v > eps ? "+" : v < -eps ? "−" : "±"; }
static std::string num(double v, int dp) { char b[32]; std::snprintf(b, sizeof b, "%.*f", dp, v); return b; }
std::string formatParam(std::string_view id, float v) {
    if (id == "density") return num(v, 0) + " g/s";
    if (id == "grain")   return num(v, 0) + " ms";
    if (id == "position" || id == "spread" || id == "panspread" || id == "sustain")
        return std::to_string(jsRound(v * 100.0)) + "%";
    if (id == "pitchsemi" || id == "pitchcent") {
        long r = jsRound(v);  return sgn((double) r, 0) + std::to_string(std::labs(r)) + (id == "pitchsemi" ? " st" : " ct");
    }
    if (id == "macro") return sgn(v, .005) + num(std::fabs(v), 2);
    if (id == "attack" || id == "decay" || id == "release") {
        long ms = jsRound(v * 1000.0);  return ms < 1000 ? std::to_string(ms) + " ms" : num(v < 0.9995f ? 1.0 : v, 2) + " s";
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
}
```
(`ms >= 1000` branch: `.9996` → `num(1.0,2)` = "1.00 s".) `formatSampleMeta`: `num(seconds,1)`, rate as `num(rate/1000,1)` with a trailing `.0` stripped, `1→mono, 2→stereo, else "N ch"`, joined with ` · ` (U+00B7). Re-run tests: expect `test_format OK`.

- [ ] **Step 3: `makeLayout()` (Q1, Q2).** Add a log range helper and string-from-value attributes so hosts also show `42 g/s`:

```cpp
static juce::NormalisableRange<float> logRng(float a, float b) {
    return { a, b,
        [](float s, float e, float n)  { return s * std::pow(e / s, n); },
        [](float s, float e, float v)  { return std::log(v / s) / std::log(e / s); },
        [](float s, float e, float v)  { return juce::jlimit(s, e, v); } };
}
static juce::AudioParameterFloatAttributes A(const char* id) {
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [id](float v, int) { return juce::String::fromUTF8(fmt::formatParam(id, v).c_str()); });
}
```
Change `grain`, `attack`, `decay`, `release` to `logRng(...)` with unchanged min/max/default, `pitchsemi` to `juce::NormalisableRange<float>(-24, 24, 1.f)`, and pass `A("<id>")` to every parameter. Param IDs, ranges' ends and defaults do not change (all 13 IDs verified identical to the handoff table). If Q1/Q2 are declined, skip those two range edits only.

- [ ] **Step 4: `MotesKnob`.** Attach with JUCE's `ParameterAttachment` (reuse; it owns gestures/automation). Verify its callback/setters take **denormalised** values: `grep -n "setValueAsPartOfGesture\|ParameterAttachment (" JUCE/modules/juce_audio_processors/utilities/juce_ParameterAttachments.h`. Why not `juce::Slider`: its shift handling only affects the two-value min/max sliders (`juce_Slider.cpp:941,951`), so shift-fine and the handoff's `dx − dy` drag need our own mouse math; everything else stays JUCE.

```cpp
void MotesKnob::mouseDown(const juce::MouseEvent& e) {
    acc = param.getValue();  last = e.position;  attach.beginGesture();   // acc is unclamped normalised
}
void MotesKnob::mouseDrag(const juce::MouseEvent& e) {
    auto d = e.position - last;  last = e.position;
    acc += ((d.x - d.y) / 200.0f) * (e.mods.isShiftDown() ? 0.2f : 1.0f);   // incremental => no jump if shift toggles mid-drag
    attach.setValueAsPartOfGesture(param.convertFrom0to1(juce::jlimit(0.f, 1.f, acc)));
}
void MotesKnob::mouseUp(const juce::MouseEvent&)       { attach.endGesture(); }
void MotesKnob::mouseDoubleClick(const juce::MouseEvent&) {
    attach.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
}
bool MotesKnob::hitTest(int x, int y) { return dialBounds.contains(x, y); }
```
Note the deviation from the mock: it is start-relative (`n0 + d`), which jumps when shift toggles mid-drag; ours accumulates unclamped so dragging back past the limit still tracks the cursor but never jumps.

Dial paint (all geometry in a 100-unit box, then scaled, so **strokes scale too**: 52 px → 2.6 px track, 40 px → 2.4 px):

```cpp
void MotesKnob::paintDial(juce::Graphics& g, juce::Rectangle<float> r) {
    const float s = r.getWidth() / 100.f, n = juce::jlimit(0.f, 1.f, param.convertTo0to1(value));
    const float a = juce::degreesToRadians(-135.f + 270.f * n), a0 = bipolar ? 0.f : juce::degreesToRadians(-135.f);
    const float track = compact ? 6.f : 5.f, body = compact ? 2.5f : 2.f, ptr = compact ? 4.f : 3.f;
    auto cap = [&](float w) { return juce::PathStrokeType(w * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    const auto c = r.getCentre();  const auto L = juce::degreesToRadians(135.f);
    juce::Path p;  p.addCentredArc(c.x, c.y, 42*s, 42*s, 0, -L, L, true);
    g.setColour(theme::kTrack); g.strokePath(p, cap(track));
    if (std::abs(a - a0) > juce::degreesToRadians(0.5f)) {            // mock skips < 0.5° (a round cap would draw a dot)
        juce::Path v;  v.addCentredArc(c.x, c.y, 42*s, 42*s, 0, juce::jmin(a0, a), juce::jmax(a0, a), true);
        g.setColour(theme::kInk); g.strokePath(v, cap(track));
    }
    g.setColour(theme::kInk); g.drawEllipse(c.x - 30*s, c.y - 30*s, 60*s, 60*s, body * s);
    g.drawLine(c.x + 12*s*std::sin(a), c.y - 12*s*std::cos(a), c.x + 30*s*std::sin(a), c.y - 30*s*std::cos(a), ptr * s);
}
```
(`drawLine` with thickness gives butt caps; if the snapshot shows the pointer end differs from the round cap in the reference, stroke it as a `Path` with a rounded `PathStrokeType`.) Below the dial: value (`mono` 11/10, ink) then label (`barlow` 12/11 medium, tracking .10, upper, `kInk2`), gaps 8/7 px; repaint on the attachment callback only.

- [ ] **Step 5: Editor layout.** Primary: six 64 px-wide slots, `space-between` inside x∈[28,612], dial top at y=236.5 (use the measured x's). Secondary: slots 56 px; left cluster Pan/Drift/Fine, right cluster Attack/Decay/Sustain/Release, 1 px `kHairline` divider inset 18 px top/bottom (positions from `1e-layout.json`). Primary order: density, grain, spread, position, pitchsemi, master (labels DENSITY SIZE SPREAD POSITION PITCH MASTER); secondary labels PAN DRIFT FINE ATTACK DECAY SUSTAIN RELEASE.

- [ ] **Step 6: Verify + commit.** Build (as Task 1 step 7), `test_format`, `test_core`, `auval`; harness snapshot vs `1e.png` (knob positions, arc fill at the Isolation-Drift values once Task 6 lands; for now defaults). Manual in Ableton (say "this overwrites your main-folder install"): drag, shift-drag (5× finer), double-click resets, Pitch steps whole semitones, host automation lane for Attack follows the log curve.

```bash
git add CMakeLists.txt tests/test_format.cpp src/plugin/ParamFormat.h src/plugin/ParamFormat.cpp src/plugin/MotesParams.h src/plugin/MotesKnob.h src/plugin/MotesKnob.cpp src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.h src/plugin/PluginEditor.cpp
git commit -m "feat(ui): 13 custom knobs — formats, shift-fine drag, double-click reset, log ranges"
```

---

## Milestone 3 — source strip + peak array

### Task 3: `wav_peaks`, sample info, `SourceStrip` waveform + Position drag

**Files:** Modify `src/core/wav.h/.c`, `tests/test_core.c`, `src/plugin/PluginProcessor.h/.cpp`, `PluginEditor.*`; Create `src/plugin/SourceStrip.h/.cpp`.

**Interfaces:**
- Produces `void wav_peaks(const float* interleaved, int frames, int channels, float* out, int bins)`; every bin finite in [0,1], max bin = 1 unless the data is silent.
- Produces `struct MotesProcessor::SampleInfo { juce::String name; double seconds, sampleRate; int channels; std::array<float,900> peaks; }`, `SampleInfo getSampleInfo() const` (message thread; copies under a `CriticalSection` the audio thread never takes), `std::atomic<int> sampleVersion`.
- Produces `SourceStrip::setPeaks(const std::array<float,900>&)`, `setSpread(float)`, paints at bounds 640×41.

- [ ] **Step 1: Failing tests in `tests/test_core.c`.**

```c
static void test_wav_peaks(void){
    float pk[900];
    static float st[2000]; for(int i=0;i<2000;i++) st[i]=0.01f; st[2*500+1]=0.8f;   /* spike on R at frame 500 */
    wav_peaks(st,1000,2,pk,900);
    assert(pk[450]==1.0f);                                  /* bin = frame*900/1000 */
    for(int i=0;i<900;i++) assert(pk[i]>=0.f && pk[i]<=1.f);
    float tiny[10]={0,.5f,0,0,0,0,0,0,0,0}; wav_peaks(tiny,10,1,pk,900);   /* frames < bins: every bin populated */
    int nz=0; for(int i=0;i<900;i++) if(pk[i]>0) nz++; assert(nz>=90);
    float z[64]={0}; wav_peaks(z,64,1,pk,900); for(int i=0;i<900;i++) assert(pk[i]==0.f);   /* silent => zeros, not NaN */
    float bad[100]; for(int i=0;i<100;i++) bad[i]=0.25f; bad[10]=NAN; bad[20]=INFINITY;
    wav_peaks(bad,100,1,pk,900); for(int i=0;i<900;i++) assert(isfinite(pk[i]));
    wav_peaks(NULL,0,0,pk,900); for(int i=0;i<900;i++) assert(pk[i]==0.f);  /* empty input */
    printf("test_wav_peaks OK\n");
}
```
Register in `main`. Build `test_core`: expect compile/link FAIL (`wav_peaks` undeclared). (The mock samples every 4th frame and leaves bins past the data empty for short files; both are fixed here.)

- [ ] **Step 2: Implement** in `wav.c` and declare in `wav.h`:

```c
void wav_peaks(const float* d, int frames, int ch, float* out, int bins){
    for(int i=0;i<bins;i++) out[i]=0.f;
    if(!d||frames<=0||ch<=0||bins<=0) return;
    float top=0.f;
    for(int i=0;i<bins;i++){
        long a=(long)i*frames/bins, b=(long)(i+1)*frames/bins;
        if(b<=a) b=a+1;  if(b>frames) b=frames;
        float m=0.f;
        for(long f=a;f<b;f++) for(int c=0;c<ch;c++){ float v=fabsf(d[f*ch+c]); if(v>m && v<1e30f) m=v; }  /* NaN/Inf ignored */
        out[i]=m; if(m>top) top=m;
    }
    if(top>0.f) for(int i=0;i<bins;i++) out[i]/=top;
}
```
Run `cmake --build build --target test_core && ./build/test_core`: expect `ALL CORE TESTS PASSED`.

- [ ] **Step 3: Processor.** In the constructor (after seeding the tone) and at the end of `loadWavFile` (**outside** the callback lock), compute peaks from the interleaved buffer, then `{ ScopedLock sl(infoLock); info = {...}; } sampleVersion.fetch_add(1)`. `loadWavFile(const File&, const String& displayName = {})` (empty → `f.getFileName()`); the state-restore path passes the name stored in `apvts.state["sampleName"]` (written by the Load/drop path via `apvts.state.setProperty("sampleName", name, nullptr)` on the **message thread only**; `setStateInformation` must not write the ValueTree). Built-in tone: name "built-in tone", 1.0 s, 44100, mono. Guard: a decode that yields 0 frames returns `false` (do not call `ge_set_sample` with 0 frames).

- [ ] **Step 4: `SourceStrip`.** Cache the waveform as an `Image` at **physical** resolution; rebuild only on peaks change or scale change:

```cpp
void SourceStrip::paint(juce::Graphics& g) {
    const float sc = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!cache.isValid() || sc != cacheScale) rebuildCache(sc);
    g.drawImage(cache, getLocalBounds().toFloat());
    // spread band, active-bar overlay, playhead line(s), flashes: Tasks 3b/5
}
void SourceStrip::rebuildCache(float sc) {
    cacheScale = sc;  cache = juce::Image(juce::Image::ARGB, juce::roundToInt(getWidth()*sc), juce::roundToInt(getHeight()*sc), true);
    juce::Graphics c(cache);  c.addTransform(juce::AffineTransform::scale(sc));
    const float w = (float) getWidth(), h = (float) getHeight(), by = h * .5f, am = h * .38f;
    c.setColour(theme::kWave);
    for (float x = 0; x < w; x += 2) { float a = peaks[(size_t) juce::jmin(899, (int) (x / w * 900))] * am; c.fillRect(x, by - a, 1.f, a * 2 + .5f); }
}
```
Position line: **1 px, accent, height = 0.88·h (`by+am`), not full height** — the handoff text says "full height" but the mock's strip draws `by+am` (lineTop 0), and `1e.png` shows it stopping short of the bottom hairline; match the pixels. Spread band: accent @ 6%, `x∈[px−spread·w/2, px+spread·w/2]` (min width 2), y∈[0, 0.88h]; redraw bars inside the band in `kWaveActive` (every 2 px, same heights). For now `px` = param position. Drag: `mouseDown/Drag` → `ParameterAttachment` on `position` with `x/width` clamped 0..1 (begin/end gesture); `setMouseCursor(juce::MouseCursor::LeftRightResizeCursor)`. The mock's "drag resets visual drift to 0" has no engine analogue (drift is per-voice audio state) and is dropped.

- [ ] **Step 5: Editor timer (60 Hz)** starts here: `startTimerHz(60)`; `timerCallback` checks `proc.sampleVersion` and pushes `getSampleInfo()` into strip + footer (`name`, `fmt::formatSampleMeta`).

- [ ] **Step 6: Verify + commit.** Build, `test_core`, `auval`, harness snapshot @1x and @2x vs `1e.png` (bars 1 px crisp @2x; band/line geometry). Commit:

```bash
git add src/core/wav.h src/core/wav.c tests/test_core.c src/plugin/PluginProcessor.h src/plugin/PluginProcessor.cpp src/plugin/SourceStrip.h src/plugin/SourceStrip.cpp src/plugin/PluginEditor.h src/plugin/PluginEditor.cpp CMakeLists.txt
git commit -m "feat(ui): source strip with cached peak waveform, spread band, Position drag"
```

---

## Milestone 4 — output FIFO + phase scope

### Task 4: Lock-free scope FIFO and `ScopeView`

**Files:** Modify `src/plugin/PluginProcessor.h/.cpp`, `PluginEditor.*`, `CMakeLists.txt`; Create `src/plugin/ScopeView.h/.cpp`.

**Interfaces:**
- Produces `int MotesProcessor::pullScope(float* l, float* r, int maxFrames)` (message thread; returns frames read; **newest** `maxFrames` if more are queued, older ones discarded).
- Produces `ScopeView::addBlock(const float* l, const float* r, int n)` (draws one frame into the persistent image; call once per timer tick).

- [ ] **Step 1: Processor FIFO (preallocated, SPSC, drop on overflow).**

```cpp
static constexpr int kScopeFifo = 4096;
juce::AbstractFifo scopeFifo { kScopeFifo };
std::array<float, kScopeFifo> scopeL {}, scopeR {};
```
In `processBlock`, **after** `ge_render` (R aliases L for mono buses):

```cpp
const int n = buffer.getNumSamples();
if (n <= scopeFifo.getFreeSpace()) {            // drop whole block if the GUI is stalled: keeps blocks contiguous
    int s1, n1, s2, n2;  scopeFifo.prepareToWrite(n, s1, n1, s2, n2);
    std::copy(L, L + n1, scopeL.begin() + s1);  std::copy(R, R + n1, scopeR.begin() + s1);
    if (n2) { std::copy(L + n1, L + n, scopeL.begin() + s2);  std::copy(R + n1, R + n, scopeR.begin() + s2); }
    scopeFifo.finishedWrite(n1 + n2);
}
```
`pullScope`: `ready = scopeFifo.getNumReady(); skip = max(0, ready - maxFrames); scopeFifo.prepareToRead(ready, ...)` copy the last `maxFrames`, `finishedRead(ready)`. A 4096-frame FIFO holds ~85 ms at 48 kHz; plot cap `maxFrames = 1600`.

- [ ] **Step 2: `ScopeView`.** Offscreen `Image` at physical scale (`ARGB`, filled opaque paper on (re)creation), drawn with `Graphics g(img); g.addTransform(scale(sc))`. Per call, **in this order** (matches the mock): fade `fillAll(paper @ 0.3)` → guides (`kTrack`, 1 px: both diagonals and the vertical axis of the `rx×ry` box) → labels (`mono` 10, `kWaveActive`: `L` at `(cx−rx−10, cy−ry)`, `R` at `(cx+rx+10, cy−ry)`, `M` at `(cx, cy−ry−9)`, centred) → trace.

```cpp
const float cx = w / 2, cy = h / 2, ry = h * .4f, rx = juce::jmin(w * .42f, ry * 2.6f);
float pv = 0;
for (int i = 0; i < n; ++i) { float x = r[i] - l[i], y = l[i] + r[i];
    if (!std::isfinite(x) || !std::isfinite(y)) continue; pv = juce::jmax(pv, std::abs(x), std::abs(y)); }
const float p = juce::jmax(.3f, pv);
gain = p > gain ? p : gain * .97f + p * .03f;  const float k = .9f / gain;           // auto-gain, floor .3
if (pv > 1e-6f) {                                       // silence => no stroke (a zero-length path is a stray dot)
    juce::Path path;  bool first = true;
    for (int i = 0; i < n; ++i) { float x = r[i] - l[i], y = l[i] + r[i]; if (!std::isfinite(x) || !std::isfinite(y)) continue;
        juce::Point<float> pt { juce::jlimit(cx - rx, cx + rx, cx + x * k * rx), juce::jlimit(cy - ry, cy + ry, cy - y * k * ry) };
        first ? path.startNewSubPath(pt) : path.lineTo(pt);  first = false; }
    g.setColour(theme::kInk.withAlpha(.8f));
    g.strokePath(path, juce::PathStrokeType(1.f, juce::PathStrokeType::curved));    // rounded joins
}
```
`paint` draws the image into bounds. Editor: `n = proc.pullScope(l, r, 1600); scope.addBlock(l, r, n); scope.repaint();` each tick (also when `n == 0`, so the fade continues).

- [ ] **Step 3: Verify + commit.** Build, `test_core`, `auval`. Harness (Q4) assertions: (a) `processBlock` × N with a held note then `pullScope` returns >0 frames and ≤ 1600; (b) calling `processBlock` 200× with no pull never blocks and `scopeFifo` stays ≤ capacity (drop path), then one `pullScope` returns ≤ 1600 and the next returns 0; (c) a block containing a NaN sample draws without asserting (debug build). Snapshot after 300 ms with a held note: trace present, guides/labels at `1e.png` positions. Manual: open/close the editor with audio running (no crash, no growth).

```bash
git add CMakeLists.txt src/plugin/PluginProcessor.h src/plugin/PluginProcessor.cpp src/plugin/ScopeView.h src/plugin/ScopeView.cpp src/plugin/PluginEditor.h src/plugin/PluginEditor.cpp tools/harness/main.cpp
git commit -m "feat(ui): live stereo phase scope fed by a lock-free output FIFO"
```

---

## Milestone 5 — grain-event FIFO + playhead

### Task 5: Core hooks (`ge_pop_grain_event`, `ge_get_playheads`, `ge_voice_playing_note`) + strip flashes/playheads

**Files:** Create `src/core/grain_event.h`, `src/core/event_ring.h`; Modify `src/core/voice.h/.c`, `granular_engine.h/.c`, `tests/test_core.c`, `src/plugin/PluginProcessor.*`, `SourceStrip.*`, `PluginEditor.cpp`.

**Interfaces:**
- Produces (C ABI) `typedef struct { float pos; float span; } GrainEvent;` (`pos` = source position 0..1, `span` = fraction of the source the grain reads = `length_samples·pitch_ratio/num_frames`, clamped ≤ 1); `int ge_pop_grain_event(GranularEngine*, GrainEvent*)` (consumer thread; 1 if popped); `int ge_get_playheads(GranularEngine*, float* out, int max)` (writes `min(max,max_voices)` values, `-1` = inactive voice; returns count written); `int ge_voice_playing_note(GranularEngine*, int note)` (audio thread; 1 if a non-releasing voice holds `note`).
- Produces `bool MotesProcessor::popGrainEvent(GrainEvent&)`, `int MotesProcessor::getPlayheads(float*, int) const`.
- Produces `SourceStrip::addGrainEvent(const GrainEvent&)`, `setPlayheads(const float*, int)`.

- [ ] **Step 1: Failing tests** (append, register in `main`; also `#include "event_ring.h"`).

```c
static void test_event_ring(void){
    GrainEventRing r; ger_init(&r);
    for(int i=0;i<GE_EVENT_RING_SIZE;i++){ GrainEvent e={(float)i,0.f}; assert(ger_push(&r,e)==1); }
    GrainEvent x={0,0}; assert(ger_push(&r,x)==0);                       /* full => dropped, nothing overwritten */
    assert(atomic_load(&r.dropped)==1u);
    GrainEvent o; for(int i=0;i<GE_EVENT_RING_SIZE;i++){ assert(ger_pop(&r,&o)==1); assert(o.pos==(float)i); }   /* FIFO order */
    assert(ger_pop(&r,&o)==0);
    for(int i=0;i<3*GE_EVENT_RING_SIZE;i++){ GrainEvent e={(float)i,0.f}; assert(ger_push(&r,e)); assert(ger_pop(&r,&o)&&o.pos==(float)i); }  /* wraparound */
    printf("test_event_ring OK\n");
}
static void test_grain_events(void){
    GranularEngine* e=ge_create(44100,16);
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f;
    ge_set_sample(e,buf,44100,1,44100);
    ge_set_param(e,P_DENSITY,50); ge_set_param(e,P_POSITION,0.25f); ge_set_param(e,P_SPREAD,0.f); ge_set_param(e,P_GRAIN_MS,100.f);
    unsigned before=ge_get_grains_fired(e);
    ge_note_on(e,60,1.0f);
    float l[512],r[512]; for(int b=0;b<20;b++) ge_render(e,l,r,512);
    GrainEvent ev; int n=0;
    while(ge_pop_grain_event(e,&ev)){ n++; assert(fabsf(ev.pos-0.25f)<1e-4f); assert(fabsf(ev.span-0.1f)<1e-3f); }
    assert(n>0 && (unsigned)n==ge_get_grains_fired(e)-before);           /* one event per spawned grain */
    ge_set_param(e,P_PITCH_SEMI,12.f); ge_render(e,l,r,512);             /* +12 st => grain reads twice the source */
    while(ge_pop_grain_event(e,&ev)) assert(fabsf(ev.span-0.2f)<1e-3f);
    ge_destroy(e);
    /* 0-frame sample must not divide by zero or emit events */
    e=ge_create(44100,16); float one=0; ge_set_sample(e,&one,0,1,44100); ge_note_on(e,60,1.f); ge_render(e,l,r,512);
    assert(!ge_pop_grain_event(e,&ev)); ge_destroy(e);
    printf("test_grain_events OK\n");
}
static void test_playheads_and_playing_note(void){
    GranularEngine* e=ge_create(44100,16);
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f;
    ge_set_sample(e,buf,44100,1,44100);
    float ph[16], l[512], r[512];
    ge_set_param(e,P_POSITION,0.4f); ge_render(e,l,r,64);
    assert(ge_get_playheads(e,ph,16)==16); for(int i=0;i<16;i++) assert(ph[i]<0.f);
    assert(!ge_voice_playing_note(e,60));
    ge_note_on(e,60,1.f); ge_render(e,l,r,64); ge_get_playheads(e,ph,16);
    int live=0; for(int i=0;i<16;i++) if(ph[i]>=0.f){ live++; assert(fabsf(ph[i]-0.4f)<1e-4f); } assert(live==1);
    assert(ge_voice_playing_note(e,60) && !ge_voice_playing_note(e,61));
    ge_set_param(e,P_MACRO_SPEED,1.f); for(int b=0;b<50;b++) ge_render(e,l,r,512);   /* drift ~0.58 */
    ge_get_playheads(e,ph,16); for(int i=0;i<16;i++) if(ph[i]>=0.f){ assert(ph[i]>0.45f && ph[i]<=1.0f); }
    ge_note_off(e,60); assert(!ge_voice_playing_note(e,60));                          /* releasing is not "playing" */
    ge_destroy(e); printf("test_playheads_and_playing_note OK\n");
}
```
Build `test_core`: expect FAIL (missing headers/functions).

- [ ] **Step 2: Implement.**

`grain_event.h`: `#pragma once` + the `GrainEvent` typedef (no includes beyond none). `granular_engine.h`: `#include "grain_event.h"` and the three declarations. `event_ring.h`: C-only ring exactly as the test uses (`GE_EVENT_RING_SIZE 512`, `GrainEvent buf[]`, `atomic_uint head, tail, dropped`; `ger_init`, `ger_push` with relaxed-load of `head` / acquire-load of `tail` / release-store of `head`, drop+count when `head - tail >= SIZE`; `ger_pop` mirrored). `voice.h`: add `struct GrainEventRing* events;` to `Voice` and `double voice_playhead(const Voice*, const Params*)`. `voice.c`: at the end of a successful `spawn_grain`:

```c
if(v->events && smp->num_frames>0){
    double span=(double)g->length_samples*g->pitch_ratio/(double)smp->num_frames;
    GrainEvent ev={ (float)pos, (float)(span>1.0?1.0:span) };
    ger_push(v->events,ev);
}
double voice_playhead(const Voice* v,const Params* p){ double x=p->position+v->drift; return x<0?0:(x>1?1:x); }
```
(`spawn_grain`'s own position math is **unchanged**; the playhead is metering only.) `granular_engine.c`: add `GrainEventRing events; _Atomic float* playheads;` to the struct; in `ge_create` `ger_init(&e->events)`, calloc/`atomic_init` the playheads (−1), and `voices[i].events=&e->events`; in `ge_destroy` free playheads; at the end of `ge_render` (after the has_sample block, so idle voices still publish −1):

```c
for(int i=0;i<e->max_voices;i++){
    float ph=e->voices[i].active?(float)voice_playhead(&e->voices[i],&e->params):-1.f;
    atomic_store_explicit(&e->playheads[i],ph,memory_order_relaxed);
}
```
`ge_pop_grain_event` → `ger_pop`; `ge_get_playheads` loads each atomic; `ge_voice_playing_note` loops voices: `active && note==n && stage!=4`. Run `test_core`: expect all tests incl. the three new ones PASS. Optional ASan/UBSan pass as the old plan did.

- [ ] **Step 3: Processor pass-through** (`popGrainEvent`, `getPlayheads`). Editor `timerCallback`: **drain and discard** events once in the editor constructor (a ring filled while no editor was open must not replay as a flash storm), then each tick pop ≤ 256 events into `strip.addGrainEvent`, and `proc.getPlayheads(ph,16)` → `strip.setPlayheads`.

- [ ] **Step 4: Strip drawing.** Flashes: fixed array of 128 `{pos, spanPx, startMs}` (overwrite oldest; no heap growth). For each live flash (age < 250 ms), alpha `(1 − age/250)·0.9`, width `fw = clamp(span·w, 3, 0.25·w)`, centre `gx = pos·w`; bars every 1.5 px, 0.9 px wide:

```cpp
for (float i = 0; i < fw; i += 1.5f) {
    const float u = i / fw, win = .5f - .5f * std::cos(juce::MathConstants<float>::twoPi * u);
    const float s = peaks[(size_t) juce::jlimit(0, 899, (int) ((gx - fw / 2 + u * fw) / w * 900))] * am * win;
    g.fillRect(gx + i - fw / 2, by - s, .9f, s * 2 + .3f);
}
```
Playheads: for every voice with `ph ≥ 0`: Position line + spread band at `ph·w`; if none are active, draw them at the parameter position (that line is also the drag handle). Cap alpha accumulation by drawing the band once per distinct playhead.

- [ ] **Step 5: Verify + commit.** Build, `test_core`, `auval`, harness snapshot with a held note (flashes visible within ~250 ms of a block; playhead line equals the parameter position at Drift=0 and drifts for Drift≠0). **Report Q3 evidence:** set Position .8, Drift +1, hold a note for ~1 s in the harness and print `getPlayheads`: it saturates at 1.0 (engine clamps) whereas the mock would wrap.

```bash
git add src/core/grain_event.h src/core/event_ring.h src/core/voice.h src/core/voice.c src/core/granular_engine.h src/core/granular_engine.c tests/test_core.c src/plugin/PluginProcessor.h src/plugin/PluginProcessor.cpp src/plugin/SourceStrip.h src/plugin/SourceStrip.cpp src/plugin/PluginEditor.cpp
git commit -m "feat(core): lock-free grain-event ring and per-voice playheads; strip shows real grains"
```

### Task 5b (only if Q3 = wrap): make `position+drift` wrap like the mock

- [ ] Failing test: `P_POSITION=.8`, `P_MACRO_SPEED=1`, render ~.58 s, assert `ge_get_playheads` ≈ `fmod(.8+drift,1)`; change `voice_playhead` and `spawn_grain` to `x -= floor(x)` (spray applied after wrap, then clamped); run `test_core`; commit `fix(core): wrap drifted playhead instead of clamping at the sample end`. **This changes the sound**, so Hunter listens before it is kept.

---

## Milestone 6 — presets, HOLD, Load, WAV drop

### Task 6: Presets, HOLD latch, Load…, drag-and-drop, state restore

**Files:** Create `src/plugin/MotesPresets.h`; Modify `src/plugin/PluginProcessor.h/.cpp`, `HeaderBar.*`, `FooterBar.*`, `PluginEditor.*`, `tools/harness/main.cpp`.

**Interfaces:**
- Produces `MotesProcessor::setHold(bool)`, `bool isHoldOn() const`, `void applyPreset(int)` (message thread), `int currentPreset() const`; `kPresets[4]` (`name` + 13 values), order Isolation Drift, Glass Rain, Low Tide, Init.

- [ ] **Step 1: Presets** — table values copied verbatim from the handoff (all 13 in-range for the current ranges):

```cpp
struct Preset { const char* name; float v[13]; };   // order = kPresetIds
static constexpr const char* kPresetIds[13] = { "density","grain","position","spread","panspread","pitchsemi","pitchcent","macro","attack","decay","sustain","release","master" };
static constexpr Preset kPresets[4] = {
 {"Isolation Drift",{42,180,.38f,.22f,.45f, 0, 0,  .12f,.35f,.6f,.7f,1.8f,.8f}},
 {"Glass Rain",     {88, 24,.70f,.50f,.90f,12,-7, -.30f,.005f,.25f,.40f,.60f,.70f}},
 {"Low Tide",       { 9,460,.15f,.08f,.25f,-12, 4,  .04f,1.6f,1.0f,.90f,3.5f,.85f}},
 {"Init",           {30,120,.50f,.05f,.20f, 0, 0,  0.f,.01f,.1f,.8f,.3f,.8f}} };
```
`applyPreset(i)`: for each id `p=apvts.getParameter(id); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();` then `apvts.state.setProperty("preset", i, nullptr)`. Sample is untouched. Initial display = "Init" (the plugin's parameter defaults equal Init; the mock's initial "Isolation Drift" is a mock artefact). The name stays after manual tweaks (as the mock).

- [ ] **Step 2: HOLD in the processor.** Member `std::atomic<bool> holdRequested{false}`; audio-thread-only `bool holdWasOn=false; static constexpr int kHoldNote=60;`. In `processBlock`:

```cpp
const bool hold = holdRequested.load(std::memory_order_relaxed);
for (const auto meta : midi) {
    const auto m = meta.getMessage();
    if (hold && (m.isNoteOn() || m.isNoteOff()) && m.getNoteNumber() == kHoldNote) continue;   // the latch already sounds C4
    if (m.isNoteOn())  ge_note_on(engine, m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) ge_note_off(engine, m.getNoteNumber());
    else if (m.isAllNotesOff()) ge_all_notes_off(engine);
}
if (hold) { if (!ge_voice_playing_note(engine, kHoldNote)) ge_note_on(engine, kHoldNote, 1.0f); }   // re-latches after sample load, all-notes-off, stealing
else if (holdWasOn) ge_note_off(engine, kHoldNote);
holdWasOn = hold;
```
HOLD is **not** saved in plugin state (a project must not start droning on load) and not an automatable parameter; it persists across editor close/open because it lives in the processor. Known edge (documented, accepted): a C4 key already held when HOLD is switched on counts as the latch and is released when HOLD goes off.

- [ ] **Step 3: Seam assertions (Q4 harness `--selftest`)**: (a) HOLD on → block → `ge_active_voice_count()==1`; (b) incoming MIDI C4 on/off while HOLD → count stays 1; (c) `ge_all_notes_off` via MIDI → still exactly 1 sounding (not 2) after the block; (d) `loadWavFile` while HOLD → sounding again next block; (e) 16 other notes then a 17th (stealing) → latch re-established; (f) HOLD off → voice reaches release and the count returns to 0 after `release` seconds of blocks. Without the harness: do (a)–(f) by ear/meter in Ableton.

- [ ] **Step 4: Load…, drop, state.** Footer `onLoad` launches the existing async `FileChooser` (`*.wav`). Editor implements `juce::FileDragAndDropTarget` (`isInterestedInFileDrag` → true; `filesDropped(files, x, y)` accepts only when `y` ∈ [45, 223) i.e. scope + strip, first file only). Both paths call `loadWavFile(f)` then `apvts.state.setProperty("sampleName", f.getFileName(), nullptr)`; on `false` set footer meta to `could not decode that file` and leave the previous name. `setStateInformation` passes the stored `sampleName` into `loadWavFile`; the editor timer's `sampleVersion` check refreshes strip + footer, so a restore while the editor is open updates it. Header ‹ › cycle `(i±1+4)%4` through `applyPreset`; HOLD pill → `proc.setHold`; editor reads `proc.isHoldOn()` / `currentPreset()` in its constructor so reopening shows true state.

- [ ] **Step 5: Verify + commit.** Build, `test_core`, `test_format`, `auval`; harness `--selftest` and snapshots of all four presets vs `1e.png` (Isolation Drift should match the reference knobs/values: 42 g/s, 180 ms, 22%, 38%, ±0 st, −1.9 dB, 45%, +0.12, ±0 ct, 350 ms, 600 ms, 70%, 1.80 s). Manual in Ableton (overwrites main-folder install; say so): HOLD makes sound with no MIDI, drag a WAV onto the scope and the strip, Load… works, a corrupt file shows the error text, save/reload the set keeps sample name + waveform.

```bash
git add src/plugin/MotesPresets.h src/plugin/PluginProcessor.h src/plugin/PluginProcessor.cpp src/plugin/HeaderBar.h src/plugin/HeaderBar.cpp src/plugin/FooterBar.h src/plugin/FooterBar.cpp src/plugin/PluginEditor.h src/plugin/PluginEditor.cpp tools/harness/main.cpp
git commit -m "feat(ui): factory presets, HOLD latch via processBlock, Load…/WAV drop, sample name in state"
```

---

## Milestone 7 — remove `MotesField`, cleanup, review

### Task 7: Delete the old field, tidy, whole-branch review

**Files:** Delete `src/plugin/MotesField.h/.cpp`; Modify `CMakeLists.txt`, `src/plugin/PluginEditor.h` (drop `#include "MotesField.h"`), maybe `PluginProcessor.*`.

- [ ] **Step 1:** `git rm src/plugin/MotesField.h src/plugin/MotesField.cpp`; remove it from `target_sources`. Grep for leftovers: `grep -rn "MotesField\|getOutRms\|getGrainsFired" src/ tools/ CMakeLists.txt`. `getOutRms/getGrainsFired` (processor wrappers) become unused; keep the core `ge_get_*` meters and their test, delete the two wrappers only if nothing else uses them.
- [ ] **Step 2:** Final clean build from scratch (`rm -rf build`), `test_core`, `test_format`, harness `--selftest`, `auval -v aumu Mote Hntr`, snapshot @1x/@2x vs `1e.png`, and `git status` shows only intended files (` m JUCE` untouched, no `build/`, no `docs/design-reference/out/`).
- [ ] **Step 3: Whole-branch review** (per-task reviews missed bugs at the audio/GUI seam before): invoke `superpowers:requesting-code-review` on `main..feat/ui-makeover`, with explicit focus on the five Review Focus items, the lock-free contract (grep the `processBlock`/`ge_render` call chain for `new|malloc|Lock|Critical`), and ring ordering (acquire/release pairs).
- [ ] **Step 4:** Commit `refactor(ui): remove MotesField, final cleanup`. Then write the hand-off note: exact edits for `CLAUDE.md` (Design section → paper-and-ink 640×496, fonts, new files, harness option) and the old spec (mark superseded), for Hunter to copy to the main folder.

---

## Self-review

- **Spec coverage:** header/preset/HOLD (T1, T6); scope incl. mapping, auto-gain, persistence, guides, labels (T4); strip waveform cache, spread band, Position line/drag, flashes, drop (T3, T5, T6); 13 knobs, formats, bipolar arcs, drag/shift/double-click, log/semitone ranges (T2); footer/Load…/meta/error text (T1, T6); presets (T6); engine hooks: output FIFO (T4), grain events (T5), playheads (T5), peaks (T3); fixed size (T1); fonts + OFL + fallback (T1); one accent constant (T1). Not built by design: 1a–1d, resize, mock's drift-reset on drag, mock corner radius/shadow.
- **Placeholders:** none; the only conditionals are the explicit Q1–Q4 gates, each with a stated default.
- **Types:** `GrainEvent{pos,span}`, `SampleInfo`, `pullScope`, `KnobSpec`, `kPresets`, `ge_*` signatures are identical wherever they appear.
