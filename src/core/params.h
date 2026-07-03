#pragma once
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
