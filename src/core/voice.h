#pragma once
#include "grain.h"
#include "wav.h"
#include "params.h"

#define VOICE_MAX_GRAINS 64
typedef struct {
    int active; int note; float note_pitch; /* 2^((note-60)/12) */
    double position; /* 0..1 playhead */
    double grain_phase_acc; /* fractional grains due */
    Grain grains[VOICE_MAX_GRAINS];
    /* ADSR */
    int stage; /* 0 idle,1 A,2 D,3 S,4 R */
    float env; float velocity;
    float release_start_env; /* env captured at moment release begins */
    unsigned age_samples; /* for stealing (oldest = largest) */
} Voice;

void voice_note_on(Voice* v, int note, float velocity);
void voice_note_off(Voice* v);
/* render one block, mixing into l/r (length n). rng_state advanced for spray. */
void voice_render(Voice* v, const WavData* smp, const Params* p,
                  double sr, float* l, float* r, int n, unsigned* rng);
int voice_is_finished(const Voice* v);
