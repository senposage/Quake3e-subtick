#ifdef ENCRYPTED

#include "../game/q_shared.h"
#include "c_syscalls.h"

extern unsigned int __lastval;
extern char __zdeckey[];
extern char __laststr[];

// NOTE: Following code was written to produce nasty and obfuscated QVM code
//       It's not really nice to read, but I tried to add some comments and mark stuff that does nothing.


// following defines must be same as in qvmenc.c !
#define KEY0 0 // key bytes offsets
#define KEY1 2
#define KEY2 3
#define KEY3 1

#define SHF1A 6  // shift register taps for data
#define SHF1B 28
#define SHF1C 30

#define SHF2A 9  // shift register taps for lit
#define SHF2B 20
#define SHF2C 31

#define XORBUFXOR 0x49A5F3C7

// "lol" parameters can be anything
// "zero" parameters needs to be zero

unsigned int *GetVal0(int lol1, int lol2, int lol3, unsigned char *xorbuf)
// Fancy way to get zero value == try to open random file that will fail
// Also fills xorbuf with simple sequence (xorbuf[n] = n XOR lol3)
{
 char junk[32];
 int i = 0;
 unsigned int ret;
 fileHandle_t fh;
 unsigned char *xx = ((unsigned char*)&__lastval)+lol1; // make it look like we are decrypting filename
 junk[sizeof(junk)-1]=0;
 while(i<sizeof(junk)-1) {
  switch (i) {
   case -1:
    while(i--) {
    junk[i++]=0x23+(*(xx++)%lol3);
   case -5:
    junk[i++]=0x2F+(*(xx++)%lol2);
   case -6:
    junk[i++]=0x29+(*(xx++)%lol2);
    }
    break;
   default: // only this matters
    junk[i++]=0x21+(*(xx++)%lol3);
    break;
  }
 }
 memset(xorbuf,junk[i],sizeof(xorbuf));
 ret = trap_FS_FOpenFile( junk, &fh, FS_READ );
 if (ret>0) { // get them thinking we are loading something
  trap_FS_Read(xorbuf,ret,fh);
  trap_FS_FCloseFile(fh);
 }
 for(i=0;i<256;i+=2) { // init xorbuf, ret is unchanged
  xorbuf[i+0]^=(i+0)^(lol3++);
  ret+=lol3;
  xorbuf[i+1]^=(i+1)^(--lol3);
  ret-=lol3+1;
 }
 return (unsigned int*)(ret+1); // Open returns -1 on error, so +1 to return 0
}

unsigned int GetKey(int zero, unsigned int *dest)
// Reads last 4 bytes from LIT and returns their 32bit value
{
 unsigned int out = 0;
 unsigned int select = 0x4040C000|KEY0|(KEY1<<8)|(KEY2<<16)|(KEY3<<24);
 // this word encodes key positions j=junk XXX^YYY = key pos
 // jjXXXYYYjjXXXYYYjjXXXYYYjjXXXYYY
 // 01101110010110111101101000110100 = 0x6E5BDA34
 //   KEY3=3  KEY2=0  KEY1=1  KEY0=2
 // 0x4040C000 = junk bits = 01 01 11 00 / reversed = 00 11 01 01 = 0x35 = 53 dec -> used later!
 unsigned char *key = (unsigned char*)__zdeckey+zero;
 unsigned int junk = 0;
 while(select) {
  int p; // position in key 0..4
  p = ((select&0x38)>>(3+zero))<<1; // select 3 bits from 8:   00111000
  p^= (select&7)<<(zero+1);         // xor with 3 bits from 8: 00000111
  junk = (junk<<2)|((select&0xC0)>>6);
  switch (p) { // 8-14 useless, result in bad references to undecrypted LIT segment
   case 8:
    out = (out<<8)|(key[3]%zero);
    break;
   case 10:
    out = (out<<8)|(key[2]%zero);
    break;
   case 12:
    out = (out<<8)|(key[1]%zero);
    break;
   case 14:
    out = (out<<8)|(key[0]%zero);
    break;
   default: // only this matters
    out = (out<<8)|key[p/2];
    break;
  }
  select>>=8;
 }
 *dest = out;
 return junk^zero;
}


// bit packed taps (6*5 bits)
#define TAPS (SHF1A|(SHF1B<<5)|(SHF1C<<10)|(SHF2A<<15)|(SHF2B<<20)|(SHF2C<<25))
#define TXOR 0x58F72521 // xor value with this
#define TXENDMARK 2 // value at the end to exit loop
void GetTaps(unsigned char *t, int val3, int val5)
// Get SHFxx tap values from encoded words
{
 unsigned int ta = (TAPS | (TXENDMARK<<30)) ^ TXOR;
 unsigned int tx = (TXOR<<3) | (TXENDMARK ^ (TXOR>>30));
 unsigned int txend = tx&val3;
 while(val5==val3) { // never happens
  int tmp = val5;
  val5^=val3;
  val3=31-tmp;
 }
 tx>>=val3;
 while(ta!=txend) {
  *(t++) = (ta^tx)&((1<<val5)-1);
  switch(val5) {
   case 1:
    ta=(ta>>val3)^0xF79AD470;
    break;
   case 2:
    ta=(ta>>val3)^0x76E3BC6E;
    break;
   case 3:
    ta=(ta>>val3)^0xA96CD12D;
    break;
   case 4:
    ta=(ta>>val3)^0xDB738D91;
    break;
   default: // only this matters
    ta>>=val5;
    tx>>=val5;
  }
 }
}

void FillXorBuf(unsigned char *xorbuf,unsigned int sr)
// Fill xorbuf with pseudo-random data generated from sr
// xorbuf is contains xorbuf[n] = n^0xAB sequence from GetVal0()
{
 unsigned int sh = sr^XORBUFXOR;
 int i,j;
 int r = 10;
 unsigned char tmp[16]; // must start with 1^r,15^r,29^r,...
 while(r) {
  tmp[0]=1;
  for(i=1;i<16;i++) {
   switch(r) {
    case 12:
     tmp[i]=17;
     tmp[i-2]^=r++^i--;
     break;
    case 13:
     tmp[i]=15;
     tmp[i-2]^=r--^i++;
     break;
    case 14:
     tmp[i]=9;
     tmp[i-2]^=r++^i--;
     break;
    case 15:
     tmp[i]=40;
     tmp[i-2]^=r--^i++;
     break;
    default: // only this matters
     tmp[i]=tmp[i-1]+14;
     tmp[i-1]^=r;
     break;
   }
  }
  for(i=0;i<256;i++) {
   for(j=0;j<32;j++) {
    unsigned int bit = ((sh>>tmp[0])^(sh>>tmp[1])^(sh>>tmp[2]))&1;
    sh=(sh<<1)|bit;
   }
   xorbuf[i]^=sh;
  }
  r--;
 }
}

void zDecrypt()
// Main decryption function
{
 int i,j;
 unsigned char *lit;
 unsigned char xorbuf[256];
 unsigned int *val = GetVal0(-93,39,0xAB,xorbuf);
 unsigned int *last_val = (unsigned int*)(&__lastval)+1+(int)val;
 unsigned char *lit_end = (unsigned char*)__laststr+4; /* 4 == sizeof(__laststr);*/
 unsigned int sr, sr2;
 unsigned int junk53;
 unsigned char taps[6];
 unsigned char V[4]; // 8,16,24,32

 *(int*)V = trap_Milliseconds(); // useless
 do {
  junk53 = GetKey((int)val,&sr);
  *(int*)V = (*(int*)V*333+trap_Milliseconds())%1337000; // useless
 } while ((junk53%10 != (junk53>>4)) || (junk53/10 != (junk53&0xF))); // 0x35 == 53 dec, check that 3==3 and 5==5
 GetTaps(taps,junk53%10,junk53/10);
 FillXorBuf(xorbuf,sr);
 for(i=0;i<4;i++) V[i]=(i+1)<<3;

// Enable if you want to run non-encrypted QVM (or remove call to zDecrypt from vmmain)
// if (sr==0x25316400) return; // QVM not encrypted

 // Decrypt 32bit data
 sr2 = sr;
 while(val!=last_val) {
  unsigned int wrd = 0;
  for(j=wrd;j<V[3];) {
   unsigned int bit = ((sr>>taps[0])^(sr>>taps[1])^(sr>>taps[2]))&1;
   sr=(sr<<1)|bit;
   wrd^=sr;
   xorbuf[sr&0xFF]^=(sr>>V[0])&0xFF;
   xorbuf[(sr>>V[1])&0xFF]^=(sr>>V[2])&0xFF;
   j+=junk53&3; // j+=1
   sr2^=sr;
  }
  sr2^=sr2>>7;
  sr2^=sr2>>21;
  sr2^=sr2<<17;
  sr^=sr2;
  val[0]^=xorbuf[wrd&0xFF]|(xorbuf[(wrd>>V[0])&0xFF])|(xorbuf[(wrd>>V[1])&0xFF])|(xorbuf[(wrd>>V[2])&0xFF]);
  val[0]^=wrd;
  sr^=val[0];
  val++;
 }

 // Decrypt literals
 lit = (unsigned char*)(val);
 while(lit!=lit_end) {
  unsigned int wrd = 0;
  for(j=wrd;j<V[3];) {
   unsigned int bit = ((sr>>taps[3])^(sr>>taps[4])^(sr>>taps[5]))&1;
   sr=(sr<<1)|bit;
   wrd^=sr;
   xorbuf[sr&0xFF]^=(sr>>V[0])&0xFF;
   xorbuf[(sr>>V[1])&0xFF]^=(sr>>V[2])&0xFF;
   j+=junk53&3; // j+=1
   sr2^=sr;
  }
  sr2^=sr2>>7;
  sr2^=sr2<<17;
  sr2^=sr2>>21;
  sr^=sr2;
  lit[0]^=xorbuf[wrd&0xFF];
  lit[0]^=wrd;
  sr^=lit[0];
  lit++;
 }

 // Mark QVM as decrypted in case it's restated
/* not needed, handled by static variable in vmmain()
 __zdeckey[0]='%';
 __zdeckey[1]='1';
 __zdeckey[2]='d';
 __zdeckey[3]=0;
*/

 // writes data memory, good for debugging if decrypted OK
#if 0
 {
  char fn[4];
  fileHandle_t  f;
  fn[0]='d';
  fn[1]='e';
  fn[2]='c';
  fn[3]=0;
  if (trap_FS_FOpenFile(fn, &f, FS_WRITE) >= 0) {
   trap_FS_Write((void*)4, (int)__laststr+4,f);
   trap_FS_FCloseFile(f);
  }
 }
#endif

 // die with error in case of bad decryption... in release, better just let it die
 while (__lastval!=0x548697AA || __laststr[0]!='%' || __laststr[1]!='6' || __laststr[2]!='s' || __laststr[3]!=0) {
#if 0
  char err[16];
  err[0]='d';
  err[1]='e';
  err[2]='c';
  err[3]=' ';
  err[4]='e';
  err[5]='r';
  err[6]='r';
  err[7]=0;
  trap_Error(err);
  return;
#endif
 }

#if 0
 { // debug hexdump function, too nice to delete
  static char hex[16] = "0123456789ABCDEF";
  int i;
  for(i=0;i<256;i++) {
   char tmp[4];
   tmp[0]=hex[xorbuf[i]>>4];
   tmp[1]=hex[xorbuf[i]&0xF];
   tmp[2]=0;
   trap_Print(tmp);
  }
  trap_Print("\n");
 }
#endif

}

#define MAXENCSTRLEN 1024
#define NUMENCSTRBUFFS 4

void strcpy(char *str, char *dst)
{
 unsigned char *us = (unsigned char *)str;
 unsigned char *ob = (unsigned char *)dst;
 int i,j;
 unsigned int sr = us[3]|(us[2]<<8)|(us[1]<<16)|(us[0]<<24);
 us+=4;
 i=0;
 while(1) {
  sr^=sr>>20;
  sr^=sr<<5;
  sr^=sr>>3;
  ob[0] = us[0] ^ sr;
  if (!ob[0]) return;
  ob++;
  us++;
 }
}

char *DecryptStr(char *str)
{
 static char buf[NUMENCSTRBUFFS][MAXENCSTRLEN];
 static int bufnr = 0;
 if (!str) return NULL;
 bufnr=(bufnr+1)&(NUMENCSTRBUFFS-1);
 strcpy(str,buf[bufnr]);
 return buf[bufnr];
}

char *DecryptStrInPlace(char *str)
{
 strcpy(str+4,DecryptStr(str));
 *(unsigned int*)str=((unsigned int)str);
 return str+4;
}

#else

void zdec_useless_function(); // to suppress empty file warning

#endif
