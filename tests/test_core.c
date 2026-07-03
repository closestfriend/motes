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
    remove(p);
    printf("test_wav_load OK\n");
}

static void test_wav_load_24(void){
    const char* p="/tmp/kg_fixture_24.wav";
    /* -4194304 == -0.5 * 2^23 -- exercises the sign-extension decode path. */
    assert(write_test_wav_24(p, 50, 48000, -4194304)==0);
    WavData w;
    assert(wav_load(p,&w)==0);
    assert(w.num_frames==50);
    assert(w.num_channels==1);
    assert(fabs(w.sample_rate-48000.0)<1e-6);
    assert(fabsf(w.data[0]-(-0.5f))<0.0005f);
    wav_free(&w);
    remove(p);
    printf("test_wav_load_24 OK\n");
}

static void test_wav_load_f32(void){
    const char* p="/tmp/kg_fixture_f32.wav";
    assert(write_test_wav_f32(p, 64, 44100, 0.25f)==0);
    WavData w;
    assert(wav_load(p,&w)==0);
    assert(w.num_frames==64);
    assert(w.num_channels==1);
    assert(fabs(w.sample_rate-44100.0)<1e-6);
    /* float32 samples round-trip exactly through wav_load. */
    assert(fabsf(w.data[0]-0.25f)<1e-6f);
    wav_free(&w);
    remove(p);
    printf("test_wav_load_f32 OK\n");
}

static void test_wav_load_malformed_truncated(void){
    const char* p="/tmp/kg_fixture_truncated.wav";
    assert(write_truncated_wav(p, 200, 44100)==0);
    WavData w;
    int rc = wav_load(p,&w);
    assert(rc<0);
    remove(p);
    printf("test_wav_load_malformed_truncated OK\n");
}

int main(void){
    test_wav_load();
    test_wav_load_24();
    test_wav_load_f32();
    test_wav_load_malformed_truncated();
    printf("ALL CORE TESTS PASSED\n");
    return 0;
}
