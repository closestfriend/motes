#include "granular_engine.h"
#include "voice.h"
#include "params.h"
#include "wav.h"
#include "window.h"
#include "event_ring.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

struct GranularEngine {
    double sr; int max_voices; Voice* voices;
    WavData sample; int has_sample;
    Params params; float pitch_semi, pitch_cent;
    unsigned rng;
    _Atomic float out_rms;
    atomic_uint grains_fired;
    GrainEventRing events;
    _Atomic float* playheads;   /* max_voices entries, -1 = inactive */
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
    atomic_init(&e->out_rms, 0.0f);
    atomic_init(&e->grains_fired, 0u);
    ger_init(&e->events);
    e->playheads=(_Atomic float*)calloc(max_voices,sizeof(_Atomic float));
    for(int i=0;i<max_voices;i++){ atomic_init(&e->playheads[i],-1.0f); e->voices[i].events=&e->events; }
    return e;
}
void ge_destroy(GranularEngine* e){ if(!e)return; wav_free(&e->sample); free(e->voices); free((void*)e->playheads); free(e); }
void ge_prepare(GranularEngine* e, double sr){ e->sr=sr; }

void ge_set_sample(GranularEngine* e, const float* interleaved, int frames, int ch, double src_rate){
    /* Reached both from state-load (host suspended) and the editor's "Load
     * WAV" button on the message thread with NO suspension. Every spawned
     * Grain holds a raw pointer (g->src) into the CURRENT e->sample buffer.
     * Hard-reset all voices/grains before we free/replace that buffer so no
     * grain can dereference freed memory on the next audio callback. This
     * stops currently-sounding notes on sample load -- expected, and safe. */
    for(int i=0;i<e->max_voices;i++){
        Voice* v=&e->voices[i];
        v->active=0; v->stage=0;
        for(int gi=0;gi<VOICE_MAX_GRAINS;gi++) v->grains[gi].active=0;
    }
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
    unsigned fired=0;
    if(e->has_sample){
        for(int i=0;i<e->max_voices;i++) if(e->voices[i].active)
            fired += (unsigned)voice_render(&e->voices[i],&e->sample,&e->params,e->sr,l,r,n,&e->rng);
    }
    if(fired) atomic_fetch_add_explicit(&e->grains_fired, fired, memory_order_relaxed);
    double sumsq=0.0;
    for(int i=0;i<n;i++){ sumsq += (double)l[i]*l[i] + (double)r[i]*r[i]; }
    float rms = (n>0) ? (float)sqrt(sumsq/(2.0*n)) : 0.0f;
    atomic_store_explicit(&e->out_rms, rms, memory_order_relaxed);
    for(int i=0;i<e->max_voices;i++){
        float ph=e->voices[i].active?(float)voice_playhead(&e->voices[i],&e->params):-1.0f;
        atomic_store_explicit(&e->playheads[i],ph,memory_order_relaxed);
    }
}
int ge_active_voice_count(GranularEngine* e){
    int c=0; for(int i=0;i<e->max_voices;i++) if(e->voices[i].active) c++; return c;
}
float ge_get_out_rms(GranularEngine* e){ return atomic_load_explicit(&e->out_rms, memory_order_relaxed); }
unsigned ge_get_grains_fired(GranularEngine* e){ return atomic_load_explicit(&e->grains_fired, memory_order_relaxed); }

int ge_pop_grain_event(GranularEngine* e, GrainEvent* out){ return ger_pop(&e->events,out); }
int ge_get_playheads(GranularEngine* e, float* out, int max){
    int n=max<e->max_voices?max:e->max_voices;
    for(int i=0;i<n;i++) out[i]=atomic_load_explicit(&e->playheads[i],memory_order_relaxed);
    return n;
}
int ge_voice_playing_note(GranularEngine* e, int note){
    for(int i=0;i<e->max_voices;i++)
        if(e->voices[i].active && e->voices[i].note==note && e->voices[i].stage!=4) return 1;
    return 0;
}
