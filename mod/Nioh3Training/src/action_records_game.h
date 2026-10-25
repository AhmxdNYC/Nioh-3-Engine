#pragma once
#include "action_records.h"
static ArTable g_action_records;
static void ArLoadGame(void) {
    char path[MAX_PATH];snprintf(path,sizeof path,"%s\\Nioh3Training_action_records.tsv",g_dir);
    FILE* f=fopen(path,"rb"); if(!f){Log("ACTIONRECORD: table missing; hits/damage unknown (no hand fallback)");return;}
    if(fseek(f,0,SEEK_END)){fclose(f);return;} long n=ftell(f);
    if(n<=0 || n>16*1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);Log("ACTIONRECORD: table size invalid");return;}
    char* text=(char*)malloc((size_t)n+1);if(!text){fclose(f);return;}
    size_t got=fread(text,1,(size_t)n,f);fclose(f);text[got]=0;
    int count=got==(size_t)n && !memchr(text,0,got)?ArLoad(&g_action_records,text):-1;free(text);
    Log("ACTIONRECORD: %d archive rows loaded; join action id + motion + animation hash; conflicting joins remain unknown",count);
}
