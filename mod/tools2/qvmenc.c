#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define U8 unsigned char
#define U32 unsigned int
#define I32 int

// r00t's QVM encryptor
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
//

#define VM_MAGIC        0x12721444
#define VM_MAGIC_VER2   0x12721445

typedef struct {
    I32     vmMagic;

    I32     instructionCount;

    I32     codeOffset;
    I32     codeLength;

    I32     dataOffset;
    I32     dataLength;
    I32     litLength;          // ( dataLength - litLength ) should be byteswapped on load
    I32     bssLength;          // zero filled memory appended to datalength

    //!!! below here is VM_MAGIC_VER2 !!!
    I32     jtrgLength;         // number of jump table targets
} vmHeader_t;

void FillXorBuf(U8 *xorbuf,U32 sr)
{
 U32 sh = sr^XORBUFXOR;
 int i,j;
 int r = 10;
 while(r) {
  int t1 = 1^r;
  int t2 = 15^r;
  int t3 = 29^r;
  for(i=0;i<256;i++) {
   for(j=0;j<32;j++) {
    U32 bit = ((sh>>t1)^(sh>>t2)^(sh>>t3))&1;
    sh=(sh<<1)|bit;
   }
   xorbuf[i]^=sh;
  }
  r--;
 }
// for(i=0;i<256;i++) printf("%02X",xorbuf[i]);
// printf("\n");
}

int __cdecl main(int argc, char **argv)
{
 // load file to memory
 int fl;
 FILE *f;
 U8 *buf;
 vmHeader_t *h;
 int i;
 U32 sr,sr2;
 U8 xorbuf[256];
 U8 *lit;
 int litlen;


 if (argc!=3) {
  printf("qvmenc [input QVM file] [output QVM file]\n");
  return 1;
 }

 f = fopen(argv[1],"rb");
 if (!f) {
  printf("Can't read input QVM\n");
  return 1;
 }
 fseek(f,0,SEEK_END);
 fl = ftell(f);
 fseek(f,0,SEEK_SET);
 buf = malloc(fl);
 fread(buf,1,fl,f);
 fclose(f);

 // check for magic
 h = (vmHeader_t *)buf;
 if (h->vmMagic != VM_MAGIC && h->vmMagic != VM_MAGIC_VER2) {
  printf("Invalid QVM file magic\n");
  return 1;
 }

 // create key
 srand(time(0));
 sr = 0x3C7A91F4;
 for(i=0;i<h->codeLength;i++) {
  U32 bit = ((sr>>7)^(sr>>13)^(sr>>27))&1;
  sr=(sr<<1)|bit;
  sr^=buf[h->codeOffset+i];
  sr^=rand()<<8;
 }
 printf("KEY: %08X\n",sr);

 for(i=0;i<256;i++) xorbuf[i]=i^0xAB;
 FillXorBuf(xorbuf,sr);

 // put key at the end of QVM file (variable __zdeckey)
 lit = buf+h->dataOffset+h->dataLength;
 litlen = h->litLength;
 while(!lit[litlen-1]) litlen--;
 if (lit[litlen-3]!='%' || lit[litlen-2]!='1' || lit[litlen-1]!='d') {
  printf("invalid qvm ending: %02X %02X %02X (should be %%1d)\n",lit[litlen-3],lit[litlen-2],lit[litlen-1]);
  exit(1);
 }
 litlen-=3;

 lit[litlen+KEY0] = (sr>>24)&0xFF;
 lit[litlen+KEY1] = (sr>>16)&0xFF;
 lit[litlen+KEY2] = (sr>>8)&0xFF;
 lit[litlen+KEY3] = sr&0xFF;

 // encrypt 32bit data
 sr2 = sr;
 {
  U32 *data = (U32*)(buf+h->dataOffset);
  for(i=0;i<h->dataLength;i+=4) {
   U32 wrd = 0;
   int j;
   for(j=0;j<32;j++) {
    U32 bit = ((sr>>SHF1A)^(sr>>SHF1B)^(sr>>SHF1C))&1;
    sr=(sr<<1)|bit;
    wrd^=sr;
    xorbuf[sr&0xFF]^=(sr>>8)&0xFF;
    xorbuf[(sr>>16)&0xFF]^=(sr>>24)&0xFF;
    sr2^=sr;
   }
   sr2^=sr2>>7;
   sr2^=sr2>>21;
   sr2^=sr2<<17;
   sr^=sr2;
   sr^=data[0];
   data[0]^=wrd;
   data[0]^=xorbuf[wrd&0xFF]|(xorbuf[(wrd>>8)&0xFF])|(xorbuf[(wrd>>16)&0xFF])|(xorbuf[(wrd>>24)&0xFF]);
   data++;
  }
  printf("DATA len=%d, key_after=%08X\n",h->dataLength,sr);
 }

 // encrypt strings
 for(i=0;i<litlen;i++) {
  U32 wrd = 0;
  int j;
  for(j=0;j<32;j++) {
   U32 bit = ((sr>>SHF2A)^(sr>>SHF2B)^(sr>>SHF2C))&1;
   sr=(sr<<1)|bit;
   wrd^=sr;
   xorbuf[sr&0xFF]^=(sr>>8)&0xFF;
   xorbuf[(sr>>16)&0xFF]^=(sr>>24)&0xFF;
   sr2^=sr;
  }
  sr2^=sr2>>7;
  sr2^=sr2<<17;
  sr2^=sr2>>21;
  sr^=sr2;
  sr^=lit[i];
  lit[i]^=wrd;
  lit[i]^=xorbuf[wrd&0xFF];
 }
 printf("LIT len=%d, key_after=%08X\n",litlen,sr);

 // write to output
 f = fopen(argv[2],"wb");
 fwrite(buf,fl,1,f);
 fclose(f);
 free(buf);
 return 0;
}
