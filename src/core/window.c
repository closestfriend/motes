#include "window.h"
#include <math.h>
#define WIN_N 4096
static float g_table[WIN_N+1];
static int g_ready=0;
void window_init(void){
    if(g_ready) return;
    for(int i=0;i<=WIN_N;i++){
        double p=(double)i/WIN_N;
        g_table[i]=(float)(0.5*(1.0-cos(2.0*M_PI*p)));
    }
    g_ready=1;
}
float window_hann(float phase01){
    if(phase01<=0.f) return 0.f; if(phase01>=1.f) return 0.f;
    float x=phase01*WIN_N; int i=(int)x; float frac=x-i;
    return g_table[i]+(g_table[i+1]-g_table[i])*frac;
}
