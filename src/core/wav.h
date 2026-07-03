#pragma once
typedef struct { float* data; int num_frames; int num_channels; double sample_rate; } WavData;

/* wav_load return codes: 0 = ok, <0 = error.
 *  -1  fopen failed
 *  -2  file smaller than a minimal RIFF/WAVE header (44 bytes)
 *  -3  malloc failed (file buffer)
 *  -4  short read (file changed size / IO error)
 *  -5  not a RIFF/WAVE file
 *  -6  no "data" chunk found, or num_channels == 0
 *  -7  malloc failed (decoded sample buffer)
 *  -8  a chunk's declared length runs past the end of the file
 *      (malformed/truncated input)
 *  -9  "fmt " chunk is shorter than the 16 bytes required to read
 *      format/channels/rate/bits
 * -10  bits-per-sample is not one of {8,16,24,32}
 * -11  (format, bits) combination is not a supported decode path
 *      (only PCM 8/16/24 and IEEE float 32 are supported)
 */
int wav_load(const char* path, WavData* out);
void wav_free(WavData* w);
