#pragma once

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
float    ge_get_out_rms(GranularEngine*);
unsigned ge_get_grains_fired(GranularEngine*);
