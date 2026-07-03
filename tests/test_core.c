#include "wav.h"
#include "wav_fixture.h"
#include "window.h"
#include "grain.h"
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

static void test_window(void){
    window_init();
    assert(window_hann(0.0f) < 0.001f);
    assert(window_hann(1.0f) < 0.001f);
    assert(window_hann(0.5f) > 0.999f);
    printf("test_window OK\n");
}

static void test_grain_length_and_taper(void){
    window_init();
    float src[1000]; for(int i=0;i<1000;i++) src[i]=1.0f; /* DC = 1.0 */
    Grain g={0}; g.src=src; g.src_frames=1000; g.src_channels=1;
    g.read_pos=0; g.pitch_ratio=1.0; g.length_samples=100; g.age=0;
    g.pan=0.0f; g.amp=1.0f; g.active=1;
    int ticks=0; float first_l=0,first_r=0,mid_l=0,mid_r=0;
    for(int i=0;i<200;i++){
        float l=0,r=0; int alive=grain_tick(&g,&l,&r);
        if(i==0){ first_l=l; first_r=r; }
        if(i==50){ mid_l=l; mid_r=r; }
        if(alive) ticks++; else break;
    }
    assert(ticks==100);                 /* runs exactly length_samples */
    assert(fabsf(first_l)<0.05f);       /* window ~0 at start */
    assert(mid_l>0.6f);                 /* window near peak mid-grain */
    printf("test_grain OK\n");
}

int main(void){
    test_wav_load();
    test_wav_load_24();
    test_wav_load_f32();
    test_wav_load_malformed_truncated();
    test_window();
    test_grain_length_and_taper();
    printf("ALL CORE TESTS PASSED\n");
    return 0;
}
