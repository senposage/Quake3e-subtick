#ifdef ENCRYPTED
/*

 Runtime strings decryption by r00t
 ----------------------------------

 Examples:
  char *blah = "blah!";        // This string will be encrypted in QVM
                                   // Use _S() for initializing structs, then later  to get string.

  trap_Printf(DecryptStr(blah));   // Decrypts string into temp buffer, then returns it (similar to va())
                                   // String stays encrypted in memory, it's decrypted every time line is executed
                                   // Do NOT use for cvar_register and other functions that store string pointer for long-term use.

  char tmp[32];                    // strcpy() decrypts string and copies it to supplied buffer, just like strcpy().
  strcpy(blah,tmp);             // Use instead of strcpy(tmp,DecryptStr(blah)), it's faster.

  trap_Printf(blah); //  decrypts string in QVM memory on first call,
                                   // when called again, it simply returns decrypted string pointer.
                                   // No temp buffer is used, string is decrypted in place.
                                   // This safe for cvar_register and other functions that store string address.

  trap_Printf("Test12345");    // _X() can be use to simply encrypt strings passed to functions.
                                   // String will be decrypted on first use and returned pointer is fixed (no temp buffers)
                                   // _X() only makes sense when calling functions, will not work in static variable defines.


 When ENCRYPTED isn't defined, all decryption/encryption is skipped.

 Detailed encryption process:
  1) _S() adds 4 extra bytes at the end of string
  2) .c/.h is compiled to .asm using LCC
  3) qvmencstr is run on all .asm files, it detects the extra bytes marking string to be encrypted,
     replaces it with 32bit key + encrypted string bytes.
  4) QVM is compiled using q3asm

 Detailed decryption process:
  1) First 4 bytes from string are read, if all zeroes, string is already decrypted, so return ptr to string+4
  2) Decrypt string bytes, either in-place (DECRYPT_ONCE) or to supplied buffer (strcpy)
  3) In case of in-place decryption, first 4 bytes are overwritten with zero to mark string as decrypted


*/

#define _S(x) (x"\x00\xF1\xC0\xDE") // this marks strings to be encrypted during .asm processing

char *DecryptStr(char *str);
void strcpy(char *str, char *dst);


char *DecryptStrInPlace(char *str);
// returns pointer to string, marks it as decrypted -> use macro below instead:

#define in ((*(unsigned int*)(in)==(((unsigned int)(in)) ))?((in)+4):DecryptStrInPlace(in))
// DecryptOnce = string decrypted on 1st use, inplace, small overhead


#define _X(x) _S(x)
// Define local encrypted string, will be decrypted on first use. Safe for later pointer references.

#else


#define _S(x) x
#define _X(x) x
#define x x
#define strcpy(x,y) strcpy(y,x)

#endif
