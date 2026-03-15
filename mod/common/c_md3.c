/**
 * Filename: c_md3.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "c_syscalls.h"
#include "c_md3.h"
#include "c_bmalloc.h"
#include "../game/q_shared.h"

/*
 * Endian conversion
 */

#define LE4(x) (((int)(((unsigned char *)&x)[0])) | \
			   ((int)(((unsigned char *)&x)[1]) << 8) | \
			   ((int)(((unsigned char *)&x)[2]) << 16) | \
			   ((int)(((unsigned char *)&x)[3]) << 24))

#define LE2(x) (((short)(((unsigned char *)&x)[0])) | \
			   ((short)(((unsigned char *)&x)[1]) << 8))

//static float LittleEndianFloat(int value)
//{
//	return ((float *)(&value))[0];
//}

//#define LEF(x) LittleEndianFloat(LE4(x))

#define LEF(x) do{\
 int y = LE4(x);\
 (x) = ((float *)(&y))[0];\
}while(0)


/**
 * C_LoadMD3
 *
 * Loads an MD3 file and does endian-swapping
 */
md3_t *C_LoadMD3(const char *filename)
{
	fileHandle_t   handle;
	int 		   size;
	md3_t		  *md3;
	md3Frame_t	  *frame;
	md3Surface_t  *surface;
	md3Triangle_t *tri;
	md3TexCoord_t *st;
	md3Vertex_t   *vertex;
	md3Tag_t	  *tag;
	int 		  i, j;

	size = trap_FS_FOpenFile(filename, &handle, FS_READ);

	md3 = bmalloc(size);
	if (md3 == 0) {
		Com_Error(ERR_FATAL, "Out of memory.");
		return 0;
	}

	trap_FS_Read(md3, size, handle);
	trap_FS_FCloseFile(handle);

	if (md3->ident != MD3_IDENT) {
		bfree(md3);
		Com_Error(ERR_FATAL, "MD3 identifier %d incorrect in %s.", md3->ident, filename);
		return 0;
	}

	if (md3->version != MD3_VERSION) {
		bfree(md3);
		Com_Error(ERR_FATAL, "MD3 version %d incorrect, should be %d in %s.",
				md3->version, MD3_VERSION, filename);
		return 0;
	}

	/*
	 * For compatibility with big-endian systems,
	 * all the bytes must be read and swapped if necessary
	 */
	md3->ident			= LE4(md3->ident);
	md3->version		= LE4(md3->version);
	md3->numFrames		= LE4(md3->numFrames);
	md3->numTags		= LE4(md3->numTags);
	md3->numSurfaces	= LE4(md3->numSurfaces);
	md3->ofsFrames		= LE4(md3->ofsFrames);
	md3->ofsTags		= LE4(md3->ofsTags);
	md3->ofsSurfaces	= LE4(md3->ofsSurfaces);
	md3->ofsEnd 		= LE4(md3->ofsEnd);

	if (md3->numFrames < 1) {
		Com_Error(ERR_FATAL, "No frames read in %s.", filename);
	}

	/*
	 * Read all the frames and swap them
	 */
	frame = (md3Frame_t *)((char *)md3 + md3->ofsFrames);
	for (i = 0; i < md3->numFrames; i++, frame++) {
		/*frame->radius = */LEF(frame->radius);
/*
		for (j = 0; j < 3; j++) {
			frame->minBounds[j] = LEF(frame->minBounds[j]);
			frame->maxBounds[j] = LEF(frame->maxBounds[j]);
			frame->localOrigin[j] = LEF(frame->localOrigin[j]);
		}
*/
		/*frame->minBounds[0] = */LEF(frame->minBounds[0]);
		/*frame->maxBounds[0] = */LEF(frame->maxBounds[0]);
		/*frame->localOrigin[0] = */LEF(frame->localOrigin[0]);
		/*frame->minBounds[1] = */LEF(frame->minBounds[1]);
		/*frame->maxBounds[1] = */LEF(frame->maxBounds[1]);
		/*frame->localOrigin[1] = */LEF(frame->localOrigin[1]);
		/*frame->minBounds[2] = */LEF(frame->minBounds[2]);
		/*frame->maxBounds[2] = */LEF(frame->maxBounds[2]);
		/*frame->localOrigin[2] = */LEF(frame->localOrigin[2]);

	}

	/*
	 * Read all the tags and swap them
	 */
	tag = (md3Tag_t *)((char *)md3 + md3->ofsTags);
	for (i = 0; i < md3->numTags * md3->numFrames; i++, tag++) {
/*
		for (j = 0; j < 3; j++) {
			tag->origin[j] = LEF(tag->origin[j]);
			for (k = 0; k < 3; k++) {
				tag->axis[k][j] = LEF(tag->axis[k][j]);
			}
		}
*/

	  /*tag->origin[0] = */LEF(tag->origin[0]);
	  /*tag->axis[0][0] = */LEF(tag->axis[0][0]);
	  /*tag->axis[1][0] = */LEF(tag->axis[1][0]);
	  /*tag->axis[2][0] = */LEF(tag->axis[2][0]);
	  /*tag->origin[1] = */LEF(tag->origin[1]);
	  /*tag->axis[0][1] = */LEF(tag->axis[0][1]);
	  /*tag->axis[1][1] = */LEF(tag->axis[1][1]);
	  /*tag->axis[2][1] = */LEF(tag->axis[2][1]);
	  /*tag->origin[2] = */LEF(tag->origin[2]);
	  /*tag->axis[0][2] = */LEF(tag->axis[0][2]);
	  /*tag->axis[1][2] = */LEF(tag->axis[1][2]);
	  /*tag->axis[2][2] = */LEF(tag->axis[2][2]);
	}

	/*
	 * Read all the surfaces and swap them
	 */
	surface = (md3Surface_t *)((char *)md3 + md3->ofsSurfaces);
	for (i = 0; i < md3->numSurfaces; i++) {
		surface->ident = LE4(surface->ident);
		surface->flags = LE4(surface->flags);
		surface->numFrames = LE4(surface->numFrames);
		surface->numShaders = LE4(surface->numShaders);
		surface->numTriangles = LE4(surface->numTriangles);
		surface->ofsTriangles = LE4(surface->ofsTriangles);
		surface->ofsST = LE4(surface->ofsST);
		surface->ofsXYZNormal = LE4(surface->ofsXYZNormal);
		surface->ofsEnd = LE4(surface->ofsEnd);

		/* Change all surface names to lower case */
		Q_strlwr(surface->name);

		/* Strip off trailing _1 or _2? */
		j = strlen(surface->name);
		if (j > 2 && surface->name[j-2] == '_') {
			surface->name[j-2] = 0;
		}

		/*
		 * Read all the triangles and swap them
		 */
		tri = (md3Triangle_t *)((char *)surface +
				surface->ofsTriangles);
		for (j = 0; j < surface->numTriangles; j++, tri++) {
//			  for (k = 0; k < 3; k++) tri->indices[k] = LE4(tri->indices[k]);
		 tri->indices[0] = LE4(tri->indices[0]);
		 tri->indices[1] = LE4(tri->indices[1]);
		 tri->indices[2] = LE4(tri->indices[2]);
		}

		/*
		 * Read and swap all the texture coordinates
		 */
		st = (md3TexCoord_t *)((char *)surface + surface->ofsST);
		for (j = 0; j < surface->numVerts; j++) {
			/*st->st[0] = */LEF(st->st[0]);
			/*st->st[1] = */LEF(st->st[1]);
		}

		/*
		 * Read and swap all the vertices
		 */
		vertex = (md3Vertex_t *)((char *)surface +
				surface->ofsXYZNormal);
		for (j = 0; j < surface->numVerts * surface->numFrames;
				j++, vertex++) {
			vertex->xyz[0] = LE2(vertex->xyz[0]);
			vertex->xyz[1] = LE2(vertex->xyz[1]);
			vertex->xyz[2] = LE2(vertex->xyz[2]);
			vertex->normal = LE2(vertex->normal);
		}

		/*
		 * Iterate to next surface
		 */
		surface = (md3Surface_t *)((char *)surface + surface->ofsEnd);
	}

	return md3;
}

/**
 * C_MD3InterpolateTags
 *
 * Interpolate frames using a fraction
 */
/* INLINED
void C_MD3InterpolateTags(const md3Tag_t *t1, const md3Tag_t *t2,
		md3Tag_t *to, float backlerp)
{
	int i, l;
	for (i = 0; i < 3; i++) {
		to->origin[i] = t2->origin[i] + (t1->origin[i] - t2->origin[i]) * backlerp;
		for (l = 0; l < 3; l++) {
			to->axis[i][l] = t2->axis[i][l] + (t1->axis[i][l] - t2->axis[i][l]) * backlerp;
		}
	}
}
*/