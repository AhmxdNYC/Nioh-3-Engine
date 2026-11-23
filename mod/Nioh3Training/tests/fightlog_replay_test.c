// Replays a recorded log through the production ring and formatter. No game.
#include "../src/fightlog.h"
#include <assert.h>
int main(int argc, char** argv) {
    assert(argc==2); FILE* f=fopen(argv[1],"r");assert(f);
    FlLog log;FlInit(&log);char line[1024],id[32]="",buf[8192];unsigned chr=0;int starts=0,ends=0;
    while(fgets(line,sizeof line,f)) {
        unsigned h,m,s,ms;if(sscanf(line,"[%u:%u:%u.%u]",&h,&m,&s,&ms)!=4)continue;
        char* p=strstr(line,"TRAIN start ");if(p)sscanf(p,"TRAIN start %31s",id);
        p=strstr(line,"TRAIN arena session ");if(p)sscanf(p,"TRAIN arena session %31[^:]",id);
        p=strstr(line,"ARENA start chr ");if(p)sscanf(p,"ARENA start chr %u",&chr);
        FlClock c={2026,11,4,(int)h,(int)m,(int)s};uint32_t tick=((h*60+m)*60+s)*1000+ms;
        p=strstr(line,"FIGHT start: ");
        if(p){char name[FL_NAME];assert(sscanf(p,"FIGHT start: %47[^|]",name)==1);size_t n=strlen(name);while(n && name[n-1]==' ')name[--n]=0;
            FlStart(&log,strstr(p,"Practice")?FL_PRACTICE:FL_BOSSMATCH,name,tick,&c);FlIdentify(&log,id,chr);++starts;}
        p=strstr(line,"FIGHT end: ");
        if(p){int result=strstr(p,"| win")?FL_WIN:strstr(p,"| loss")?FL_LOSS:FL_QUIT;assert(FlEnd(&log,result,tick,&c));++ends;}
    }
    fclose(f);assert(starts==4 && ends==4 && log.count==4 && log.open==-1);
    assert(log.rec[0].result==FL_WIN && log.rec[1].result==FL_WIN && log.rec[2].result==FL_QUIT && log.rec[3].result==FL_QUIT);
    const uint32_t durations[]={48803,62772,243505,101903};
    for(int i=0;i<4;++i)assert(log.rec[i].t1-log.rec[i].t0==durations[i]);
    assert(!strcmp(log.rec[0].boss_id,"R1CBC") && !strcmp(log.rec[1].boss_id,"R1CBC"));
    assert(!strcmp(log.rec[2].boss_id,"RC1D9") && !strcmp(log.rec[3].boss_id,"RF5E5"));
    assert(log.rec[2].character_id==803396 && log.rec[3].character_id==696121);
    FlFormat(&log,buf,sizeof buf);puts("-- Fights (last 20 this session; local clock time) --");fputs(buf,stdout);
    puts("PASS: 4 recorded fights, 2 wins/2 quits; exact millisecond durations and captured identities match.");return 0;
}
