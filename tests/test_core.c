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
    printf("test_wav_load OK\n");
}

int main(void){ test_wav_load(); printf("ALL CORE TESTS PASSED\n"); return 0; }
