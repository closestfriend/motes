#pragma once
typedef struct {
    const float* src; int src_frames; int src_channels;
    double read_pos; double pitch_ratio;
    int length_samples; int age;
    float pan; float amp; int active;
} Grain;
int grain_tick(Grain* g, float* l, float* r);
