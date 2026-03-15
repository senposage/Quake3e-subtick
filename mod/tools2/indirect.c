#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
INDIRECT FUNCTIONS CALLS FOR QVM
--------------------------------

This QVM assembler pre-processor looks for asm lines like these:

 ADDRGP4 trap_Print
 CALLV


And replaces them with:

 data
 LABELV _i_234_trap_Print
 address trap_Print
 code
 ADDRGP4 _i_234_trap_Print
 INDIRP4
 CALLV


** Why?
Becase this makes QVM disassembly PITA. Without decrypting the data segments, it's impossible to
know functions called from QVM code. QVM disassembler tools that look for traps are also useless
because they can't process this indirect addressing.

** Cons?
It adds one extra instruction (results in 1-3 instruction in compiled x86 QVM code) for every
function called. This should not have any visible impact on speed.

** Ignore? What is that for?
Because this indirect calling requires data segment, it can't be for functions decrypting data memory.
All these functions must be added to ignore like. Typical example would be:
"!zDecrypt,@zDecrypt,@GetVal0,@GetKey,@GetTaps,@FillXorBuf"
(Don't change calls to zDecrypt from vmMain, don't touch any calls in functions used by zDecrypt.)

*/


int addr_cntr = 0;
char *ignore = NULL;

int PatchStr(char *out, int olen, int *done, char *proc)
{
 char *cl = 0;
 char *fn = out+8;
 char *ign;
 char tmp[1024];
 char fn_old[256];
 char fn_new[256];
 int ol;
 int nl;

 if (!proc[0]) return olen;
 out[olen]=0;
 cl = strstr(out,"CALL");
 if (!cl) return olen;
 while(*fn>' ') fn++;
 strncpy(fn_old,out+8,fn-out-8);
 fn_old[fn-out-8]=0;

// printf("proc=%s call=%s\n",proc,fn_old);
 if (ignore) {
  nl = sprintf(tmp,"!%s",fn_old);
  ign = strstr(ignore,tmp);
  if (ign && (!ign[nl] || ign[nl]==',')) {
//   printf("-- ignored call to function %s\n",fn_old);
   return olen;
  }
  nl = sprintf(tmp,"@%s",proc);
  ign = strstr(ignore,tmp);
  if (ign && (!ign[nl] || ign[nl]==',')) {
//   printf("-- ignored call in function %s\n",proc);
   return olen;
  }
 }

 nl = sprintf(fn_new,"_i_%d_%s",addr_cntr++,fn_old);

 ol = sprintf(tmp,";*** indirect proc=%s call=%s\n",proc,fn_old);
 ol+= sprintf(ol+tmp,"data\nLABELV %s\naddress %s\n" "code\nADDRGP4 %s\nINDIRP4\n%s\n\n",fn_new,fn_old,fn_new,cl);

 strcpy(out,tmp);
 done[0]++;

 return ol;
}

int ProcessFile(char *infile, char *outfile)
{
 int onum = 0;
 int done = 0;
 char *out = malloc(1024*1024*8); // 8MB should be enough for every .asm
 char buf2[1024*64];
 int b2num = 0;
 char ln[1024];
 char proc_name[64];
 int seg = 0; // lit,data,code
 int inlabel = 0;

 FILE *f = fopen(infile,"rt");
 if (!f) {
  printf("ERROR: can't open asm file %s\n",infile);
  return -1;
 }

 proc_name[0] = 0;

 while(1) {
  int sl,flushb2,addtob2;
  ln[0]=0;
  fgets(ln,1024,f);
  if (!ln[0]) break;
  sl = strlen(ln);
  flushb2 = 0;
  addtob2 = 0;
  if (!strncmp(ln,"lit",3) && ln[3]<' ') {
   seg=1;
   inlabel = 0;
   flushb2 = 1;
  } else
  if (!strncmp(ln,"data",4) && ln[4]<' ') {
   seg=2;
   inlabel = 0;
   flushb2 = 1;
  } else
  if (!strncmp(ln,"code",4) && ln[4]<' ') {
   seg=3;
   inlabel = 0;
   flushb2 = 1;
  } else
  if (seg==3 && !strncmp(ln,"proc ",5)) {
   char *x = ln+5;
   int pl = 0;
   while(*x>' ') proc_name[pl++] = *(x++);
   proc_name[pl]=0;
   inlabel = 0;
   flushb2 = 1;
  } else
  if (seg==3 && !strncmp(ln,"ADDRGP4 ",8) && ln[8]!='$') {
   inlabel = 1;
   flushb2 = 1;
   addtob2 = 1;
  } else
  if (inlabel && seg==3 && !strncmp(ln,"CALL",4)) {
//   printf("%s\n",ln);
   addtob2 = 1;
  } else {
   inlabel = 0;
   flushb2 = 1;
  }

  if (flushb2 && b2num) {
   b2num = PatchStr(buf2,b2num,&done,proc_name);
   memcpy(out+onum,buf2,b2num);
   onum+=b2num;
   b2num=0;
  }
  if (addtob2) {
   memcpy(buf2+b2num,ln,sl);
   b2num+=sl;
  } else {
   memcpy(out+onum,ln,sl);
   onum+=sl;
  }
 }
 fclose(f);
 if (b2num) {
  b2num = PatchStr(buf2,b2num,&done,proc_name);
  memcpy(out+onum,buf2,b2num);
  onum+=b2num;
 }
 if (done || outfile!=infile) {
  // write to output
  f = fopen(outfile,"wt");
  fwrite(out,1,onum,f);
  fclose(f);
 }
 free(out);
 return done;
}

int __cdecl main(int argc, char **argv)
{
 if (argc<2) {
  printf("qvmencstr [input ASM file] {output ASM file (optional)} {ignore}\n -or-\n");
  printf("qvmencstr [.q3asm script to process all referenced asm files] {ignore}\n");
  printf("\nignore format: [!|@]func1,[!|@]func2,...\n    ! = do not modify calls to this function\n    @ = do not modify calls in this function\nExample: !zDecrypt,@zDecrypt,@GetVal0,@GetKey,@GetTaps,@FillXorBuf\n");
  return 1;
 }


 if (strstr(argv[1],".q3asm")) {
  char ln[256];
  FILE *f = fopen(argv[1],"rt");
  if (!f) {
   printf("can't open q3asm script file %s\n",argv[1]);
   return 1;
  }
  if (argc>2) ignore = argv[2];
  while(1) {
   int sl,d;
   ln[0]=0;
   fgets(ln,256,f);
   if (!ln[0]) break;
   if (ln[0]=='-') continue; // skip options
   sl = strlen(ln);
   while(ln[sl-1]=='\r' || ln[sl-1]=='\n') ln[--sl]=0;
   strcpy(ln+sl,".asm");
   printf("Patching jumps in %-30s : ",ln);
   d = ProcessFile(ln,ln);
   if (d>=0) printf("DONE (%d)\n",d);
  }
  fclose(f);
 } else {
  if (argc>3) ignore = argv[3];
  ProcessFile(argv[1],(argc>2)?argv[2]:argv[1]);
 }
 return 0;
}
