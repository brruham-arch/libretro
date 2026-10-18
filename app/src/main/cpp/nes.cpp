#include <jni.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <mutex>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t s8;

// ---------- Cartridge / mapper (0,1,2,3,7) ----------
static std::vector<u8> prg, chr;
static int mapper=0, prgBanks=1, mirrorMode=0;
static bool chrRam=false;
static u8 chrRamBuf[8192], prgRam[8192];
static u8 bankReg=0, chrReg=0, mShift=0x10, mCtrl=0x0C, mChr0=0, mChr1=0, mPrg=0;

static u8 prgRead(u16 a){
  int off=a&0x3FFF, b;
  switch(mapper){
    case 1: { int mode=(mCtrl>>2)&3;
      if(mode<2) b=(mPrg&0xE)+((a>>14)&1);
      else if(mode==2) b=(a<0xC000)?0:mPrg;
      else b=(a<0xC000)?mPrg:prgBanks-1;
      break; }
    case 2: b=(a<0xC000)?bankReg:prgBanks-1; break;
    case 7: b=(bankReg&7)*2+((a>>14)&1); break;
    default: b=(a-0x8000)>>14; break;
  }
  b%=prgBanks;
  return prg[(size_t)b*0x4000+off];
}
static u8 chrRead(u16 a){
  if(chrRam) return chrRamBuf[a&0x1FFF];
  size_t i;
  if(mapper==1){
    if(mCtrl&0x10) i=(size_t)((a<0x1000)?mChr0:mChr1)*0x1000+(a&0xFFF);
    else i=(size_t)(mChr0&0x1E)*0x1000+a;
  } else if(mapper==3) i=(size_t)chrReg*0x2000+a;
  else i=a;
  return chr[i%chr.size()];
}
static void chrWrite(u16 a,u8 v){ if(chrRam) chrRamBuf[a&0x1FFF]=v; }
static void mapperWrite(u16 a,u8 v){
  switch(mapper){
    case 1: {
      if(v&0x80){ mShift=0x10; mCtrl|=0x0C; }
      else {
        bool done=mShift&1;
        mShift=(mShift>>1)|((v&1)<<4);
        if(done){
          switch((a>>13)&3){ case 0:mCtrl=mShift;break; case 1:mChr0=mShift;break; case 2:mChr1=mShift;break; default:mPrg=mShift&0xF; }
          mShift=0x10;
        }
      }
      static const int mm[4]={2,3,1,0}; mirrorMode=mm[mCtrl&3];
      break; }
    case 2: bankReg=v&0xF; break;
    case 3: chrReg=v&3; break;
    case 7: bankReg=v&7; mirrorMode=(v&0x10)?3:2; break;
  }
}

// ---------- PPU ----------
static u8 nt[2048], pal[32], oam[256];
static u16 vAddr, tAddr; static u8 fineX; static bool wLatch;
static u8 pCtrl, pMask, pStatus, oamAddr, readBuf;
static bool nmiPending=false;
static u32 fb[256*240];
static const u32 palette[64]={
0x545454,0x001E74,0x081090,0x300088,0x440064,0x5C0030,0x540400,0x3C1800,0x202A00,0x083A00,0x004000,0x003C00,0x00323C,0x000000,0x000000,0x000000,
0x989698,0x084CC4,0x3032EC,0x5C1EE4,0x8814B0,0xA01464,0x982220,0x783C00,0x545A00,0x287200,0x087C00,0x007628,0x006678,0x000000,0x000000,0x000000,
0xECEEEC,0x4C9AEC,0x787CEC,0xB062EC,0xE454EC,0xEC58B4,0xEC6A64,0xD48820,0xA0AA00,0x74C400,0x4CD020,0x38CC6C,0x38B4CC,0x3C3C3C,0x000000,0x000000,
0xECEEEC,0xA8CCEC,0xBCBCEC,0xD4B2EC,0xECAEEC,0xECAED4,0xECB4B0,0xE4C490,0xCCD278,0xB4DE78,0xA8E290,0x98E2B4,0xA0D6E4,0xA0A2A0,0x000000,0x000000};

static u16 ntAddr(u16 a){
  a=(a-0x2000)&0xFFF; int t=a>>10, off=a&0x3FF, page;
  switch(mirrorMode){ case 0: page=t>>1; break; case 1: page=t&1; break; case 2: page=0; break; default: page=1; }
  return page*0x400+off;
}
static u8 palIdx(u16 a){ a&=0x1F; if((a&0x13)==0x10) a&=~0x10; return (u8)a; }
static u8 ppuRd(u16 a){ a&=0x3FFF; if(a<0x2000) return chrRead(a); if(a<0x3F00) return nt[ntAddr(a)]; return pal[palIdx(a)]; }
static void ppuWr(u16 a,u8 v){ a&=0x3FFF; if(a<0x2000) chrWrite(a,v); else if(a<0x3F00) nt[ntAddr(a)]=v; else pal[palIdx(a)]=v&0x3F; }

static u8 ppuRegRead(int r){
  u8 d=0;
  switch(r){
    case 2: d=(pStatus&0xE0)|(readBuf&0x1F); pStatus&=~0x80; wLatch=false; break;
    case 4: d=oam[oamAddr]; break;
    case 7: { u16 a=vAddr&0x3FFF;
      if(a>=0x3F00){ d=ppuRd(a); readBuf=ppuRd(a-0x1000); } else { d=readBuf; readBuf=ppuRd(a); }
      vAddr=(vAddr+((pCtrl&4)?32:1))&0x7FFF; break; }
  }
  return d;
}
static void ppuRegWrite(int r,u8 d){
  switch(r){
    case 0: { bool old=pCtrl&0x80; pCtrl=d; tAddr=(tAddr&0xF3FF)|((d&3)<<10);
      if(!old&&(d&0x80)&&(pStatus&0x80)) nmiPending=true; break; }
    case 1: pMask=d; break;
    case 3: oamAddr=d; break;
    case 4: oam[oamAddr++]=d; break;
    case 5:
      if(!wLatch){ tAddr=(tAddr&0xFFE0)|(d>>3); fineX=d&7; wLatch=true; }
      else { tAddr=(tAddr&0x8C1F)|((d&7)<<12)|((d&0xF8)<<2); wLatch=false; }
      break;
    case 6:
      if(!wLatch){ tAddr=(tAddr&0x00FF)|((d&0x3F)<<8); wLatch=true; }
      else { tAddr=(tAddr&0xFF00)|d; vAddr=tAddr; wLatch=false; }
      break;
    case 7: ppuWr(vAddr,d); vAddr=(vAddr+((pCtrl&4)?32:1))&0x7FFF; break;
  }
}

static void renderLine(int line){
  u32* out=&fb[line*256];
  u8 bgPix[256]; memset(bgPix,0,256);
  u32 back=0xFF000000|palette[pal[0]&0x3F];
  for(int i=0;i<256;i++) out[i]=back;
  if(!(pMask&0x18)) return;
  if(pMask&8){
    u16 vv=vAddr, base=(pCtrl&0x10)?0x1000:0; int fy=(vv>>12)&7;
    for(int t=0;t<33;t++){
      u8 tile=ppuRd(0x2000|(vv&0xFFF));
      u8 at=ppuRd(0x23C0|(vv&0xC00)|((vv>>4)&0x38)|((vv>>2)&7));
      int sh=((vv>>4)&4)|(vv&2), attr=(at>>sh)&3;
      u8 lo=ppuRd(base+tile*16+fy), hi=ppuRd(base+tile*16+fy+8);
      for(int b=0;b<8;b++){
        int sx=t*8+b-fineX;
        if(sx<0||sx>255) continue;
        int px=(((hi>>(7-b))&1)<<1)|((lo>>(7-b))&1);
        if(!px) continue;
        if(sx<8 && !(pMask&2)) continue;
        bgPix[sx]=1;
        out[sx]=0xFF000000|palette[pal[palIdx((attr<<2)|px)]&0x3F];
      }
      if((vv&0x1F)==31){ vv&=~0x1F; vv^=0x400; } else vv++;
    }
  }
  if(pMask&0x10){
    int h=(pCtrl&0x20)?16:8, cnt=0; u8 sdone[256]; memset(sdone,0,256);
    for(int i=0;i<64;i++){
      int row=line-(oam[i*4]+1);
      if(row<0||row>=h) continue;
      if(++cnt>8){ pStatus|=0x20; break; }
      u8 tile=oam[i*4+1], at=oam[i*4+2]; int x0=oam[i*4+3];
      if(at&0x80) row=h-1-row;
      u16 pb;
      if(h==16){ pb=(tile&1)?0x1000:0; tile&=0xFE; if(row>=8){ tile++; row-=8; } }
      else pb=(pCtrl&8)?0x1000:0;
      u8 lo=ppuRd(pb+tile*16+row), hi=ppuRd(pb+tile*16+row+8);
      for(int b=0;b<8;b++){
        int sx=x0+b; if(sx>255) break;
        int bit=(at&0x40)?b:7-b;
        int px=(((hi>>bit)&1)<<1)|((lo>>bit)&1);
        if(!px) continue;
        if(sx<8 && !(pMask&4)) continue;
        if(i==0 && bgPix[sx] && sx!=255) pStatus|=0x40;
        if(sdone[sx]) continue;
        sdone[sx]=1;
        if((at&0x20) && bgPix[sx]) continue;
        out[sx]=0xFF000000|palette[pal[palIdx(0x10|((at&3)<<2)|px)]&0x3F];
      }
    }
  }
}
static void endLineUpdate(){
  if(!(pMask&0x18)) return;
  if((vAddr&0x7000)!=0x7000) vAddr+=0x1000;
  else {
    vAddr&=~0x7000; int y=(vAddr&0x3E0)>>5;
    if(y==29){ y=0; vAddr^=0x800; } else if(y==31) y=0; else y++;
    vAddr=(vAddr&~0x3E0)|(y<<5);
  }
  vAddr=(vAddr&~0x41F)|(tAddr&0x41F);
}

// ---------- CPU bus ----------
static u8 ram[2048];
static volatile u8 padState=0; static u8 padShift=0; static bool padStrobe=false;
static int stallCycles=0;
static u16 pc; static u8 rA,rX,rY,rS,rP;

static u8 rd(u16 a){
  if(a<0x2000) return ram[a&0x7FF];
  if(a<0x4000) return ppuRegRead(a&7);
  if(a==0x4016){ u8 r; if(padStrobe) r=padState&1; else { r=padShift&1; padShift=(padShift>>1)|0x80; } return r|0x40; }
  if(a<0x6000) return 0;
  if(a<0x8000) return prgRam[a-0x6000];
  return prgRead(a);
}
static void wr(u16 a,u8 v){
  if(a<0x2000){ ram[a&0x7FF]=v; return; }
  if(a<0x4000){ ppuRegWrite(a&7,v); return; }
  if(a==0x4014){ for(int i=0;i<256;i++) oam[(u8)(oamAddr+i)]=rd((v<<8)|i); stallCycles+=513; return; }
  if(a==0x4016){ if(v&1) padStrobe=true; else { if(padStrobe) padShift=padState; padStrobe=false; } return; }
  if(a<0x6000) return;
  if(a<0x8000){ prgRam[a-0x6000]=v; return; }
  mapperWrite(a,v);
}

// ---------- CPU 6502 ----------
static const u8 cyc[256]={
7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,
2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, 2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,
2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,
2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, 2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7};

static inline u16 rd16(u16 a){ return rd(a)|(rd(a+1)<<8); }
static inline void push(u8 v){ wr(0x100+rS,v); rS--; }
static inline u8 pull(){ rS++; return rd(0x100+rS); }
static inline void setZN(u8 v){ rP=(rP&0x7D)|(v&0x80)|(v?0:2); }
static void adc(u8 m){ int s=rA+m+(rP&1); rP&=0xBE; if(s>0xFF) rP|=1; if((~(rA^m))&(rA^s)&0x80) rP|=0x40; rA=(u8)s; setZN(rA); }
static void cmp(u8 r,u8 m){ rP=(rP&0x7C)|(r>=m?1:0); setZN((u8)(r-m)); }
static u8 doShift(int k,u8 v){
  u8 r, c=rP&1;
  switch(k){
    case 0: rP=(rP&0xFE)|(v>>7); r=v<<1; break;
    case 1: rP=(rP&0xFE)|(v>>7); r=(v<<1)|c; break;
    case 2: rP=(rP&0xFE)|(v&1); r=v>>1; break;
    default: rP=(rP&0xFE)|(v&1); r=(v>>1)|(c<<7); break;
  }
  setZN(r); return r;
}
static u16 getAddr(int m,bool& cr){
  u16 b,a; u8 z;
  switch(m){
    case 0: z=rd(pc++)+rX; return rd(z)|(rd((u8)(z+1))<<8);
    case 1: return rd(pc++);
    case 2: return pc++;
    case 3: a=rd16(pc); pc+=2; return a;
    case 4: z=rd(pc++); b=rd(z)|(rd((u8)(z+1))<<8); a=b+rY; cr=((a^b)&0xFF00)!=0; return a;
    case 5: return (u8)(rd(pc++)+rX);
    case 6: b=rd16(pc); pc+=2; a=b+rY; cr=((a^b)&0xFF00)!=0; return a;
    case 7: b=rd16(pc); pc+=2; a=b+rX; cr=((a^b)&0xFF00)!=0; return a;
    default: return (u8)(rd(pc++)+rY);
  }
}

static int cpuStep(){
  if(stallCycles){ int s=stallCycles; stallCycles=0; return s; }
  if(nmiPending){ nmiPending=false; push(pc>>8); push(pc&0xFF); push((rP&~0x10)|0x20); rP|=4; pc=rd16(0xFFFA); return 7; }
  u8 op=rd(pc++);
  int cy=cyc[op];
  switch(op){
    case 0x00: pc++; push(pc>>8); push(pc&0xFF); push(rP|0x30); rP|=4; pc=rd16(0xFFFE); return 7;
    case 0x20: { u16 t=rd16(pc); u16 r=pc+1; push(r>>8); push(r&0xFF); pc=t; return 6; }
    case 0x40: rP=(pull()&0xEF)|0x20; pc=pull(); pc|=pull()<<8; return 6;
    case 0x60: pc=pull(); pc|=pull()<<8; pc++; return 6;
    case 0x4C: pc=rd16(pc); return 3;
    case 0x6C: { u16 p=rd16(pc); pc=rd(p)|(rd((p&0xFF00)|((p+1)&0xFF))<<8); return 5; }
    case 0x08: push(rP|0x30); return 3;
    case 0x28: rP=(pull()&0xEF)|0x20; return 4;
    case 0x48: push(rA); return 3;
    case 0x68: rA=pull(); setZN(rA); return 4;
    case 0x88: rY--; setZN(rY); return 2;
    case 0xA8: rY=rA; setZN(rY); return 2;
    case 0xC8: rY++; setZN(rY); return 2;
    case 0xE8: rX++; setZN(rX); return 2;
    case 0x18: rP&=~1; return 2;
    case 0x38: rP|=1; return 2;
    case 0x58: rP&=~4; return 2;
    case 0x78: rP|=4; return 2;
    case 0x98: rA=rY; setZN(rA); return 2;
    case 0xB8: rP&=~0x40; return 2;
    case 0xD8: rP&=~8; return 2;
    case 0xF8: rP|=8; return 2;
    case 0x8A: rA=rX; setZN(rA); return 2;
    case 0x9A: rS=rX; return 2;
    case 0xAA: rX=rA; setZN(rX); return 2;
    case 0xBA: rX=rS; setZN(rX); return 2;
    case 0xCA: rX--; setZN(rX); return 2;
    case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: return 2;
  }
  if((op&0x1F)==0x10){
    static const u8 fl[4]={0x80,0x40,0x01,0x02};
    bool f=(rP&fl[op>>6])!=0, want=((op>>5)&1)!=0;
    s8 off=(s8)rd(pc++);
    if(f==want){ cy++; u16 np=pc+off; if((np^pc)&0xFF00) cy++; pc=np; }
    return cy;
  }
  int cc=op&3, bbb=(op>>2)&7, aaa=op>>5;
  bool cr=false; u16 ad;
  if(cc==1||cc==3){
    if(op==0x89){ pc++; return 2; }
    ad=getAddr(bbb,cr);
    if(cc==3) return cy;
    switch(aaa){
      case 0: rA|=rd(ad); setZN(rA); break;
      case 1: rA&=rd(ad); setZN(rA); break;
      case 2: rA^=rd(ad); setZN(rA); break;
      case 3: adc(rd(ad)); break;
      case 4: wr(ad,rA); return cy;
      case 5: rA=rd(ad); setZN(rA); break;
      case 6: cmp(rA,rd(ad)); break;
      default: adc((u8)~rd(ad)); break;
    }
    return cy+(cr?1:0);
  }
  if(cc==2){
    if(bbb==2){ rA=doShift(aaa,rA); return 2; }
    if(bbb==4||bbb==6) return cy;
    if(bbb==0){ if(aaa==5){ rX=rd(pc++); setZN(rX); return 2; } pc++; return 2; }
    int m=bbb;
    if(aaa==4||aaa==5){ if(bbb==5) m=8; else if(bbb==7) m=6; }
    if(aaa==4&&bbb==7){ pc+=2; return cy; }
    ad=getAddr(m,cr);
    switch(aaa){
      case 4: wr(ad,rX); return cy;
      case 5: rX=rd(ad); setZN(rX); return cy+(cr?1:0);
      case 6: { u8 v=rd(ad)-1; wr(ad,v); setZN(v); return cy; }
      case 7: { u8 v=rd(ad)+1; wr(ad,v); setZN(v); return cy; }
      default: { u8 v=doShift(aaa,rd(ad)); wr(ad,v); return cy; }
    }
  }
  // cc==0
  if(bbb==2||bbb==4||bbb==6) return cy;
  ad=getAddr(bbb==0?2:bbb,cr);
  switch(aaa){
    case 1: if(bbb==1||bbb==3){ u8 v=rd(ad); rP=(rP&0x3D)|(v&0xC0)|((rA&v)?0:2); } break;
    case 4: if(bbb!=0) wr(ad,rY); return cy;
    case 5: rY=rd(ad); setZN(rY); break;
    case 6: if(bbb==0||bbb==1||bbb==3) cmp(rY,rd(ad)); break;
    case 7: if(bbb==0||bbb==1||bbb==3) cmp(rX,rd(ad)); break;
  }
  return cy+(cr?1:0);
}

// ---------- Frame ----------
// ---------- Cheat ----------
static std::mutex mx;
static u8 snap[2048]; static bool cand[2048]; static bool snapOk=false;
struct Frz{ u16 a; u8 v; };
static std::vector<Frz> frz;
static void applyFreeze(){ for(size_t i=0;i<frz.size();i++) ram[frz[i].a]=frz[i].v; }
static void cheatClear(){ snapOk=false; frz.clear(); memset(cand,0,sizeof cand); }
extern "C" {
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_searchReset(JNIEnv*,jclass){
  std::lock_guard<std::mutex> l(mx); memcpy(snap,ram,2048);
  for(int i=0;i<2048;i++) cand[i]=true; snapOk=true; }
JNIEXPORT jint JNICALL Java_com_brruham_nes_NesCore_searchFilter(JNIEnv*,jclass,jint mode,jint val){
  std::lock_guard<std::mutex> l(mx); if(!snapOk) return -1; int n=0;
  for(int i=0;i<2048;i++){
    if(!cand[i]) continue;
    u8 c=ram[i], p=snap[i]; bool ok;
    switch(mode){ case 0: ok=(c==(u8)val); break; case 1: ok=(c!=p); break; case 2: ok=(c==p); break; case 3: ok=(c>p); break; default: ok=(c<p); }
    cand[i]=ok; if(ok) n++;
  }
  memcpy(snap,ram,2048); return n; }
JNIEXPORT jint JNICALL Java_com_brruham_nes_NesCore_searchCount(JNIEnv*,jclass){
  std::lock_guard<std::mutex> l(mx); if(!snapOk) return -1; int n=0;
  for(int i=0;i<2048;i++) if(cand[i]) n++; return n; }
JNIEXPORT jintArray JNICALL Java_com_brruham_nes_NesCore_searchGet(JNIEnv* env,jclass,jint mx_){
  std::lock_guard<std::mutex> l(mx); std::vector<jint> o;
  for(int i=0;i<2048 && (int)o.size()<mx_;i++) if(snapOk && cand[i]) o.push_back((i<<8)|ram[i]);
  jintArray r=env->NewIntArray((jsize)o.size());
  if(!o.empty()) env->SetIntArrayRegion(r,0,(jsize)o.size(),o.data());
  return r; }
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_poke(JNIEnv*,jclass,jint a,jint v){
  std::lock_guard<std::mutex> l(mx); if(a>=0&&a<2048) ram[a]=(u8)v; }
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_freeze(JNIEnv*,jclass,jint a,jint v,jboolean on){
  std::lock_guard<std::mutex> l(mx);
  for(size_t i=0;i<frz.size();){ if(frz[i].a==a) frz.erase(frz.begin()+i); else i++; }
  if(on && a>=0 && a<2048){ Frz f; f.a=(u16)a; f.v=(u8)v; frz.push_back(f); } }
JNIEXPORT jintArray JNICALL Java_com_brruham_nes_NesCore_freezeGet(JNIEnv* env,jclass){
  std::lock_guard<std::mutex> l(mx); std::vector<jint> o;
  for(size_t i=0;i<frz.size();i++) o.push_back((frz[i].a<<8)|frz[i].v);
  jintArray r=env->NewIntArray((jsize)o.size());
  if(!o.empty()) env->SetIntArrayRegion(r,0,(jsize)o.size(),o.data());
  return r; }
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_freezeClear(JNIEnv*,jclass){
  std::lock_guard<std::mutex> l(mx); frz.clear(); }
}
static int dotBudget=0;
static void runDots(int n){ dotBudget+=n; while(dotBudget>0){ dotBudget-=3*cpuStep(); applyFreeze(); } }
static void emuFrame(){
  for(int line=0;line<262;line++){
    if(line==241){ pStatus|=0x80; if(pCtrl&0x80) nmiPending=true; }
    if(line==261){ pStatus&=~0xE0; if(pMask&0x18) vAddr=(vAddr&0x841F)|(tAddr&0x7BE0); }
    if(line<240){ runDots(256); renderLine(line); endLineUpdate(); runDots(85); }
    else runDots(341);
  }
}
static void resetAll(){
  memset(ram,0,sizeof ram); memset(nt,0,sizeof nt); memset(pal,0,sizeof pal); memset(oam,0,sizeof oam);
  memset(prgRam,0,sizeof prgRam); memset(chrRamBuf,0,sizeof chrRamBuf); memset(fb,0,sizeof fb);
  bankReg=chrReg=0; mShift=0x10; mCtrl=0x0C; mChr0=mChr1=mPrg=0;
  vAddr=tAddr=0; fineX=0; wLatch=false; pCtrl=pMask=pStatus=oamAddr=readBuf=0;
  nmiPending=false; dotBudget=0; stallCycles=0; padStrobe=false; padShift=0;
  rA=rX=rY=0; rS=0xFD; rP=0x24; pc=rd16(0xFFFC);
}

extern "C" {
JNIEXPORT jint JNICALL Java_com_brruham_nes_NesCore_load(JNIEnv* env,jclass,jbyteArray arr){
  jsize n=env->GetArrayLength(arr);
  std::vector<u8> d(n);
  env->GetByteArrayRegion(arr,0,n,(jbyte*)d.data());
  if(n<16||d[0]!='N'||d[1]!='E'||d[2]!='S'||d[3]!=0x1A) return 1;
  int pb=d[4], cb=d[5], f6=d[6], f7=d[7];
  int mp=(f6>>4)|(f7&0xF0);
  if(mp!=0&&mp!=1&&mp!=2&&mp!=3&&mp!=7) return 100+mp;
  size_t off=16+((f6&4)?512:0), ps=(size_t)pb*16384, cs=(size_t)cb*8192;
  if(pb==0||off+ps+cs>(size_t)n) return 2;
  prg.assign(d.begin()+off,d.begin()+off+ps);
  chr.assign(d.begin()+off+ps,d.begin()+off+ps+cs);
  chrRam=(cb==0); prgBanks=pb; mapper=mp;
  mirrorMode=(f6&1)?1:0; if(mapper==7) mirrorMode=2;
  resetAll(); cheatClear();
  return 0;
}
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_reset(JNIEnv*,jclass){ if(!prg.empty()) resetAll(); }
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_runFrame(JNIEnv*,jclass){ std::lock_guard<std::mutex> lk(mx); if(!prg.empty()) emuFrame(); }
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_getFrame(JNIEnv* env,jclass,jintArray out){
  env->SetIntArrayRegion(out,0,256*240,(const jint*)fb);
}
JNIEXPORT void JNICALL Java_com_brruham_nes_NesCore_setInput(JNIEnv*,jclass,jint m){ padState=(u8)m; }
}
