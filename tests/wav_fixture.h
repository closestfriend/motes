#pragma once
/* Writes a mono PCM16 WAV: `frames` samples of value `amp` (constant). Returns 0 ok. */
int write_test_wav(const char* path, int frames, int sample_rate, short amp);
