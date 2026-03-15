/**
 * Filename: c_bufferedfile.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "c_bufferedfile.h"

void C_FileOpen(fileBuffer_t *b, char *filename, fsMode_t mode)
{
	b->size = trap_FS_FOpenFile(filename, &b->handle, mode);
	b->mode = mode;
}

void C_FileClose(fileBuffer_t *b)
{
	trap_FS_FCloseFile(b->handle);
}

int C_FileGetChar(fileBuffer_t *b)
{
	if (b->index < b->size)
	{
		int i = b->index % BUFFER_SIZE;
		if (i == 0)
			trap_FS_Read(b->buffer, BUFFER_SIZE, b->handle);
		b->index++;
		return b->buffer[i];
	}
	else
	{
		return -1;
	}
}

int C_FileRead(fileBuffer_t *b, char *out, int length)
{
	char *start = out;
	if (b->index < b->size)
	{
		while (length && b->index < b->size)
		{
			int i = b->index % BUFFER_SIZE;
			while (i < BUFFER_SIZE && length)
			{
				*out++ = b->buffer[i++];
				--length;
				++b->index;
			}
			if (i == BUFFER_SIZE)
			{
				trap_FS_Read(b->buffer, BUFFER_SIZE, b->handle);
			}
		}
		return out - start;
	}
	else
	{
		return -1;
	}
}
