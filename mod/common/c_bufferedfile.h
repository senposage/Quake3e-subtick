/**
 * Filename: c_bufferedfile.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_BUFFEREDFILE_H_
#define _C_BUFFEREDFILE_H_

#include "c_syscalls.h"

#define BUFFER_SIZE 1024

typedef struct
{
	fileHandle_t handle;
	char buffer[BUFFER_SIZE];
	int index;
	int size;
	fsMode_t mode;
} fileBuffer_t;

#endif /* _C_BUFFEREDFILE_H_ */
