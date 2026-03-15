/**
 * Filename: bg_gear.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 */
#ifndef _BG_GEAR_H_
#define _BG_GEAR_H_

typedef struct utGear_s {
	int  primary;
	int  secondary;
	int  sidearm;

	int  grenade;

	int  item1;
	int  item2;
	int  item3;
} utGear_t;

void	 utGear_Compress(utGear_t *gear, char *compressed);
void	 utGear_Decompress(char *compressed, utGear_t *gear);
qboolean utGear_Verify(utGear_t *gear);

#define GEAR_ALL     0
#define GEAR_NADES   (1 << 0)
#define GEAR_SNIPERS (1 << 1)
#define GEAR_SPAS    (1 << 2)
#define GEAR_PISTOLS (1 << 3)
#define GEAR_AUTOS   (1 << 4)
#define GEAR_NEGEV   (1 << 5)
#define GEAR_FLASH   (1 << 6)

#endif
