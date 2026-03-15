
#ifndef _C_BMALLOC_H_
#define _C_BMALLOC_H_

int add_pool(void *start, size_t size);
void print_lists(void);

void *bmalloc(size_t size);
void bfree(void *ptr);

void bclear(void);

#ifndef Q3_VM
#include "stdlib.h"
//#define bmalloc malloc
//#define bfree free
#endif

#endif
