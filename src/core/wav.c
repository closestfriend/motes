#include "wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rd32(const unsigned char* p){ return p[0]|p[1]<<8|p[2]<<16|(unsigned)p[3]<<24; }
static unsigned rd16(const unsigned char* p){ return p[0]|p[1]<<8; }

int wav_load(const char* path, WavData* out){
    FILE* f=fopen(path,"rb"); if(!f) return -1;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<44){ fclose(f); return -2; }
    unsigned char* buf=(unsigned char*)malloc(sz);
    if(!buf){ fclose(f); return -3; }
    if(fread(buf,1,sz,f)!=(size_t)sz){ free(buf); fclose(f); return -4; }
    fclose(f);
    if(memcmp(buf,"RIFF",4)||memcmp(buf+8,"WAVE",4)){ free(buf); return -5; }

    unsigned fmt=0,ch=0,sr=0,bits=0; long off=12; long dataOff=-1,dataLen=0;
    while(off+8<=sz){
        const unsigned char* id=buf+off; unsigned clen=rd32(buf+off+4); long body=off+8;
        /* Reject any chunk whose declared length would read past the actual
         * file size -- a header-declared length is untrusted input and must
         * never be used to index past the buffer we actually read. */
        if(clen > (unsigned long)(sz - body)){ free(buf); return -8; }
        if(!memcmp(id,"fmt ",4)){
            if(clen<16){ free(buf); return -9; }
            fmt=rd16(buf+body); ch=rd16(buf+body+2); sr=rd32(buf+body+4); bits=rd16(buf+body+14);
        } else if(!memcmp(id,"data",4)){ dataOff=body; dataLen=clen; }
        off = body + clen + (clen&1);
    }
    if(dataOff<0||ch==0){ free(buf); return -6; }
    /* dataOff+dataLen <= sz is already guaranteed by the chunk-bounds check
     * above (every chunk, including "data", was validated against sz). */

    if(!(bits==8||bits==16||bits==24||bits==32)){ free(buf); return -10; }
    int supported = (fmt==1 && (bits==8||bits==16||bits==24)) || (fmt==3 && bits==32);
    if(!supported){ free(buf); return -11; }

    int bytes=bits/8; int frames=dataLen/(bytes*ch);
    float* data=(float*)malloc(sizeof(float)*frames*ch);
    if(!data){ free(buf); return -7; }
    const unsigned char* d=buf+dataOff;
    for(int i=0;i<frames*(int)ch;i++){
        const unsigned char* s=d+(long)i*bytes; float v=0.f;
        if(fmt==3 && bits==32){ float f32; memcpy(&f32,s,4); v=f32; }
        else if(bits==16){ short x=(short)rd16(s); v=x/32768.0f; }
        else if(bits==24){ int x=(s[0])|(s[1]<<8)|(s[2]<<16); if(x&0x800000) x|=~0xFFFFFF; v=x/8388608.0f; }
        else if(bits==8){ v=(s[0]-128)/128.0f; }
        data[i]=v;
    }
    out->data=data; out->num_frames=frames; out->num_channels=(int)ch; out->sample_rate=(double)sr;
    free(buf); return 0;
}

void wav_free(WavData* w){ if(w&&w->data){ free(w->data); w->data=NULL; } }
