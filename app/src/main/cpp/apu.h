#pragma once
static const u8 lenTab[32]={10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30};
static const u8 dutyTab[4]={0x40,0x60,0x78,0x9F};
static const u16 noiseTab[16]={4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068};
struct Pulse{ bool en,halt,cv,estart,sen,sneg,sreload; u8 duty,vol,ediv,edec,sper,ssh,sdiv,seq,len; u16 per,tim; };
static Pulse P[2];
static bool tEn,tCtl,tRel; static u8 tLinR,tLin,tSeq,tLen; static u16 tPer,tTim;
static bool nEn,nHalt,nCv,nStart,nMode; static u8 nVol,nDiv,nDec,nLen; static u16 nPer,nTim,nSh;
static int fcCyc; static bool fcMode,apuOdd;
static double sAcc; static long sumP,sumT,sumN; static int sumC;
static int16_t abuf[4096]; static int acount; static float hpX,hpY;

static void apuReset(){
  memset(P,0,sizeof P); tEn=tCtl=tRel=false; tLinR=tLin=tSeq=tLen=0; tPer=tTim=0;
  nEn=nHalt=nCv=nStart=nMode=false; nVol=nDiv=nDec=nLen=0; nPer=4; nTim=0; nSh=1;
  fcCyc=0; fcMode=false; apuOdd=false; sAcc=0; sumP=sumT=sumN=0; sumC=0; acount=0; hpX=hpY=0;
}
static void envClk(bool& st,u8& div,u8& dec,u8 vol,bool halt){
  if(st){ st=false; dec=15; div=vol; }
  else if(div==0){ div=vol; if(dec>0) dec--; else if(halt) dec=15; }
  else div--;
}
static int pTarget(Pulse& p,int ch){ int d=p.per>>p.ssh; return p.sneg ? p.per-d-(ch==0?1:0) : p.per+d; }
static void sweepClk(Pulse& p,int ch){
  int t=pTarget(p,ch); bool mute=(p.per<8||t>0x7FF);
  if(p.sdiv==0 && p.sen && p.ssh && !mute) p.per=(u16)t;
  if(p.sdiv==0||p.sreload){ p.sdiv=p.sper; p.sreload=false; } else p.sdiv--;
}
static void quarter(){
  for(int i=0;i<2;i++) envClk(P[i].estart,P[i].ediv,P[i].edec,P[i].vol,P[i].halt);
  envClk(nStart,nDiv,nDec,nVol,nHalt);
  if(tRel) tLin=tLinR; else if(tLin) tLin--;
  if(!tCtl) tRel=false;
}
static void half(){
  for(int i=0;i<2;i++){ if(!P[i].halt && P[i].len) P[i].len--; sweepClk(P[i],i); }
  if(!nHalt && nLen) nLen--;
  if(!tCtl && tLen) tLen--;
}
static int pOut(Pulse& p,int ch){
  if(!p.en||!p.len||p.per<8||pTarget(p,ch)>0x7FF) return 0;
  if(!((dutyTab[p.duty]>>(7-p.seq))&1)) return 0;
  return p.cv?p.vol:p.edec;
}
static int tOut(){ if(tPer<2) return 7; return tSeq<16?15-tSeq:tSeq-16; }
static int nOut(){ if(!nEn||!nLen||(nSh&1)) return 0; return nCv?nVol:nDec; }
static void apuTick(){
  fcCyc++;
  if(fcCyc==3729||fcCyc==11186) quarter();
  else if(fcCyc==7457){ quarter(); half(); }
  else if(!fcMode && fcCyc==14915){ quarter(); half(); fcCyc=0; }
  else if(fcMode && fcCyc==18641){ quarter(); half(); fcCyc=0; }
  if(tTim==0){ tTim=tPer; if(tLen&&tLin) tSeq=(tSeq+1)&31; } else tTim--;
  if(nTim==0){
    nTim=nPer;
    int fb=(nSh&1)^((nSh>>(nMode?6:1))&1);
    nSh=(nSh>>1)|(fb<<14);
  } else nTim--;
  apuOdd=!apuOdd;
  if(apuOdd) for(int i=0;i<2;i++){
    if(P[i].tim==0){ P[i].tim=P[i].per; P[i].seq=(P[i].seq+1)&7; } else P[i].tim--;
  }
  sumP+=pOut(P[0],0)+pOut(P[1],1); sumT+=tOut(); sumN+=nOut(); sumC++;
  sAcc+=44100.0;
  if(sAcc>=1789773.0){
    sAcc-=1789773.0;
    double pa=(double)sumP/sumC, ta=(double)sumT/sumC, na=(double)sumN/sumC;
    double pu=pa>0?95.88/(8128.0/pa+100.0):0;
    double x=ta/8227.0+na/12241.0;
    double tn=x>0?159.79/(1.0/x+100.0):0;
    float v=(float)(pu+tn);
    float y=v-hpX+0.996f*hpY; hpX=v; hpY=y;
    int s=(int)(y*30000.0f); if(s>32767)s=32767; if(s<-32768)s=-32768;
    if(acount<4096) abuf[acount++]=(int16_t)s;
    sumP=sumT=sumN=0; sumC=0;
  }
}
static void apuClock(int n){ while(n-->0) apuTick(); }
static u8 apuStatus(){ return (P[0].len?1:0)|(P[1].len?2:0)|(tLen?4:0)|(nLen?8:0); }
static void apuWrite(u16 a,u8 v){
  if(a<=0x4007){
    int ch=(a>>2)&1; Pulse& p=P[ch];
    switch(a&3){
      case 0: p.duty=v>>6; p.halt=v&0x20; p.cv=v&0x10; p.vol=v&15; break;
      case 1: p.sen=v&0x80; p.sper=(v>>4)&7; p.sneg=v&8; p.ssh=v&7; p.sreload=true; break;
      case 2: p.per=(p.per&0x700)|v; break;
      default: p.per=(p.per&0xFF)|((v&7)<<8); if(p.en) p.len=lenTab[v>>3]; p.seq=0; p.estart=true; break;
    }
    return;
  }
  switch(a){
    case 0x4008: tCtl=v&0x80; tLinR=v&0x7F; break;
    case 0x400A: tPer=(tPer&0x700)|v; break;
    case 0x400B: tPer=(tPer&0xFF)|((v&7)<<8); if(tEn) tLen=lenTab[v>>3]; tRel=true; break;
    case 0x400C: nHalt=v&0x20; nCv=v&0x10; nVol=v&15; break;
    case 0x400E: nMode=v&0x80; nPer=noiseTab[v&15]; break;
    case 0x400F: if(nEn) nLen=lenTab[v>>3]; nStart=true; break;
    case 0x4015:
      P[0].en=v&1; P[1].en=v&2; tEn=v&4; nEn=v&8;
      if(!P[0].en) P[0].len=0; if(!P[1].en) P[1].len=0; if(!tEn) tLen=0; if(!nEn) nLen=0;
      break;
    case 0x4017: fcMode=v&0x80; fcCyc=0; if(fcMode){ quarter(); half(); } break;
  }
}
extern "C" JNIEXPORT jint JNICALL Java_com_brruham_nes_NesCore_getAudio(JNIEnv* env,jclass,jshortArray out){
  int n=acount; jsize cap=env->GetArrayLength(out); if(n>cap) n=cap;
  if(n>0) env->SetShortArrayRegion(out,0,n,(const jshort*)abuf);
  acount=0; return n;
}
