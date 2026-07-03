#include "wav_fixture.h"
#include <stdio.h>
#include <string.h>
static void w32(FILE* f, unsigned v){ fputc(v&255,f);fputc((v>>8)&255,f);fputc((v>>16)&255,f);fputc((v>>24)&255,f); }
static void w16(FILE* f, unsigned v){ fputc(v&255,f);fputc((v>>8)&255,f); }

int write_test_wav(const char* path, int frames, int sr, short amp){
    FILE* f=fopen(path,"wb"); if(!f) return -1;
    unsigned data=frames*2, byteRate=sr*2;
    fwrite("RIFF",1,4,f); w32(f,36+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); w32(f,16); w16(f,1); w16(f,1);
    w32(f,sr); w32(f,byteRate); w16(f,2); w16(f,16);
    fwrite("data",1,4,f); w32(f,data);
    for(int i=0;i<frames;i++) w16(f,(unsigned short)amp);
    fclose(f); return 0;
}

int write_test_wav_24(const char* path, int frames, int sr, int amp){
    FILE* f=fopen(path,"wb"); if(!f) return -1;
    unsigned data=(unsigned)frames*3, byteRate=(unsigned)sr*3;
    fwrite("RIFF",1,4,f); w32(f,36+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); w32(f,16); w16(f,1); w16(f,1);
    w32(f,sr); w32(f,byteRate); w16(f,3); w16(f,24);
    fwrite("data",1,4,f); w32(f,data);
    unsigned v=(unsigned)(amp & 0xFFFFFF);
    for(int i=0;i<frames;i++){ fputc(v&255,f); fputc((v>>8)&255,f); fputc((v>>16)&255,f); }
    if(data&1) fputc(0,f);
    fclose(f); return 0;
}

int write_test_wav_f32(const char* path, int frames, int sr, float amp){
    FILE* f=fopen(path,"wb"); if(!f) return -1;
    unsigned data=(unsigned)frames*4, byteRate=(unsigned)sr*4;
    fwrite("RIFF",1,4,f); w32(f,36+data); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); w32(f,16); w16(f,3); w16(f,1);
    w32(f,sr); w32(f,byteRate); w16(f,4); w16(f,32);
    fwrite("data",1,4,f); w32(f,data);
    unsigned char b[4]; memcpy(b,&amp,4);
    for(int i=0;i<frames;i++) fwrite(b,1,4,f);
    fclose(f); return 0;
}

/* Header claims a full-size PCM16 data chunk of `frames` samples, but only
   half those sample bytes are actually written before the file ends -- the
   file on disk is shorter than the RIFF/data headers promise. */
int write_truncated_wav(const char* path, int frames, int sr){
    FILE* f=fopen(path,"wb"); if(!f) return -1;
    unsigned claimedData=(unsigned)frames*2;
    int actualFrames=frames/2;
    fwrite("RIFF",1,4,f); w32(f,36+claimedData); fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f); w32(f,16); w16(f,1); w16(f,1);
    w32(f,sr); w32(f,sr*2); w16(f,2); w16(f,16);
    fwrite("data",1,4,f); w32(f,claimedData); /* lie: on-disk data is shorter */
    for(int i=0;i<actualFrames;i++) w16(f,1234);
    fclose(f); return 0;
}
