#include <stdio.h>
#include <stdlib.h>
#include "../mod/Nioh3Training/src/auto_moves.h"
// list unknown-by-spawn attacks: event, object tree, damaging slots
static AmTable t; static uint8_t*D; static uint16_t own[AM_MAXMOVES][AM_MAXROWS]; static int nown[AM_MAXMOVES];
static void ownrows(void){ const uint32_t na=t.n; uint32_t sum=0; for(uint32_t r=0;r<na;r++) sum+=AmU16(D,t.off[0]+64+2*r); uint32_t p=t.off[0]+t.len[0]-sum;
 for(uint32_t r=0;r<na;r++){uint32_t rec=AmU16(D,t.off[0]+64+2*r); int c=AmU16(D,p); if(c>AM_MAXROWS)c=AM_MAXROWS; for(int k=0;k<c;k++) own[r][k]=AmU16(D,p+2+2*k); nown[r]=c; p+=rec;}}
static void slot(const char*pre,uint16_t act,int s){int r=AmFind(&t,act); if(r<0||s>=nown[r]){printf("%s slot %04X/%d UNRES\n",pre,act,s);return;} uint32_t b=t.off[2]+own[r][s]*AM_HB;
 printf("%s slot %04X/%d row%u st %04X ef %04X ln %u pow %u el %u ty %04X grp %u |", pre, act, s, own[r][s], AmU16(D,b+8),AmU16(D,b+10),AmU16(D,b+12),AmU16(D,b+0x10),AmU16(D,b+0x12),AmU16(D,b+0x14),AmHitGroup(D,b));
 for(int z=0x16;z<0x40;z+=2) printf(" %04X",AmU16(D,b+z)); printf("\n");}
static void obj(int N,int dep){ if(dep>4) return; uint32_t b=t.off[7]+N*336; char pre[16]; memset(pre,' ',dep*2+2); pre[dep*2+2]=0;
 printf("%sobj %d +12 %04X +14 %u +15 %u | ", pre, N, AmU16(D,b+0x12), D[b+0x14], D[b+0x15]); for(int z=0;z<0x100;z+=2){uint16_t v=AmU16(D,b+z); if(v&&v!=0xFFFF) printf("%X:%X ",z,v);} printf("\n");
 if(D[b+0x15]!=0xFF) slot(pre,AmU16(D,b+0x12),D[b+0x15]);
 for(int k=0;k<6;k++){uint16_t c=AmU16(D,b+0x110+2*k); if(c==0xFFFF)continue; uint32_t e=t.off[8]+c*80; printf("%s comp %u ty %u |",pre,c,AmU16(D,e+0x4C)); for(int z=0;z<80;z+=2){uint16_t v=AmU16(D,e+z); if(v&&v!=0xFFFF) printf(" %X:%X",z,v);} printf("\n");
  if(AmU16(D,e+0x4C)==33) slot(pre,AmU16(D,e),AmU16(D,e+4)); if(AmU16(D,e+0x4C)==3) obj(AmU16(D,e+4)-20000,dep+1);}
 for(int k=0;k<4;k++){uint16_t c=AmU16(D,b+0x100+2*k); if(c&&c!=0xFFFF) obj(c-20000,dep+1);} }
int main(int argc,char**argv){ if(argc>3&&!strcmp(argv[2],"-o")){FILE*f=fopen(argv[1],"rb"); fseek(f,0,SEEK_END); long L=ftell(f); rewind(f); D=malloc(L); fread(D,1,L,f); fclose(f); AmParse(D,(uint32_t)L,&t); ownrows(); for(int a=3;a<argc;a++) obj(atoi(argv[a]),0); return 0;}
 char nm[1024]; FILE*l=fopen(argv[1],"r"); int lim=atoi(argv[2]); int cnt=0;
 while(fgets(nm,sizeof nm,l)&&cnt<lim){ nm[strcspn(nm,"\r\n")]=0; FILE*f=fopen(nm,"rb"); if(!f)continue; fseek(f,0,SEEK_END); long L=ftell(f); rewind(f); D=malloc(L); fread(D,1,L,f); fclose(f);
  int na=AmParse(D,(uint32_t)L,&t); if(na<=0){free(D);continue;} ownrows();
  for(int r=0;r<na&&cnt<lim;r++){AmMove*m=&t.m[r]; if(!m->attack||m->hits||m->nohit||!m->spawns||m->untimed) continue; cnt++;
   printf("== %s %04X motion %u nwin %u\n", nm+60, m->id, m->motion, m->nwin); uint32_t o=t.off[1]+r*AM_ROW;
   for(int k=0;k<16;k++){uint16_t ev=AmU16(D,o+0x40+6*k); if(ev==0xFFFF) continue; printf(" ev %u @%u len %u\n",ev,AmU16(D,o+0x42+6*k),AmU16(D,o+0x44+6*k)); if(ev>=20000&&ev-20000u<t.len[7]/336) obj(ev-20000,0);} }
  free(D);} }
