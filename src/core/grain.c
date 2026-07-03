#include "grain.h"
#include "window.h"

static float sample_at(const Grain* g, double pos, int ch){
    if(pos<0||pos>=g->src_frames-1) return 0.f;
    long i=(long)pos; double f=pos-i;
    const float* s=g->src;
    int C=g->src_channels;
    float a=s[i*C+ch], b=s[(i+1)*C+ch];
    return (float)(a+(b-a)*f);
}

int grain_tick(Grain* g, float* l, float* r){
    if(!g->active || g->age>=g->length_samples){ g->active=0; return 0; }
    float phase=(float)g->age/(float)g->length_samples;
    float w=window_hann(phase)*g->amp;
    float mono, left, right;
    if(g->src_channels>=2){
        left=sample_at(g,g->read_pos,0);
        right=sample_at(g,g->read_pos,1);
    } else {
        mono=sample_at(g,g->read_pos,0); left=right=mono;
    }
    /* equal-power-ish pan */
    float pan=g->pan; float lg=(pan<=0)?1.0f:(1.0f-pan); float rg=(pan>=0)?1.0f:(1.0f+pan);
    *l += left*w*lg;
    *r += right*w*rg;
    g->read_pos += g->pitch_ratio;
    g->age++;
    if(g->age>=g->length_samples){ g->active=0; }
    return 1;
}
