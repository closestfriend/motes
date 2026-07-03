#pragma once
/* Writes a mono PCM16 WAV: `frames` samples of value `amp` (constant). Returns 0 ok. */
int write_test_wav(const char* path, int frames, int sample_rate, short amp);

/* Writes a mono PCM24 WAV: `frames` samples of 24-bit signed value `amp`
   (valid range +-8388607). Use a negative amp to exercise the decoder's
   sign-extension path. Returns 0 ok. */
int write_test_wav_24(const char* path, int frames, int sample_rate, int amp);

/* Writes a mono IEEE-float WAV (fmt==3, bits==32): `frames` samples of
   value `amp`. Returns 0 ok. */
int write_test_wav_f32(const char* path, int frames, int sample_rate, float amp);

/* Writes a malformed/truncated WAV: the RIFF/fmt/data chunk headers claim a
   `data` chunk of `frames` PCM16 samples, but only half that many sample
   bytes are actually written to disk before the file ends. Exercises
   wav_load's bounds checking against the true file size. Returns 0 ok. */
int write_truncated_wav(const char* path, int frames, int sample_rate);
