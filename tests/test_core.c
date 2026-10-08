#include "wav.h"
#include "wav_fixture.h"
#include "window.h"
#include "grain.h"
#include "voice.h"
#include "params.h"
#include "granular_engine.h"
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

static Params default_params(void){
    Params p={0}; p.density=30; p.grain_ms=120; p.position=0.5f; p.spread=0.f;
    p.pan_spread=0.f; p.pitch_mult=1.0f; p.macro_speed=0.f;
    p.attack=0.01f; p.decay=0.1f; p.sustain=0.8f; p.release=0.2f; p.master_gain=1.0f;
    return p;
}

static void test_voice_sounds_then_silences(void){
    window_init();
    WavData smp; smp.num_frames=44100; smp.num_channels=1; smp.sample_rate=44100;
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f; smp.data=buf;
    Params p=default_params(); unsigned rng=1;
    Voice v={0}; voice_note_on(&v,60,1.0f);
    double sr=44100; int n=512; float l[512],r[512];
    /* render ~0.5s, expect audible energy */
    float energy=0;
    for(int b=0;b<40;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        for(int i=0;i<n;i++) energy+=l[i]*l[i]; }
    assert(energy>1.0f);
    /* note off, render past release, expect near-silence at the end */
    voice_note_off(&v);
    float tail=0;
    for(int b=0;b<60;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        if(b>=50) for(int i=0;i<n;i++) tail+=l[i]*l[i]; }
    assert(tail<0.01f);
    assert(voice_is_finished(&v));
    printf("test_voice OK\n");
}

/* Regression: sustain=0 + note-off during attack must still terminate the
 * voice within ~release seconds. Before the fix, adsr_tick's release step
 * was s/(r*sr) with s re-read from p->sustain; with sustain==0 that step is
 * 0, so env freezes and the voice never becomes finished (zombie voice). */
static void test_voice_release_sustain_zero_during_attack(void){
    window_init();
    WavData smp; smp.num_frames=44100; smp.num_channels=1; smp.sample_rate=44100;
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f; smp.data=buf;
    Params p=default_params();
    p.attack=0.05f; p.decay=0.1f; p.sustain=0.f; p.release=0.1f;
    unsigned rng=1;
    Voice v={0}; voice_note_on(&v,60,1.0f);
    double sr=44100; int n=512; float l[512],r[512];
    /* render a couple blocks: well within the 0.05s (~2205 sample) attack */
    for(int b=0;b<2;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng); }
    assert(v.stage==1); /* still attacking, env>0 */
    assert(v.env>0.0001f);
    voice_note_off(&v);
    assert(v.stage==4); /* now releasing */
    /* bounded wait: release is 0.1s (~4410 samples) -> generous margin */
    int release_samples=(int)(p.release*sr+0.5);
    int max_blocks=(release_samples/n)+5;
    int finished=0;
    for(int b=0;b<max_blocks && !finished;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        if(voice_is_finished(&v)) finished=1; }
    assert(finished); /* zombie voice bug: this fails (never finishes) pre-fix */
    printf("test_voice_release_sustain_zero_during_attack OK\n");
}

/* Regression: small sustain (e.g. 0.001) + note-off near attack peak must
 * still terminate in roughly `release` seconds, not ~1/sustain times longer.
 * Before the fix, step = s/(r*sr) is computed from the tiny sustain value
 * even though env is near 1.0 at release onset, so it takes ~1/s times the
 * configured release duration (~1000x slower here). */
static void test_voice_release_small_sustain_near_peak(void){
    window_init();
    WavData smp; smp.num_frames=44100; smp.num_channels=1; smp.sample_rate=44100;
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f; smp.data=buf;
    Params p=default_params();
    p.attack=0.05f; p.decay=0.1f; p.sustain=0.001f; p.release=0.1f;
    unsigned rng=1;
    Voice v={0}; voice_note_on(&v,60,1.0f);
    double sr=44100; int n=512; float l[512],r[512];
    /* render most (but not all) of the ~2205-sample attack: env near peak */
    for(int b=0;b<4;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng); }
    assert(v.stage==1); /* still attacking, near peak */
    assert(v.env>0.5f);
    voice_note_off(&v);
    assert(v.stage==4);
    /* bounded wait: allow a generous 2x release-seconds margin */
    int release_samples=(int)(2.0*p.release*sr+0.5);
    int max_blocks=(release_samples/n)+5;
    int finished=0;
    for(int b=0;b<max_blocks && !finished;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v,&smp,&p,sr,l,r,n,&rng);
        if(voice_is_finished(&v)) finished=1; }
    assert(finished); /* ~200s-release bug: this fails (times out) pre-fix */
    printf("test_voice_release_small_sustain_near_peak OK\n");
}

/* Regression: the Position knob (p->position) must be the LIVE base of the
 * grain read pointer, not just a value NObody displays. Before the fix,
 * spawn_grain read from v->position (seeded to 0.0 and only ever moved by
 * macro drift), so with macro_speed==0 every grain read from frame 0
 * regardless of the Position param -- the knob was dead.
 * Build a source where the first half is silence (0.0) and the second half
 * is full-scale DC (1.0). position=0.05 must read the silent half (~0
 * energy); position=0.95 must read the loud half (audible energy). */
static void test_position_param_drives_grain_read(void){
    window_init();
    WavData smp; smp.num_frames=44100; smp.num_channels=1; smp.sample_rate=44100;
    static float buf[44100];
    for(int i=0;i<44100;i++) buf[i] = (i<22050) ? 0.0f : 1.0f;
    smp.data=buf;
    double sr=44100; int n=512; int nblocks=20; float l[512],r[512];

    Params p_low=default_params(); p_low.spread=0.f; p_low.macro_speed=0.f; p_low.position=0.05f;
    unsigned rng_low=1;
    Voice v_low={0}; voice_note_on(&v_low,60,1.0f);
    float energy_low=0;
    for(int b=0;b<nblocks;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v_low,&smp,&p_low,sr,l,r,n,&rng_low);
        for(int i=0;i<n;i++) energy_low+=l[i]*l[i]; }

    Params p_hi=default_params(); p_hi.spread=0.f; p_hi.macro_speed=0.f; p_hi.position=0.95f;
    unsigned rng_hi=1;
    Voice v_hi={0}; voice_note_on(&v_hi,60,1.0f);
    float energy_hi=0;
    for(int b=0;b<nblocks;b++){ for(int i=0;i<n;i++){l[i]=0;r[i]=0;}
        voice_render(&v_hi,&smp,&p_hi,sr,l,r,n,&rng_hi);
        for(int i=0;i<n;i++) energy_hi+=l[i]*l[i]; }

    printf("  position=0.05 energy=%f  position=0.95 energy=%f\n",energy_low,energy_hi);
    assert(energy_hi > energy_low*10.0f + 0.01f); /* dead-Position bug: both ~equal (both read frame 0) pre-fix */
    printf("test_position_param_drives_grain_read OK\n");
}

static void test_engine_polyphony_and_stealing(void){
    GranularEngine* e=ge_create(44100,16);
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f;
    ge_set_sample(e,buf,44100,1,44100);
    ge_set_param(e,P_ATTACK,0.5f); ge_set_param(e,P_RELEASE,0.5f);
    for(int nnote=60;nnote<63;nnote++) ge_note_on(e,nnote,1.0f);
    float l[256],r[256]; for(int i=0;i<256;i++){l[i]=0;r[i]=0;}
    ge_render(e,l,r,256);
    assert(ge_active_voice_count(e)==3);
    /* fill to 16, then a 17th steals -> stays at 16 */
    for(int nnote=63;nnote<76;nnote++) ge_note_on(e,nnote,1.0f);
    assert(ge_active_voice_count(e)==16);
    ge_note_on(e,90,1.0f);
    assert(ge_active_voice_count(e)==16);
    ge_destroy(e);
    printf("test_engine OK\n");
}

static void test_meters(void){
    GranularEngine* e=ge_create(44100,16);
    static float buf[44100]; for(int i=0;i<44100;i++) buf[i]=0.5f;
    ge_set_sample(e,buf,44100,1,44100);
    ge_set_param(e,P_DENSITY,50);
    unsigned before=ge_get_grains_fired(e);
    ge_note_on(e,60,1.0f);
    float l[512],r[512];
    for(int b=0;b<20;b++){ for(int i=0;i<512;i++){l[i]=0;r[i]=0;} ge_render(e,l,r,512); }
    assert(ge_get_grains_fired(e) > before);   /* grains actually fired */
    assert(ge_get_out_rms(e) > 0.0f);          /* audible while sounding */
    ge_note_off(e,60);
    for(int b=0;b<400;b++){ for(int i=0;i<512;i++){l[i]=0;r[i]=0;} ge_render(e,l,r,512); }
    assert(ge_get_out_rms(e) < 0.001f);        /* silent after release */
    ge_destroy(e);
    printf("test_meters OK\n");
}

static void test_wav_peaks(void){
    float pk[900];
    static float st[2000]; for(int i=0;i<2000;i++) st[i]=0.01f; st[2*500+1]=0.8f;   /* spike on R at frame 500 */
    wav_peaks(st,1000,2,pk,900);
    assert(pk[450]==1.0f);                                  /* bin = frame*900/1000 */
    for(int i=0;i<900;i++) assert(pk[i]>=0.f && pk[i]<=1.f);
    float tiny[10]={0,.5f,0,0,0,0,0,0,0,0}; wav_peaks(tiny,10,1,pk,900);   /* frames < bins: every bin populated */
    int nz=0; for(int i=0;i<900;i++) if(pk[i]>0) nz++; assert(nz>=90);
    float z[64]={0}; wav_peaks(z,64,1,pk,900); for(int i=0;i<900;i++) assert(pk[i]==0.f);   /* silent => zeros, not NaN */
    float bad[100]; for(int i=0;i<100;i++) bad[i]=0.25f; bad[10]=NAN; bad[20]=INFINITY;
    wav_peaks(bad,100,1,pk,900); for(int i=0;i<900;i++) assert(isfinite(pk[i]));
    wav_peaks(NULL,0,0,pk,900); for(int i=0;i<900;i++) assert(pk[i]==0.f);  /* empty input */
    printf("test_wav_peaks OK\n");
}

int main(void){
    test_wav_load();
    test_wav_load_24();
    test_wav_load_f32();
    test_wav_load_malformed_truncated();
    test_window();
    test_grain_length_and_taper();
    test_voice_sounds_then_silences();
    test_voice_release_sustain_zero_during_attack();
    test_voice_release_small_sustain_near_peak();
    test_position_param_drives_grain_read();
    test_engine_polyphony_and_stealing();
    test_meters();
    test_wav_peaks();
    printf("ALL CORE TESTS PASSED\n");
    return 0;
}
