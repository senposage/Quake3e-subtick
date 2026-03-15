/**
 * Filename: c_arieshl.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_ARIESHL_H_
#define _C_ARIESHL_H_

typedef enum {
    HL_UNKNOWN,
    HL_HEAD,
    HL_HELMET,
    HL_TORSO,
    HL_VEST,
    HL_ARML,
    HL_ARMR,
    HL_GROIN,
    HL_BUTT,
    HL_LEGUL,
    HL_LEGUR,
    HL_LEGLL,
    HL_LEGLR,
    HL_FOOTL,
    HL_FOOTR,
    HL_MAX
} ariesHitLocation_t;

typedef struct ariesHitLocationMap_s {
	char *              name;
	ariesHitLocation_t  location;
} ariesHitLocationMap_t;

extern char *c_ariesHitLocationNames[HL_MAX];

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
struct ariesHitLocationMap_s *
in_word_set (register const char *str, register unsigned int len);

#endif /* _C_ARIESHL_H_ */
