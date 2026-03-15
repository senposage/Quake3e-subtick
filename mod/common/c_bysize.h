
#ifndef _C_BYSIZE_H_
#define _C_BYSIZE_H_

void remove_chunksize(void *data);
void insert_bysize(char *data, size_t size);
char *obtainbysize( size_t size);
void print_sizes(void);

#endif
