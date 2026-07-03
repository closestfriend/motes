#pragma once
typedef struct { float* data; int num_frames; int num_channels; double sample_rate; } WavData;
int wav_load(const char* path, WavData* out);
void wav_free(WavData* w);
