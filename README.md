## Motes

A polyphonic granular sample-as-synth for Ableton Live and other DAWs. Load a sample, play it from MIDI, and shape the grain cloud with 13 parameters. Available as **AU** and **VST3** for macOS on Apple Silicon (arm64).

![The Motes editor](/motes-demo-recording.gif)


- **Phase scope**: a live stereo goniometer of the real output.
- **Source strip**: the sample's waveform, with the spread band, the playhead of each sounding voice, and a flash for every grain as it is cut.
- **13 knobs**: Density, Size, Spread, Position, Pitch, Master, Pan, Drift, Fine, Attack, Decay, Sustain, Release. Drag, Shift to fine-tune, double-click to reset.
- **Header**: factory presets (‹ ›) and **HOLD**, which latches a note so you can audition without MIDI.
- **Load…** or drop a WAV onto the scope or strip.

## Architecture

- **Zero-dependency C DSP Core (`src/core/`)**: Polyphonic grain scheduler, Hann window generator, cubic/linear interpolation read heads, and per-voice ADSR envelopes written in standalone C with zero audio-thread heap allocations.
- **Lock-Free Telemetry**: Audio-to-UI ring buffers stream real-time stereo output samples (goniometer phase scope) and active grain spawn coordinates (waveform source strip) without blocking `processBlock`.
- **JUCE Wrapper (`src/plugin/`)**: Builds `AU` and `VST3` for macOS on Apple Silicon (arm64).

## Download & Installation

Pre-built binaries can be downloaded from [GitHub Releases](https://github.com/closestfriend/motes/releases).

1. Download `Motes-<version>-macOS.zip` from the [latest release](https://github.com/closestfriend/motes/releases/latest).
2. Unzip and copy the plugins into your audio plug-ins folder:
   - **AU**: Copy `Motes.component` to `~/Library/Audio/Plug-Ins/Components/`
   - **VST3**: Copy `Motes.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
   *(In Finder, press `Cmd+Shift+G` and paste the path to jump directly to the folder).*
3. **macOS Gatekeeper notice**: Since open-source plugins downloaded from the web aren't notarized with an Apple Developer ID, macOS quarantines them by default. Clear the quarantine flag in Terminal:
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/Components/Motes.component
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/Motes.vst3
   ```
4. In Ableton Live, open **Preferences > Plug-Ins** and click **Rescan Plug-Ins** (or hold `Option` while clicking Rescan for a full scan).
5. (Optional) Validate the AU via Terminal:
   ```bash
   auval -v aumu Mote Hntr
   ```

---

## Build from Source

Requires an Apple Silicon Mac, CMake, and the Command Line Tools (full Xcode is not required). JUCE is included as a submodule.

```bash
git submodule update --init
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

A build automatically installs the AU and VST3 into `~/Library/Audio/Plug-Ins/`.

`git status` will show ` m JUCE`. That is intentional: the JUCE submodule is pinned to upstream 7.0.12 and a small patch (`patches/`, a macOS 15 SDK fix) is applied at configure time. Don't commit it.

---

## Tests

```bash
cmake --build build --target test_core test_format
ctest --test-dir build --output-on-failure
```

- `test_core` covers the portable C11 engine in `src/core/`.
- `test_format` covers the editor's value formatting.

There is also an optional headless harness for the editor and the audio/GUI seam (`-DMOTES_BUILD_HARNESS=ON`, off by default):
```bash
motes_harness --selftest
motes_harness --snapshot out.png   # renders editor without a host
```

## Layout

- `src/core/`: the DSP engine, portable C11 with no JUCE dependency, exposed through a small C ABI.
- `src/plugin/`: the JUCE shell (processor, editor and components).
- `assets/fonts/`: Barlow Condensed and IBM Plex Mono (SIL OFL; licenses alongside).
- `docs/`: design notes, plans, and the editor's reference capture.
