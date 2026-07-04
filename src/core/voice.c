#include "voice.h"
#include "params.h"
#include "window.h"
#include <math.h>

static float frand(unsigned* s){ *s=*s*1664525u+1013904223u; return ((*s>>8)&0xFFFFFF)/(float)0x1000000; }

void voice_note_on(Voice* v, int note, float velocity){
    v->active=1; v->note=note; v->note_pitch=powf(2.0f,(note-60)/12.0f);
    v->drift=0.0; v->grain_phase_acc=0.0; v->velocity=velocity;
    v->stage=1; v->env=0.0f; v->age_samples=0; v->release_start_env=0.0f;
    for(int i=0;i<VOICE_MAX_GRAINS;i++) v->grains[i].active=0;
}
void voice_note_off(Voice* v){ if(v->active && v->stage!=0 && v->stage!=4){ v->release_start_env=v->env; v->stage=4; } }
int voice_is_finished(const Voice* v){ return !v->active; }

static float adsr_tick(Voice* v, const Params* p, double sr){
    float a=p->attack, d=p->decay, s=p->sustain, r=p->release;
    float step;
    switch(v->stage){
        case 1: step=(a<=0)?1.f:(1.0f/(a*sr)); v->env+=step; if(v->env>=1.f){v->env=1.f;v->stage=2;} break;
        case 2: step=(d<=0)?1.f:((1.0f-s)/(d*sr)); v->env-=step; if(v->env<=s){v->env=s;v->stage=3;} break;
        case 3: v->env=s; break;
        /* Linear ramp from the envelope level captured at release onset
         * (release_start_env) down to 0 in exactly `r` seconds. Using the
         * live env-at-release-time (rather than re-reading p->sustain every
         * tick) means this terminates correctly for ANY sustain value,
         * including 0, and from ANY stage the release began in. */
        case 4: step=(r<=0)?1.f:(v->release_start_env/(r*sr)); v->env-=step; if(v->env<=0.0001f){v->env=0.f;v->stage=0;v->active=0;} break;
        default: v->env=0.f;
    }
    return v->env;
}

static void spawn_grain(Voice* v, const WavData* smp, const Params* p, unsigned* rng){
    for(int i=0;i<VOICE_MAX_GRAINS;i++){
        if(!v->grains[i].active){
            Grain* g=&v->grains[i];
            float spray=(frand(rng)-0.5f)*p->spread;
            double pos=p->position+v->drift+spray; if(pos<0)pos=0; if(pos>1)pos=1;
            g->src=smp->data; g->src_frames=smp->num_frames; g->src_channels=smp->num_channels;
            g->read_pos=pos*smp->num_frames;
            g->pitch_ratio=v->note_pitch*p->pitch_mult;
            g->length_samples=(int)(p->grain_ms*0.001f*smp->sample_rate);
            if(g->length_samples<2) g->length_samples=2;
            g->age=0; g->pan=(frand(rng)*2.0f-1.0f)*p->pan_spread;
            g->amp=v->velocity; g->active=1;
            return;
        }
    }
}

void voice_render(Voice* v, const WavData* smp, const Params* p, double sr,
                  float* l, float* r, int n, unsigned* rng){
    if(!v->active || !smp || !smp->data) return;
    double grains_per_sample=p->density/sr;
    for(int i=0;i<n;i++){
        float env=adsr_tick(v,p,sr);
        /* schedule grains */
        v->grain_phase_acc+=grains_per_sample;
        while(v->grain_phase_acc>=1.0){ v->grain_phase_acc-=1.0; spawn_grain(v,smp,p,rng); }
        /* macro drift: slow offset applied on top of the live p->position base */
        if(p->macro_speed!=0.0f && smp->num_frames>0)
            v->drift+=(double)p->macro_speed/ (smp->num_frames/sr) / sr;
        if(v->drift>1.0)v->drift-=1.0; if(v->drift<0.0)v->drift+=1.0;
        /* mix live grains */
        float gl=0.f, gr=0.f;
        for(int gi=0;gi<VOICE_MAX_GRAINS;gi++) if(v->grains[gi].active) grain_tick(&v->grains[gi],&gl,&gr);
        l[i]+=gl*env*p->master_gain;
        r[i]+=gr*env*p->master_gain;
        v->age_samples++;
    }
}
