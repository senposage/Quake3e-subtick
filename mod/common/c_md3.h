/**
 * Filename: c_md3.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_MD3_H_
#define _C_MD3_H_

#include "../game/q_shared.h"

#define MD3_IDENT			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION 		15

/* limits */
#define MD3_MAX_LODS		3
#define MD3_MAX_TRIANGLES	8192	/* per surface */
#define MD3_MAX_VERTS		4096	/* per surface */
#define MD3_MAX_SHADERS 	256 	/* per surface */
#define MD3_MAX_FRAMES		1024	/* per model */
#define MD3_MAX_SURFACES	32		/* per model */
#define MD3_MAX_TAGS		16		/* per frame */

/* vertex scales */
#define MD3_XYZ_SCALE		(1.0/64)

typedef struct {
	int ident;
	int version;
	char	name[64];
	int flags;
	int numFrames;
	int numTags;
	int numSurfaces;
	int numSkins;
	int ofsFrames;
	int ofsTags;
	int ofsSurfaces;
	int ofsEnd;
} md3_t;

typedef struct {
	vec3_t	minBounds;
	vec3_t	maxBounds;
	vec3_t	localOrigin;
	float	radius;
	char	name[16];
} md3Frame_t;

typedef struct {
	char	name[64];
	vec3_t	origin;
	vec3_t	axis[3];
} md3Tag_t;

typedef struct {
	int ident;
	char	name[64];
	int flags;
	int numFrames;
	int numShaders;
	int numVerts;
	int numTriangles;
	int ofsTriangles;
	int ofsShaders;
	int ofsST;
	int ofsXYZNormal;
	int ofsEnd;
} md3Surface_t;

typedef struct {
	char	name[64];
	int shaderIndex;
} md3Shader_t;

typedef struct {
	int indices[3];
} md3Triangle_t;

typedef struct {
	float	st[2];
} md3TexCoord_t;

typedef struct {
	short	xyz[3];
	short	normal;
} md3Vertex_t;

md3_t *C_LoadMD3(const char *filename);
void C_MD3InterpolateTags(const md3Tag_t *t1, const md3Tag_t *t2,
		md3Tag_t *to, float fraction);
#endif /* _C_MD3_H_ */
