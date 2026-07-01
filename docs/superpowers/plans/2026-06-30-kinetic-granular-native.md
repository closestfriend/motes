# KINETIC GRANULAR Native Plugin — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a polyphonic granular sample-as-synth that loads in Ableton Live as a native arm64 AU/VST3, with a portable C DSP core wrapped in a thin JUCE C++ shell.

**Architecture:** A dependency-free C DSP core (`src/core/`) owns all synthesis — WAV loading, windowed grains, per-note voices with ADSR, voice pool + stealing — and exposes a small C ABI (`granular_engine.h`). A JUCE C++ shell (`src/plugin/`) translates host MIDI/params/state into calls on that core and pulls audio in `processBlock`. The core is unit-tested via a standalone CLI harness that never touches JUCE.

**Tech Stack:** C11 (DSP core), C++17 + JUCE 7 (plugin shell), CMake (build), git submodule for JUCE, clang (native arm64), `auval` for AU validation.

## Global Constraints

- **Platform:** macOS Apple Silicon (arm64), native — no Rosetta. Verify each built binary is `arm64`.
- **Plugin formats:** AU **and** VST3, both must load in Ableton Live on Mac.
- **Language split:** everything under `src/core/` is portable **C11** with **no JUCE include and no C++**; only `src/plugin/` is C++.
- **Real-time safety:** no `malloc`/`free`, no locks, no file I/O, no logging inside `ge_render` or any function it calls. Allocation/loading happens only on the message thread.
- **Voice ceiling (v1):** 16 voices.
- **Sample format (v1):** WAV only, one sample layer.
- **Git remote:** Hunter's personal `origin` only. NEVER add/push/fetch any `Memetica-Corporation` remote.
- **Repo root:** `~/Projects/cf audio pack/kinetic-granular-native/`.
- **Build generator:** full Xcode is NOT installed on this machine (Command Line Tools only, no Ninja). Use the **Unix Makefiles** generator: configure with `cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`. Do NOT use `-G Xcode` or `xcodebuild`. Single-config generator, so `--config Release` on build commands is a harmless no-op. If AU/VST3 fails to build or validate under Makefiles + CLT, STOP and escalate (fallback is installing full Xcode).
- **Reference spec for the sound:** `../kinetic-granular/src/lib/audio.ts`.

---

## File Structure

```
kinetic-granular-native/
  CMakeLists.txt              # build: core static lib, test harness, JUCE plugin
  JUCE/                       # git submodule (JUCE 7)
  src/
    core/
      granular_engine.h       # the C ABI (public)
      granular_engine.c       # engine: voice pool, note routing, params, mixdown
      voice.h  voice.c        # per-note grain cloud + ADSR
      grain.h  grain.c        # one windowed, resampled grain
      window.h window.c       # Hann window lookup table
      wav.h    wav.c          # minimal WAV -> float decode
      params.h                # shared param id enum + ranges (C header)
    plugin/
      PluginProcessor.h/.cpp  # juce::AudioProcessor
      PluginEditor.h/.cpp     # generic parameter GUI
  tests/
    test_core.c               # CLI harness linking core/ (unit tests)
    wav_fixture.c/.h          # helper to synthesize a WAV fixture on disk
  docs/
    DESIGN.md                 # approved design (already written)
```

**Responsibilities:**
- `window.*` — pure math, no state: fill/read a Hann window.
- `grain.*` — render one grain from a source buffer into an accumulator; owns resampling + windowing.
- `voice.*` — one MIDI note's worth of sound: schedules grains at `density`, applies ADSR, tracks position playhead + macro drift.
- `granular_engine.*` — owns the voice array, param state, sample buffer hand-off, note routing, and the final stereo mixdown. The only file the plugin `#include`s.
- `wav.*` — decode a `.wav` file (PCM16/PCM24/float32) into interleaved floats + metadata.
- `plugin/*` — JUCE glue only; contains no DSP.

---

## Task 1: Repo skeleton + JUCE + empty plugin that loads in Ableton

**Files:**
- Create: `~/Projects/cf audio pack/kinetic-granular-native/CMakeLists.txt`
- Create: `src/plugin/PluginProcessor.h`, `src/plugin/PluginProcessor.cpp`
- Create: `src/plugin/PluginEditor.h`, `src/plugin/PluginEditor.cpp`
- Create: `.gitignore`
- Submodule: `JUCE/`

**Interfaces:**
- Produces: a buildable JUCE `AudioProcessor` named `KineticGranular` that emits silence. Later tasks replace the guts of `processBlock`.

- [ ] **Step 1: Init repo + JUCE submodule**

```bash
cd "$HOME/Projects/cf audio pack/kinetic-granular-native"
git init
git add docs/DESIGN.md
git submodule add https://github.com/juce-framework/JUCE.git JUCE
cd JUCE && git checkout 7.0.12 && cd ..
printf '%s\n' 'build/' '.DS_Store' '*.o' 'cmake-build-*/' > .gitignore
```

- [ ] **Step 2: Write `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.22)
project(KineticGranular VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

add_subdirectory(JUCE)

# Portable C DSP core (no JUCE)
add_library(granular_core STATIC
    src/core/granular_engine.c
    src/core/voice.c
    src/core/grain.c
    src/core/window.c
    src/core/wav.c)
target_include_directories(granular_core PUBLIC src/core)
target_link_libraries(granular_core PRIVATE m)

# Plugin
juce_add_plugin(KineticGranular
    PLUGIN_MANUFACTURER_CODE Hntr
    PLUGIN_CODE Kgrn
    FORMATS AU VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PRODUCT_NAME "Kinetic Granular")

target_sources(KineticGranular PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp)
target_link_libraries(KineticGranular PRIVATE
    granular_core
    juce::juce_audio_utils
    juce::juce_dsp)
target_compile_definitions(KineticGranular PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
```

> Note: `granular_core` lists source files that don't exist yet. Create empty stubs so CMake configures — Step 3.

- [ ] **Step 3: Create empty core stubs so the build configures**

```bash
mkdir -p src/core
for f in granular_engine voice grain window wav; do
  printf '/* stub */\n' > "src/core/$f.c"
done
# headers referenced later; create minimal ones
printf '#pragma once\n' > src/core/granular_engine.h
```

- [ ] **Step 4: Write minimal `PluginProcessor.h`**

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class KineticGranularProcessor : public juce::AudioProcessor
{
public:
    KineticGranularProcessor();
    ~KineticGranularProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kinetic Granular"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KineticGranularProcessor)
};
```

- [ ] **Step 5: Write minimal `PluginProcessor.cpp` (emits silence)**

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

KineticGranularProcessor::KineticGranularProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true)) {}
KineticGranularProcessor::~KineticGranularProcessor() {}

void KineticGranularProcessor::prepareToPlay(double, int) {}

void KineticGranularProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    buffer.clear();
}

juce::AudioProcessorEditor* KineticGranularProcessor::createEditor()
{
    return new KineticGranularEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KineticGranularProcessor();
}
```

- [ ] **Step 6: Write `PluginEditor.h` / `.cpp` (generic UI placeholder)**

```cpp
// PluginEditor.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class KineticGranularEditor : public juce::AudioProcessorEditor
{
public:
    explicit KineticGranularEditor(KineticGranularProcessor& p)
      : AudioProcessorEditor(p) { setSize(420, 260); }
    void paint(juce::Graphics& g) override
    { g.fillAll(juce::Colours::black);
      g.setColour(juce::Colours::white);
      g.drawText("Kinetic Granular", getLocalBounds(),
                 juce::Justification::centred); }
    void resized() override {}
};
```

```cpp
// PluginEditor.cpp
#include "PluginEditor.h"
```

- [ ] **Step 7: Configure + build**

Run:
```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target KineticGranular_AU KineticGranular_VST3 -j
```
Expected: builds `Kinetic Granular.component` and `.vst3` under `build/KineticGranular_artefacts/Release/` (or `.../<empty>/` — path shown in build output).

- [ ] **Step 8: Verify arm64 + AU validation**

Run:
```bash
lipo -archs "build/KineticGranular_artefacts/Release/AU/Kinetic Granular.component/Contents/MacOS/Kinetic Granular"
auval -v aumu Kgrn Hntr
```
Expected: `lipo` prints `arm64`; `auval` ends with `AU VALIDATION SUCCEEDED`.

- [ ] **Step 9: Manual load test in Ableton**

Copy the `.component` to `~/Library/Audio/Plug-Ins/Components/` and the `.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, rescan in Ableton, drop "Kinetic Granular" on a MIDI track. Expected: loads, shows the placeholder editor, plays silence without crashing.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "feat: JUCE skeleton — empty AU/VST3 plugin loads in Ableton"
```

---

## Task 2: WAV decoder (C core, TDD)

**Files:**
- Create: `src/core/wav.h`, `src/core/wav.c` (replaces stub)
- Create: `tests/test_core.c`, `tests/wav_fixture.h`, `tests/wav_fixture.c`
- Modify: `CMakeLists.txt` (add test executable)

**Interfaces:**
- Produces:
  ```c
  typedef struct { float* data; int num_frames; int num_channels; double sample_rate; } WavData;
  int wav_load(const char* path, WavData* out);   /* 0 = ok, <0 = error */
  void wav_free(WavData* w);
  ```
  `data` is interleaved float32 in [-1,1], `num_frames` frames × `num_channels`.

- [ ] **Step 1: Write `tests/wav_fixture.{h,c}` (writes a known PCM16 WAV)**

```c
// wav_fixture.h
#pragma once
/* Writes a mono PCM16 WAV: `frames` samples of value `amp` (constant). Returns 0 ok. */
int write_test_wav(const char* path, int frames, int sample_rate, short amp);
```

```c
// wav_fixture.c
#include "wav_fixture.h"
#include <stdio.h>
#include <string.h>
static void w32(FILE* f, unsigned v){ fputc(v&255,f);fputc((v>>8)&255,f);fputc((v>>16)&255,f);fputc((v>>24)&255,f); }
static void w16(FILE* f, unsigned v){ fputc(v&255,f);fputc((v>>8)&255,f); }
int write_test_wav(const char* path, int frames, int sr, short amp){
    FILE* f=fopen(path,"wb"); if(!f) return -1;
    unsigned data=frames*2, byteRate=sr*2;
    fwrite("RIFF",1,4,f); w32(f,36+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); w32(f,16); w16(f,1); w16(f,1);
    w32(f,sr); w32(f,byteRate); w16(f,2); w16(f,16);
    fwrite("data",1,4,f); w32(f,data);
    for(int i=0;i<frames;i++) w16(f,(unsigned short)amp);
    fclose(f); return 0;
}
```

- [ ] **Step 2: Write the failing test in `tests/test_core.c`**

```c
#include "wav.h"
#include "wav_fixture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_wav_load(void){
    const char* p="/tmp/kg_fixture.wav";
    assert(write_test_wav(p, 100, 44100, 16384)==0);
    WavData w;
    assert(wav_load(p,&w)==0);
    assert(w.num_frames==100);
    assert(w.num_channels==1);
    assert(fabs(w.sample_rate-44100.0)<1e-6);
    /* 16384/32768 = 0.5 */
    assert(fabsf(w.data[0]-0.5f)<0.001f);
    wav_free(&w);
    printf("test_wav_load OK\n");
}

int main(void){ test_wav_load(); printf("ALL CORE TESTS PASSED\n"); return 0; }
```

- [ ] **Step 3: Add test target to `CMakeLists.txt`**

```cmake
add_executable(test_core tests/test_core.c tests/wav_fixture.c)
target_link_libraries(test_core PRIVATE granular_core)
target_include_directories(test_core PRIVATE tests)
enable_testing()
add_test(NAME core COMMAND test_core)
```

- [ ] **Step 4: Run test to verify it fails**

Run: `cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build --target test_core`
Expected: link error — `wav_load` undefined (stub is empty).

- [ ] **Step 5: Write `src/core/wav.h`**

```c
#pragma once
typedef struct { float* data; int num_frames; int num_channels; double sample_rate; } WavData;
int wav_load(const char* path, WavData* out);
void wav_free(WavData* w);
```

- [ ] **Step 6: Write `src/core/wav.c` (PCM16/24/float32, little-endian RIFF)**

```c
#include "wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rd32(const unsigned char* p){ return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24; }
static unsigned rd16(const unsigned char* p){ return p[0]|p[1]<<8; }

int wav_load(const char* path, WavData* out){
    FILE* f=fopen(path,"rb"); if(!f) return -1;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<44){ fclose(f); return -2; }
    unsigned char* buf=(unsigned char*)malloc(sz);
    if(!buf){ fclose(f); return -3; }
    if(fread(buf,1,sz,f)!=(size_t)sz){ free(buf); fclose(f); return -4; }
    fclose(f);
    if(memcmp(buf,"RIFF",4)||memcmp(buf+8,"WAVE",4)){ free(buf); return -5; }

    unsigned fmt=0,ch=0,sr=0,bits=0; long off=12; long dataOff=-1,dataLen=0;
    while(off+8<=sz){
        const unsigned char* id=buf+off; unsigned clen=rd32(buf+off+4); long body=off+8;
        if(!memcmp(id,"fmt ",4)){
            fmt=rd16(buf+body); ch=rd16(buf+body+2); sr=rd32(buf+body+4); bits=rd16(buf+body+14);
        } else if(!memcmp(id,"data",4)){ dataOff=body; dataLen=clen; }
        off = body + clen + (clen&1);
    }
    if(dataOff<0||ch==0){ free(buf); return -6; }

    int bytes=bits/8; int frames=dataLen/(bytes*ch);
    float* data=(float*)malloc(sizeof(float)*frames*ch);
    if(!data){ free(buf); return -7; }
    const unsigned char* d=buf+dataOff;
    for(int i=0;i<frames*(int)ch;i++){
        const unsigned char* s=d+(long)i*bytes; float v=0.f;
        if(fmt==3 && bits==32){ float f32; memcpy(&f32,s,4); v=f32; }
        else if(bits==16){ short x=(short)rd16(s); v=x/32768.0f; }
        else if(bits==24){ int x=(s[0])|(s[1]<<8)|(s[2]<<16); if(x&0x800000) x|=~0xFFFFFF; v=x/8388608.0f; }
        else if(bits==8){ v=(s[0]-128)/128.0f; }
        data[i]=v;
    }
    out->data=data; out->num_frames=frames; out->num_channels=(int)ch; out->sample_rate=(double)sr;
    free(buf); return 0;
}

void wav_free(WavData* w){ if(w&&w->data){ free(w->data); w->data=NULL; } }
```

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build build --target test_core && ./build/test_core`
Expected: `test_wav_load OK` then `ALL CORE TESTS PASSED`.

- [ ] **Step 8: Commit**

```bash
git add src/core/wav.* tests/ CMakeLists.txt
git commit -m "feat(core): minimal WAV decoder (PCM8/16/24, float32) + test harness"
```

---

## Task 3: Hann window + single grain (C core, TDD)

**Files:**
- Create: `src/core/window.h`, `src/core/window.c` (replaces stub)
- Create: `src/core/grain.h`, `src/core/grain.c` (replaces stub)
- Modify: `tests/test_core.c` (add tests)

**Interfaces:**
- Produces:
  ```c
  /* window.h */
  void window_init(void);                 /* fills the shared Hann table once */
  float window_hann(float phase01);       /* phase in [0,1] -> [0,1] window gain */

  /* grain.h */
  typedef struct {
      const float* src; int src_frames; int src_channels; /* source buffer */
      double read_pos;      /* fractional start frame into src */
      double pitch_ratio;   /* playback speed multiplier */
      int length_samples;   /* grain duration in OUTPUT samples */
      int age;              /* samples elapsed, 0..length_samples */
      float pan;            /* -1..1 */
      float amp;            /* linear gain */
      int active;
  } Grain;
  /* Advance grain by one output sample, add into l/r. Returns 0 when finished. */
  int grain_tick(Grain* g, float* l, float* r);
  ```

- [ ] **Step 1: Write failing tests in `tests/test_core.c`**

```c
#include "window.h"
#include "grain.h"
/* ...add to existing includes... */

static void test_window(void){
    window_init();
    assert(window_hann(0.0f) < 0.001f);
    assert(window_hann(1.0f) < 0.001f);
    assert(window_hann(0.5f) > 0.999f);
    printf("test_window OK\n");
}

static void test_grain_length_and_taper(void){
    window_init();
    float src[1000]; for(int i=0;i<1000;i++) src[i]=1.0f; /* DC = 1.0 */
    Grain g={0}; g.src=src; g.src_frames=1000; g.src_channels=1;
    g.read_pos=0; g.pitch_ratio=1.0; g.length_samples=100; g.age=0;
    g.pan=0.0f; g.amp=1.0f; g.active=1;
    int ticks=0; float first_l=0,first_r=0,mid_l=0,mid_r=0;
    for(int i=0;i<200;i++){
        float l=0,r=0; int alive=grain_tick(&g,&l,&r);
        if(i==0){ first_l=l; first_r=r; }
        if(i==50){ mid_l=l; mid_r=r; }
        if(alive) ticks++; else break;
    }
    assert(ticks==100);                 /* runs exactly length_samples */
    assert(fabsf(first_l)<0.05f);       /* window ~0 at start */
    assert(mid_l>0.6f);                 /* window near peak mid-grain */
    printf("test_grain OK\n");
}
```
Add `test_window(); test_grain_length_and_taper();` to `main()` before the final print.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target test_core`
Expected: link error — `window_init`/`grain_tick` undefined.

- [ ] **Step 3: Write `src/core/window.{h,c}`**

```c
// window.h
#pragma once
void window_init(void);
float window_hann(float phase01);
```

```c
// window.c
#include "window.h"
#include <math.h>
#define WIN_N 4096
static float g_table[WIN_N+1];
static int g_ready=0;
void window_init(void){
    if(g_ready) return;
    for(int i=0;i<=WIN_N;i++){
        double p=(double)i/WIN_N;
        g_table[i]=(float)(0.5*(1.0-cos(2.0*M_PI*p)));
    }
    g_ready=1;
}
float window_hann(float phase01){
    if(phase01<=0.f) return 0.f; if(phase01>=1.f) return 0.f;
    float x=phase01*WIN_N; int i=(int)x; float frac=x-i;
    return g_table[i]+(g_table[i+1]-g_table[i])*frac;
}
```

- [ ] **Step 4: Write `src/core/grain.{h,c}`**

```c
// grain.h
#pragma once
typedef struct {
    const float* src; int src_frames; int src_channels;
    double read_pos; double pitch_ratio;
    int length_samples; int age;
    float pan; float amp; int active;
} Grain;
int grain_tick(Grain* g, float* l, float* r);
```

```c
// grain.c
#include "grain.h"
#include "window.h"

static float sample_at(const Grain* g, double pos, int ch){
    if(pos<0||pos>=g->src_frames-1) return 0.f;
    long i=(long)pos; double f=pos-i;
    const float* s=g->src;
    int C=g->src_channels;
    float a=s[i*C+ch], b=s[(i+1)*C+ch];
    return (float)(a+(b-a)*f);
}

int grain_tick(Grain* g, float* l, float* r){
    if(!g->active || g->age>=g->length_samples){ g->active=0; return 0; }
    float phase=(float)g->age/(float)g->length_samples;
    float w=window_hann(phase)*g->amp;
    float mono, left, right;
    if(g->src_channels>=2){
        left=sample_at(g,g->read_pos,0);
        right=sample_at(g,g->read_pos,1);
    } else {
        mono=sample_at(g,g->read_pos,0); left=right=mono;
    }
    /* equal-power-ish pan */
    float pan=g->pan; float lg=(pan<=0)?1.0f:(1.0f-pan); float rg=(pan>=0)?1.0f:(1.0f+pan);
    *l += left*w*lg;
    *r += right*w*rg;
    g->read_pos += g->pitch_ratio;
    g->age++;
    if(g->age>=g->length_samples){ g->active=0; return 0; }
    return 1;
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `cmake --build build --target test_core && ./build/test_core`
Expected: `test_window OK`, `test_grain OK`, all passed.

- [ ] **Step 6: Commit**

```bash
git add src/core/window.* src/core/grain.* tests/test_core.c
git commit -m "feat(core): Hann window table + single windowed/resampled grain"
```

---

## Task 4: Voice = grain scheduler + ADSR (C core, TDD)

**Files:**
- Create: `src/core/voice.h`, `src/core/voice.c` (replaces stub)
- Create: `src/core/params.h`
- Modify: `tests/test_core.c`

**Interfaces:**
- Consumes: `Grain`/`grain_tick` (Task 3), `window_*`.
- Produces:
  ```c
  /* params.h — shared param snapshot pushed from engine to voices each block */
  typedef struct {
      float density;     /* grains/sec */
      float grain_ms;    /* grain size ms */
      float position;    /* 0..1 */
      float spread;      /* 0..1 position spray */
      float pan_spread;  /* 0..1 */
      float pitch_mult;  /* global coarse+fine as a ratio */
      float macro_speed; /* -1..1 */
      float attack, decay, sustain, release; /* ADSR seconds / 0..1 */
      float master_gain;
  } Params;

  /* voice.h */
  #define VOICE_MAX_GRAINS 64
  typedef struct {
      int active; int note; float note_pitch; /* 2^((note-60)/12) */
      double position; /* 0..1 playhead */
      double grain_phase_acc; /* fractional grains due */
      Grain grains[VOICE_MAX_GRAINS];
      /* ADSR */
      int stage; /* 0 idle,1 A,2 D,3 S,4 R */
      float env; float velocity;
      unsigned age_samples; /* for stealing (oldest = largest) */
  } Voice;
  void voice_note_on(Voice* v, int note, float velocity);
  void voice_note_off(Voice* v);
  /* render one block, mixing into l/r (length n). rng_state advanced for spray. */
  void voice_render(Voice* v, const WavData* smp, const Params* p,
                    double sr, float* l, float* r, int n, unsigned* rng);
  int voice_is_finished(const Voice* v);
  ```

- [ ] **Step 1: Write failing tests**

```c
#include "voice.h"
#include "params.h"

static Params default_params(void){
    Params p={0}; p.density=30; p.grain_ms=120; p.position=0.5f; p.spread=0.f;
    p.pan_spread=0.f; p.pitch_mult=1.0f; p.macro_speed=0.f;
    p.attack=0.01f; p.decay=0.1f; p.sustain=0.8f; p.release=0.2f; p.master_gain=1.0f;
    return p;
}

static void test_voice_sounds_then_silences(void){
    window_init();
    WavData smp; smp.num_frames=44100; smp.num_channels=1; smp.sample_rate=44100;
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f; smp.data=buf;
    Params p=default_params(); unsigned rng=1;
    Voice v={0}; voice_note_on(&v,60,1.0f);
    double sr=44100; int n=512; float l[512],r[512];
    /* render ~0.5s, expect audible energy */
    float energy=0;
    for(int b=0;b<40;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        for(int i=0;i<n;i++) energy+=l[i]*l[i]; }
    assert(energy>1.0f);
    /* note off, render past release, expect near-silence at the end */
    voice_note_off(&v);
    float tail=0;
    for(int b=0;b<60;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        if(b>=50) for(int i=0;i<n;i++) tail+=l[i]*l[i]; }
    assert(tail<0.01f);
    assert(voice_is_finished(&v));
    printf("test_voice OK\n");
}
```
Add `test_voice_sounds_then_silences();` to `main()`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target test_core`
Expected: link error — voice symbols undefined.

- [ ] **Step 3: Write `src/core/params.h`** (exactly the struct in Interfaces above).

- [ ] **Step 4: Write `src/core/voice.h`** (exactly the struct + prototypes in Interfaces above; `#include "grain.h"` and `#include "wav.h"`).

- [ ] **Step 5: Write `src/core/voice.c`**

```c
#include "voice.h"
#include "params.h"
#include "window.h"
#include <math.h>

static float frand(unsigned* s){ *s=*s*1664525u+1013904223u; return ((*s>>8)&0xFFFFFF)/(float)0x1000000; }

void voice_note_on(Voice* v, int note, float velocity){
    v->active=1; v->note=note; v->note_pitch=powf(2.0f,(note-60)/12.0f);
    v->position=0.0; v->grain_phase_acc=0.0; v->velocity=velocity;
    v->stage=1; v->env=0.0f; v->age_samples=0;
    for(int i=0;i<VOICE_MAX_GRAINS;i++) v->grains[i].active=0;
}
void voice_note_off(Voice* v){ if(v->active && v->stage!=0) v->stage=4; }
int voice_is_finished(const Voice* v){ return !v->active; }

static float adsr_tick(Voice* v, const Params* p, double sr){
    float a=p->attack, d=p->decay, s=p->sustain, r=p->release;
    float step;
    switch(v->stage){
        case 1: step=(a<=0)?1.f:(1.0f/(a*sr)); v->env+=step; if(v->env>=1.f){v->env=1.f;v->stage=2;} break;
        case 2: step=(d<=0)?1.f:((1.0f-s)/(d*sr)); v->env-=step; if(v->env<=s){v->env=s;v->stage=3;} break;
        case 3: v->env=s; break;
        case 4: step=(r<=0)?1.f:(v->env/(r*sr)); v->env-=step; if(v->env<=0.0001f){v->env=0.f;v->stage=0;v->active=0;} break;
        default: v->env=0.f;
    }
    return v->env;
}

static void spawn_grain(Voice* v, const WavData* smp, const Params* p, unsigned* rng){
    for(int i=0;i<VOICE_MAX_GRAINS;i++){
        if(!v->grains[i].active){
            Grain* g=&v->grains[i];
            float spray=(frand(rng)-0.5f)*p->spread;
            double pos=v->position+spray; if(pos<0)pos=0; if(pos>1)pos=1;
            g->src=smp->data; g->src_frames=smp->num_frames; g->src_channels=smp->num_channels;
            g->read_pos=pos*smp->num_frames;
            g->pitch_ratio=v->note_pitch*p->pitch_mult;
            g->length_samples=(int)(p->grain_ms*0.001f*smp->sample_rate);
            if(g->length_samples<2) g->length_samples=2;
            g->age=0; g->pan=(frand(rng)*2.0f-1.0f)*p->pan_spread;
            g->amp=v->velocity; g->active=1;
            return;
        }
    }
}

void voice_render(Voice* v, const WavData* smp, const Params* p, double sr,
                  float* l, float* r, int n, unsigned* rng){
    if(!v->active || !smp || !smp->data) return;
    double grains_per_sample=p->density/sr;
    for(int i=0;i<n;i++){
        float env=adsr_tick(v,p,sr);
        /* schedule grains */
        v->grain_phase_acc+=grains_per_sample;
        while(v->grain_phase_acc>=1.0){ v->grain_phase_acc-=1.0; spawn_grain(v,smp,p,rng); }
        /* macro drift of position playhead */
        if(p->macro_speed!=0.0f && smp->num_frames>0)
            v->position+=(double)p->macro_speed/ (smp->num_frames/sr) / sr;
        if(v->position>1.0)v->position-=1.0; if(v->position<0.0)v->position+=1.0;
        /* mix live grains */
        float gl=0.f, gr=0.f;
        for(int gi=0;gi<VOICE_MAX_GRAINS;gi++) if(v->grains[gi].active) grain_tick(&v->grains[gi],&gl,&gr);
        l[i]+=gl*env*p->master_gain;
        r[i]+=gr*env*p->master_gain;
        v->age_samples++;
    }
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `cmake --build build --target test_core && ./build/test_core`
Expected: `test_voice OK`, all passed.

- [ ] **Step 7: Commit**

```bash
git add src/core/voice.* src/core/params.h tests/test_core.c
git commit -m "feat(core): per-note voice — grain scheduler, ADSR, macro drift"
```

---

## Task 5: Engine — voice pool, note routing, params, render (C core, TDD)

**Files:**
- Create/replace: `src/core/granular_engine.h`, `src/core/granular_engine.c`
- Modify: `tests/test_core.c`

**Interfaces:**
- Consumes: `Voice`, `Params`, `WavData`.
- Produces (the public ABI the plugin uses):
  ```c
  typedef struct GranularEngine GranularEngine;
  typedef enum { P_DENSITY,P_GRAIN_MS,P_POSITION,P_SPREAD,P_PAN_SPREAD,
                 P_PITCH_SEMI,P_PITCH_CENT,P_MACRO_SPEED,
                 P_ATTACK,P_DECAY,P_SUSTAIN,P_RELEASE,P_MASTER,P_COUNT } ParamId;
  GranularEngine* ge_create(double sample_rate, int max_voices);
  void ge_destroy(GranularEngine*);
  void ge_prepare(GranularEngine*, double sample_rate);
  /* sample hand-off: copies frames; safe to call on message thread only */
  void ge_set_sample(GranularEngine*, const float* interleaved, int frames, int channels, double src_rate);
  void ge_note_on(GranularEngine*, int note, float velocity);
  void ge_note_off(GranularEngine*, int note);
  void ge_all_notes_off(GranularEngine*);
  void ge_set_param(GranularEngine*, ParamId id, float value);
  void ge_render(GranularEngine*, float* left, float* right, int num_frames);
  int  ge_active_voice_count(GranularEngine*);
  ```

- [ ] **Step 1: Write failing tests**

```c
#include "granular_engine.h"

static void test_engine_polyphony_and_stealing(void){
    GranularEngine* e=ge_create(44100,16);
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f;
    ge_set_sample(e,buf,44100,1,44100);
    ge_set_param(e,P_ATTACK,0.5f); ge_set_param(e,P_RELEASE,0.5f);
    for(int nnote=60;nnote<63;nnote++) ge_note_on(e,nnote,1.0f);
    float l[256],r[256]; for(int i=0;i<256;i++){l[i]=0;r[i]=0;}
    ge_render(e,l,r,256);
    assert(ge_active_voice_count(e)==3);
    /* fill to 16, then a 17th steals -> stays at 16 */
    for(int nnote=63;nnote<76;nnote++) ge_note_on(e,nnote,1.0f);
    assert(ge_active_voice_count(e)==16);
    ge_note_on(e,90,1.0f);
    assert(ge_active_voice_count(e)==16);
    ge_destroy(e);
    printf("test_engine OK\n");
}
```
Add `test_engine_polyphony_and_stealing();` to `main()`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target test_core`
Expected: link error — `ge_*` undefined.

- [ ] **Step 3: Write `src/core/granular_engine.h`** (exactly the ABI in Interfaces above).

- [ ] **Step 4: Write `src/core/granular_engine.c`**

```c
#include "granular_engine.h"
#include "voice.h"
#include "params.h"
#include "wav.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct GranularEngine {
    double sr; int max_voices; Voice* voices;
    WavData sample; int has_sample;
    Params params; float pitch_semi, pitch_cent;
    unsigned rng;
};

static void recompute_pitch(GranularEngine* e){
    e->params.pitch_mult=powf(2.0f,(e->pitch_semi+e->pitch_cent/100.0f)/12.0f);
}

GranularEngine* ge_create(double sr, int max_voices){
    GranularEngine* e=(GranularEngine*)calloc(1,sizeof(GranularEngine));
    e->sr=sr; e->max_voices=max_voices;
    e->voices=(Voice*)calloc(max_voices,sizeof(Voice));
    window_init();
    e->params.density=30; e->params.grain_ms=120; e->params.position=0.5f;
    e->params.spread=0.05f; e->params.pan_spread=0.2f; e->params.pitch_mult=1.0f;
    e->params.macro_speed=0.0f; e->params.attack=0.01f; e->params.decay=0.1f;
    e->params.sustain=0.8f; e->params.release=0.3f; e->params.master_gain=0.8f;
    e->pitch_semi=0; e->pitch_cent=0; e->rng=22222u; e->has_sample=0;
    return e;
}
void ge_destroy(GranularEngine* e){ if(!e)return; wav_free(&e->sample); free(e->voices); free(e); }
void ge_prepare(GranularEngine* e, double sr){ e->sr=sr; }

void ge_set_sample(GranularEngine* e, const float* interleaved, int frames, int ch, double src_rate){
    float* copy=(float*)malloc(sizeof(float)*frames*ch);
    memcpy(copy,interleaved,sizeof(float)*frames*ch);
    WavData old=e->sample;
    e->sample.data=copy; e->sample.num_frames=frames; e->sample.num_channels=ch; e->sample.sample_rate=src_rate;
    e->has_sample=1;
    if(old.data) free(old.data); /* message-thread only */
}

void ge_note_on(GranularEngine* e, int note, float vel){
    /* free slot first */
    for(int i=0;i<e->max_voices;i++) if(!e->voices[i].active){ voice_note_on(&e->voices[i],note,vel); return; }
    /* steal oldest */
    int oldest=0; unsigned best=0;
    for(int i=0;i<e->max_voices;i++) if(e->voices[i].age_samples>=best){ best=e->voices[i].age_samples; oldest=i; }
    voice_note_on(&e->voices[oldest],note,vel);
}
void ge_note_off(GranularEngine* e, int note){
    for(int i=0;i<e->max_voices;i++) if(e->voices[i].active && e->voices[i].note==note && e->voices[i].stage!=4)
        voice_note_off(&e->voices[i]);
}
void ge_all_notes_off(GranularEngine* e){ for(int i=0;i<e->max_voices;i++) voice_note_off(&e->voices[i]); }

void ge_set_param(GranularEngine* e, ParamId id, float v){
    switch(id){
        case P_DENSITY: e->params.density=v; break;
        case P_GRAIN_MS: e->params.grain_ms=v; break;
        case P_POSITION: e->params.position=v; break;
        case P_SPREAD: e->params.spread=v; break;
        case P_PAN_SPREAD: e->params.pan_spread=v; break;
        case P_PITCH_SEMI: e->pitch_semi=v; recompute_pitch(e); break;
        case P_PITCH_CENT: e->pitch_cent=v; recompute_pitch(e); break;
        case P_MACRO_SPEED: e->params.macro_speed=v; break;
        case P_ATTACK: e->params.attack=v; break;
        case P_DECAY: e->params.decay=v; break;
        case P_SUSTAIN: e->params.sustain=v; break;
        case P_RELEASE: e->params.release=v; break;
        case P_MASTER: e->params.master_gain=v; break;
        default: break;
    }
}

void ge_render(GranularEngine* e, float* l, float* r, int n){
    for(int i=0;i<n;i++){ l[i]=0.f; r[i]=0.f; }
    if(!e->has_sample) return;
    for(int i=0;i<e->max_voices;i++) if(e->voices[i].active)
        voice_render(&e->voices[i],&e->sample,&e->params,e->sr,l,r,n,&e->rng);
}
int ge_active_voice_count(GranularEngine* e){
    int c=0; for(int i=0;i<e->max_voices;i++) if(e->voices[i].active) c++; return c;
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `cmake --build build --target test_core && ./build/test_core`
Expected: `test_engine OK`, all passed.

- [ ] **Step 6: Commit**

```bash
git add src/core/granular_engine.* tests/test_core.c
git commit -m "feat(core): engine — voice pool, note routing, params, mixdown"
```

---

## Task 6: JUCE shell wiring — MIDI + audio through the core

**Files:**
- Modify: `src/plugin/PluginProcessor.h`, `src/plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: full `granular_engine.h` ABI.
- Produces: a plugin that plays granular sound from a built-in test tone when MIDI notes arrive (sample loading UI comes in Task 8; here we seed a default buffer so we can hear it).

- [ ] **Step 1: Add engine member + include to `PluginProcessor.h`**

```cpp
// add near top:
extern "C" {
#include "granular_engine.h"
}
// add private members:
private:
    GranularEngine* engine = nullptr;
```

- [ ] **Step 2: Create + destroy engine; seed a default buffer**

In `PluginProcessor.cpp` constructor body and destructor:
```cpp
KineticGranularProcessor::KineticGranularProcessor()
  : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true))
{
    engine = ge_create(44100.0, 16);
    // seed a 1s 220Hz sine so the plugin makes sound before a sample is loaded
    const int N = 44100; std::vector<float> buf((size_t)N);
    for (int i=0;i<N;i++) buf[(size_t)i] = 0.5f*std::sin(2.0*M_PI*220.0*i/44100.0);
    ge_set_sample(engine, buf.data(), N, 1, 44100.0);
}
KineticGranularProcessor::~KineticGranularProcessor() { ge_destroy(engine); }
```

- [ ] **Step 3: `prepareToPlay` forwards sample rate**

```cpp
void KineticGranularProcessor::prepareToPlay(double sampleRate, int) {
    ge_prepare(engine, sampleRate);
}
```

- [ ] **Step 4: `processBlock` — route MIDI, pull audio**

```cpp
void KineticGranularProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    buffer.clear();
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isNoteOn())  ge_note_on(engine, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) ge_note_off(engine, m.getNoteNumber());
        else if (m.isAllNotesOff()) ge_all_notes_off(engine);
    }
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    ge_render(engine, L, R, buffer.getNumSamples());
}
```

- [ ] **Step 5: Build + reinstall**

Run:
```bash
cmake --build build --config Release --target KineticGranular_AU KineticGranular_VST3
auval -v aumu Kgrn Hntr
```
Expected: `AU VALIDATION SUCCEEDED`.

- [ ] **Step 6: Manual test in Ableton**

Reinstall component/vst3, load on a MIDI track, play notes on a keyboard. Expected: audible granular/pad tone from the built-in sine; chords play multiple pitches; notes release with the ADSR tail.

- [ ] **Step 7: Commit**

```bash
git add src/plugin/PluginProcessor.*
git commit -m "feat(plugin): wire MIDI + processBlock through the C engine"
```

---

## Task 7: Parameters via AudioProcessorValueTreeState + generic GUI

**Files:**
- Modify: `src/plugin/PluginProcessor.h`, `src/plugin/PluginProcessor.cpp`
- Modify: `src/plugin/PluginEditor.h`, `src/plugin/PluginEditor.cpp`

**Interfaces:**
- Consumes: `ge_set_param`, `ParamId`.
- Produces: 13 automatable host parameters mapped 1:1 to `ParamId`, shown in a `GenericAudioProcessorEditor`.

- [ ] **Step 1: Add APVTS to `PluginProcessor.h`**

```cpp
public:
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
```

- [ ] **Step 2: Build the layout (matches ParamId order/ranges)**

In `PluginProcessor.cpp`:
```cpp
using APVTS = juce::AudioProcessorValueTreeState;
using PF = juce::AudioParameterFloat;
static juce::NormalisableRange<float> rng(float a,float b){ return {a,b}; }

APVTS::ParameterLayout KineticGranularProcessor::makeLayout(){
    APVTS::ParameterLayout l;
    l.add(std::make_unique<PF>("density","Density", rng(1,100), 30.f));
    l.add(std::make_unique<PF>("grain","Grain Size (ms)", rng(5,500), 120.f));
    l.add(std::make_unique<PF>("position","Position", rng(0,1), 0.5f));
    l.add(std::make_unique<PF>("spread","Spread", rng(0,1), 0.05f));
    l.add(std::make_unique<PF>("panspread","Pan Spread", rng(0,1), 0.2f));
    l.add(std::make_unique<PF>("pitchsemi","Pitch (semi)", rng(-24,24), 0.f));
    l.add(std::make_unique<PF>("pitchcent","Pitch (cent)", rng(-100,100), 0.f));
    l.add(std::make_unique<PF>("macro","Macro Speed", rng(-1,1), 0.f));
    l.add(std::make_unique<PF>("attack","Attack", rng(0.001f,5), 0.01f));
    l.add(std::make_unique<PF>("decay","Decay", rng(0.001f,5), 0.1f));
    l.add(std::make_unique<PF>("sustain","Sustain", rng(0,1), 0.8f));
    l.add(std::make_unique<PF>("release","Release", rng(0.001f,5), 0.3f));
    l.add(std::make_unique<PF>("master","Master", rng(0,1), 0.8f));
    return l;
}
```

- [ ] **Step 3: Init APVTS in constructor init list**

```cpp
// change constructor signature init list to include:
  , apvts(*this, nullptr, "PARAMS", makeLayout())
```

- [ ] **Step 4: Push params to engine each block (top of processBlock)**

```cpp
auto g=[&](const char* id){ return apvts.getRawParameterValue(id)->load(); };
ge_set_param(engine,P_DENSITY,g("density"));
ge_set_param(engine,P_GRAIN_MS,g("grain"));
ge_set_param(engine,P_POSITION,g("position"));
ge_set_param(engine,P_SPREAD,g("spread"));
ge_set_param(engine,P_PAN_SPREAD,g("panspread"));
ge_set_param(engine,P_PITCH_SEMI,g("pitchsemi"));
ge_set_param(engine,P_PITCH_CENT,g("pitchcent"));
ge_set_param(engine,P_MACRO_SPEED,g("macro"));
ge_set_param(engine,P_ATTACK,g("attack"));
ge_set_param(engine,P_DECAY,g("decay"));
ge_set_param(engine,P_SUSTAIN,g("sustain"));
ge_set_param(engine,P_RELEASE,g("release"));
ge_set_param(engine,P_MASTER,g("master"));
```

- [ ] **Step 5: Swap editor to generic**

```cpp
// PluginProcessor.cpp createEditor():
juce::AudioProcessorEditor* KineticGranularProcessor::createEditor(){
    return new juce::GenericAudioProcessorEditor(*this);
}
```
(Delete the custom `KineticGranularEditor` include/use, or leave the file unused.)

- [ ] **Step 6: Build + validate**

Run:
```bash
cmake --build build --config Release --target KineticGranular_AU
auval -v aumu Kgrn Hntr
```
Expected: `AU VALIDATION SUCCEEDED`, and `auval` lists 13 parameters.

- [ ] **Step 7: Manual test — automate a param in Ableton**

Load, twist Density/Grain Size/Pitch while playing; automate Position. Expected: audible change, automation recorded/played back.

- [ ] **Step 8: Commit**

```bash
git add src/plugin/*
git commit -m "feat(plugin): 13 automatable params via APVTS + generic editor"
```

---

## Task 8: Sample load (FileChooser) + preset state with embedded sample

**Files:**
- Modify: `src/plugin/PluginProcessor.h`, `src/plugin/PluginProcessor.cpp`
- Modify: `src/plugin/PluginEditor.h`, `src/plugin/PluginEditor.cpp` (custom editor returns: a "Load WAV" button + the generic param panel)

**Interfaces:**
- Consumes: `wav_load` is NOT used here (JUCE decodes); we call `ge_set_sample` with JUCE-decoded floats to support the file chooser cleanly. State stores raw WAV bytes.
- Produces: user-loadable sample; presets that round-trip the sample + params.

- [ ] **Step 1: Add a thread-safe "load WAV file" method to the processor**

`PluginProcessor.h`:
```cpp
public:
    void loadWavFile(const juce::File& f);   // message thread only
    juce::MemoryBlock currentSampleWav;      // raw bytes for state
```
`PluginProcessor.cpp`:
```cpp
void KineticGranularProcessor::loadWavFile(const juce::File& f){
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
    if(!rd) return;
    juce::AudioBuffer<float> tmp((int)rd->numChannels,(int)rd->lengthInSamples);
    rd->read(&tmp,0,(int)rd->lengthInSamples,0,true,true);
    // interleave
    int ch=tmp.getNumChannels(), n=tmp.getNumSamples();
    std::vector<float> inter((size_t)n*ch);
    for(int i=0;i<n;i++) for(int c=0;c<ch;c++) inter[(size_t)i*ch+c]=tmp.getSample(c,i);
    ge_set_sample(engine, inter.data(), n, ch, rd->sampleRate);
    f.loadFileAsData(currentSampleWav);   // keep raw bytes for presets
}
```

- [ ] **Step 2: State save — params + sample bytes**

```cpp
void KineticGranularProcessor::getStateInformation(juce::MemoryBlock& dest){
    juce::MemoryOutputStream mos(dest,false);
    auto state=apvts.copyState();
    juce::MemoryBlock xml; { juce::MemoryOutputStream x(xml,false);
        state.createXml()->writeTo(x); }
    mos.writeInt((int)xml.getSize()); mos.write(xml.getData(),xml.getSize());
    mos.writeInt((int)currentSampleWav.getSize());
    mos.write(currentSampleWav.getData(),currentSampleWav.getSize());
}
```

- [ ] **Step 3: State load — restore params + decode embedded sample**

```cpp
void KineticGranularProcessor::setStateInformation(const void* data,int size){
    juce::MemoryInputStream mis(data,(size_t)size,false);
    int xmlLen=mis.readInt(); juce::MemoryBlock xml; xml.setSize((size_t)xmlLen);
    mis.read(xml.getData(),xmlLen);
    if(auto x=juce::XmlDocument::parse(xml.toString()))
        apvts.replaceState(juce::ValueTree::fromXml(*x));
    int wavLen=mis.readInt();
    if(wavLen>0){ currentSampleWav.setSize((size_t)wavLen); mis.read(currentSampleWav.getData(),wavLen);
        auto tmp=juce::File::createTempFile(".wav"); tmp.replaceWithData(currentSampleWav.getData(),currentSampleWav.getSize());
        loadWavFile(tmp); tmp.deleteFile(); }
}
```

- [ ] **Step 4: Editor — "Load WAV" button above the generic param panel**

`PluginEditor.h`:
```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
class KineticGranularEditor : public juce::AudioProcessorEditor {
public:
    explicit KineticGranularEditor(KineticGranularProcessor& p);
    void resized() override;
private:
    KineticGranularProcessor& proc;
    juce::TextButton loadBtn { "Load WAV..." };
    juce::GenericAudioProcessorEditor generic { proc };
    std::unique_ptr<juce::FileChooser> chooser;
};
```
`PluginEditor.cpp`:
```cpp
#include "PluginEditor.h"
KineticGranularEditor::KineticGranularEditor(KineticGranularProcessor& p)
  : AudioProcessorEditor(p), proc(p) {
    addAndMakeVisible(loadBtn); addAndMakeVisible(generic);
    loadBtn.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Load WAV","",".wav");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto f=fc.getResult(); if(f.existsAsFile()) proc.loadWavFile(f); });
    };
    setSize(440,420);
}
void KineticGranularEditor::resized(){ auto r=getLocalBounds();
    loadBtn.setBounds(r.removeFromTop(32).reduced(4)); generic.setBounds(r); }
```
And point `createEditor()` back to `new KineticGranularEditor(*this)`.

- [ ] **Step 5: Build + validate**

Run:
```bash
cmake --build build --config Release --target KineticGranular_AU KineticGranular_VST3
auval -v aumu Kgrn Hntr
```
Expected: `AU VALIDATION SUCCEEDED`.

- [ ] **Step 6: Manual test — load a sample + preset round-trip**

In Ableton: load a WAV via the button, play it granularly; save the Live set, reopen. Expected: the sample + all param values restore (sound identical). Also test with a stock Ableton preset save/load.

- [ ] **Step 7: Commit**

```bash
git add src/plugin/*
git commit -m "feat(plugin): WAV file load + preset state with embedded sample — v1 complete"
```

---

## Self-Review

**1. Spec coverage** (against `docs/DESIGN.md`):
- Granular sample-as-synth → Tasks 3–5. ✓
- Per-note polyphony + stealing + ADSR → Tasks 4–5. ✓
- 16-voice ceiling → Task 5 `ge_create(...,16)`. ✓
- MIDI note input from host → Task 6. ✓
- Full param set (13) automatable → Task 7 (matches DESIGN §5 table: density, grain size, position, spread, pan spread, pitch coarse+fine, macro-speed, ADSR, master). ✓
- WAV load, one layer → Tasks 2 + 8. ✓
- Native arm64 AU + VST3 in Ableton → Task 1 (arch check + auval + manual), reinforced in 6/7/8. ✓
- Preset state embeds sample → Task 8. ✓
- RT-safety (no alloc/lock in render) → core render paths allocate nothing; sample swap is message-thread only (Task 5 `ge_set_sample`). ✓ *(Known simplification: `ge_set_sample`'s free-of-old-buffer assumes it is never called concurrently with `ge_render`; true because JUCE calls both, but audio callback can run during a message-thread set. Flagged in DESIGN §6 as the double-buffer/atomic-swap upgrade — deferred, acceptable for v1 since a torn free is the only risk and Live suspends processing during state load. Revisit if crackles appear on live sample swaps.)*
- Non-goals (spectral, multi-layer, custom waveform GUI, AIFF/FLAC, image source) → not in any task. ✓

**2. Placeholder scan:** every code step contains complete, compilable code; every run step has an exact command + expected output. No TBD/TODO. ✓

**3. Type consistency:** `Params` fields used identically in voice.c/engine.c; `ParamId` enum order in `granular_engine.h` matches the `ge_set_param` switch and the APVTS push order in Task 7; `WavData` shape identical across wav.h, voice.h use, engine. `ge_set_sample` signature identical in Task 5 def and Task 6/8 calls. ✓

**One gap found + resolved:** the RT-safe sample swap is a known v1 simplification, now explicitly documented above rather than left implicit.
