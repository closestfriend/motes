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
