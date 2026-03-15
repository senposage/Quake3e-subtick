#ifdef ENCRYPTED
// this must be last obj file referenced in .q3asm file

// last values for DATA and LITERAL blocks, also used to check good decryption
unsigned int __lastval = 0x548697AA;
char __laststr[4] = "%6s";
char __zdeckey[4] = "%1d"; // <<< will be replaced by key

#else

void useless_function(); // to suppress empty file warning

#endif
