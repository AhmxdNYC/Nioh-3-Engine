#include <stdio.h>
#include <stdlib.h>
#include "../mod/Nioh3Training/src/auto_moves.h"
// candidate: tree state 0 free, 1 damaging resolved, 2 unresolved; collect named (act,slot) of damaging slots reached
static AmTable t; static uint8_t*D; static uint16_t own[AM_MAXMOVES][AM_MAXROWS]; static int nown[AM_MAXMOVES];
static uint32_t n7,n8; static int reach[AM_MAXMOVES][AM_MAXROWS]; static int stamp=1; static int busy[65536];
static long cntTy14[256], lnHist[3], evlenD=0;
static void ownrows(void){ uint32_t sum=0; for(int r=0;r<t.n;r++) sum+=AmU16(D,t.off[0]+64+2*r); uint32_t p=t.off[0]+t.len[0]-sum;
 for(int r=0;r<t.n;r++){uint32_t rec=AmU16(D,t.off[0]+64+2*r); int c=AmU16(D,p); if(c>AM_MAXROWS)c=AM_MAXROWS; for(int k=0;k<c;k++) own[r][k]=AmU16(D,p+2+2*k); nown[r]=c; p+=rec;}}
static int slotst(uint16_t act,uint32_t s){int r=AmFind(&t,act); if(r<0||s>=(uint32_t)nown[r]||own[r][s]>=t.len[2]/AM_HB) return 2; uint32_t b=t.off[2]+own[r][s]*AM_HB;
 if(!(AmU16(D,b+0x10)||AmU16(D,b+0x12))) return 0; reach[r][s]=stamp; uint16_t ln=AmU16(D,b+12); lnHist[ln==0?0:ln==999?1:2]++; return 1;}
static int tree(uint32_t ev){ if(ev<20000||ev==0xFFFF||ev-20000>=n7) return 2; uint32_t N=ev-20000,b=t.off[7]+N*336; if(busy[N]==stamp) return 0; busy[N]=stamp;
 int st=0; if(D[b+0x14]>2) return 2;
 if(D[b+0x15]!=0xFF){int s=slotst(AmU16(D,b+0x12),D[b+0x15]); if(s>st)st=s; if(s==1) cntTy14[D[b+0x14]]++;}
 for(int k=0;k<6;k++){uint16_t c=AmU16(D,b+0x110+2*k); if(c==0xFFFF)continue; if(c>=n8) return 2; uint32_t e=t.off[8]+c*80; uint16_t ty=AmU16(D,e+0x4C); int s=0;
  if(ty==33) s=slotst(AmU16(D,e),AmU16(D,e+4)); else if(ty==3) s=tree(AmU16(D,e+4)); else for(int z=0;z<0x4C;z+=2){uint16_t v=AmU16(D,e+z); if((v>=20000&&v-20000u<n7)||(v>=1000&&AmFind(&t,v)>=0)) s=2;}
  if(s>st)st=s;}
 for(int k=0;k<4;k++){uint16_t c=AmU16(D,b+0x100+2*k); if(c&&c!=0xFFFF){int s=tree(c); if(s>st)st=s;}}
 return st;}
int main(int argc,char**argv){ char nm[1024]; FILE*l=fopen(argv[1],"r"); int verbose=argc>2;
 long cat[4]={0}, newc[4]={0}, pairs=0, agree=0, inwin=0, lenev=0; long hist[8]={0};
 static int sp[AM_MAXMOVES][16], nsp[AM_MAXMOVES];
 while(fgets(nm,sizeof nm,l)){ nm[strcspn(nm,"\r\n")]=0; FILE*f=fopen(nm,"rb"); if(!f)continue; fseek(f,0,SEEK_END); long L=ftell(f); rewind(f); D=malloc(L); fread(D,1,L,f); fclose(f);
  int na=AmParse(D,(uint32_t)L,&t); if(na<=0){free(D);continue;} ownrows(); n7=AmU16(D,t.off[0]+30); n8=AmU16(D,t.off[0]+34);
  static int cand[AM_MAXMOVES]; memset(reach,0,sizeof reach);
  for(int r=0;r<na;r++){nsp[r]=0; uint32_t o=t.off[1]+r*AM_ROW; for(int k=0;k<16;k++){uint16_t ev=AmU16(D,o+0x40+6*k),fr=AmU16(D,o+0x42+6*k); if(ev>=20000&&ev!=0xFFFF&&fr==0&&nsp[r]<16){int dup=0; for(int q=0;q<nsp[r];q++) dup|=sp[r][q]==ev; if(!dup) sp[r][nsp[r]++]=ev;}}}
  for(int r=0;r<na;r++){ AmMove*m=&t.m[r]; cand[r]=-1; if(!m->attack||m->hits||m->nohit) continue; if(!m->spawns) continue;
   int c= m->untimed?2:1; if(m->spawns&&m->untimed) c=3; cat[c]++;
   uint32_t o=t.off[1]+r*AM_ROW; uint16_t frs[16]; int nf=0, bad=0; stamp++;
   for(int k=0;k<16;k++){ uint16_t ev=AmU16(D,o+0x40+6*k), fr=AmU16(D,o+0x42+6*k), ln=AmU16(D,o+0x44+6*k); if(ev<20000||ev==0xFFFF) continue;
    int amb=0; if(fr==0){int cc=0; for(int q=0;q<na;q++) for(int z=0;z<nsp[q];z++) cc+=sp[q][z]==ev; amb=cc*10>=na*8;} if(amb) continue;
    int s=tree(ev); if(s==2){bad=1;break;} if(s==1){ if(ln!=0xFFFF){lenev++; bad=1; break;} int dup=0; for(int q=0;q<nf;q++) dup|=frs[q]==fr; if(!dup) frs[nf++]=fr; } }
   if(bad) continue;
   // untimed damaging named rows of this move must be reached by its own spawned trees
   if(m->untimed){ int r2=r; for(int k=0;k<nown[r2];k++){ uint32_t b=t.off[2]+own[r2][k]*AM_HB; if(own[r2][k]>=t.len[2]/AM_HB) continue; if(AmU16(D,b+8)==0xFFFF&&(AmU16(D,b+0x10)||AmU16(D,b+0x12))&&reach[r2][k]!=stamp){ /* may be named elsewhere */ bad=2; } } }
   if(bad) continue;
   cand[r]=m->nwin+nf; newc[c]++; hist[cand[r]<7?cand[r]:7]++;
   if(verbose) printf("%s %04X mo %u nwin %u spawnhits %d -> %d\n", nm+60, m->id, m->motion, m->nwin, nf, cand[r]);
  }
  for(int r=0;r<na;r++){ if(cand[r]<0||!t.m[r].motion) continue; for(int q=0;q<na;q++){ AmMove*s=&t.m[q]; if(q==r||!s->attack||!s->hits||s->motion!=t.m[r].motion) continue; pairs++; agree+=s->hits==cand[r]; if(verbose>0&&argc>3) printf("  pair %04X cand %d vs %04X %u\n", t.m[r].id,cand[r],s->id,s->hits);} }
  free(D);}
 printf("unknown with spawns: spawn-only %ld both %ld | newly countable: spawn-only %ld both %ld | dmg-event with len %ld\n",cat[1],cat[3],newc[1],newc[3],lenev);
 printf("hist 0..7+:"); for(int i=0;i<8;i++) printf(" %ld",hist[i]); printf("\nsibling pairs %ld agree %ld (%.1f%%)\n",pairs,agree,pairs?100.0*agree/pairs:0);
 printf("ty14 of dmg objs: 1=%ld 2=%ld 0=%ld | slot ln 0=%ld 999=%ld other=%ld\n",cntTy14[1],cntTy14[2],cntTy14[0],lnHist[0],lnHist[1],lnHist[2]);
}
