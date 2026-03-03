/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cmodel.c -- model loading

#include "cm_local.h"
#include "cm_load_bsp2.h"

/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadShaders
=================
*/
void CMod_LoadShaders2( lump_t *l ) {
	dBsp2Texinfo_t *in;
	dshader_t *out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "CMod_LoadShaders: funny lump size");
	}
	count = l->filelen / sizeof(*in);

	if (count < 1) {
		Com_Error (ERR_DROP, "Map with no shaders");
	}
	cms[cm].shaders = Hunk_Alloc( count * sizeof( *cms[cm].shaders ), h_high );
	cms[cm].numShaders = count;

	out = cms[cm].shaders;
	for ( i=0 ; i<count ; i++, in++, out++ ) {
		memcpy(&out->shader, va("textures/%s", in->texture[0] == '.' || in->texture[0] == '/' ? &in->texture[1] : in->texture), sizeof(out->shader));
		out->shader[sizeof(out->shader)-1] = '\0';
		out->contentFlags = LittleLong( in->flags );
		if(in->flags & Q2_SURF_SKY)
			out->surfaceFlags |= SURF_SKY;
		if(in->flags & Q2_SURF_NODRAW)
			out->surfaceFlags |= SURF_NODRAW;
		if (in->flags & MATERIAL_METAL)
			out->surfaceFlags |= SURF_METALSTEPS;
		if (in->flags & MATERIAL_SILENT)
			out->surfaceFlags |= SURF_NOSTEPS;
	}
}


/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels2( lump_t *l ) {
	dBsp2Model_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	int			*indexes;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadSubmodels: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	cms[cm].cmodels = Hunk_Alloc( count * sizeof( *cms[cm].cmodels ), h_high );
	cms[cm].numSubModels = count;

	if ( count > MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "MAX_SUBMODELS exceeded" );
	}

	for ( i=0 ; i<count ; i++, in++)
	{
		out = &cms[cm].cmodels[i];
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}

		if ( i == 0 ) {
			continue;	// world model doesn't need other info
		}

		int leafNum = in->headnode + 1;
		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numLeafBrushes = LittleLong( cms[cm].leafs[leafNum].numLeafBrushes );
		indexes = Hunk_Alloc( out->leaf.numLeafBrushes * 4, h_high );
		out->leaf.firstLeafBrush = indexes - cms[cm].leafbrushes;
		for ( j = 0 ; j < out->leaf.numLeafBrushes ; j++ ) {
			indexes[j] = LittleLong( in->headnode ) + j;
		}

		out->leaf.numLeafSurfaces = LittleLong( in->numfaces );
		indexes = Hunk_Alloc( out->leaf.numLeafSurfaces * 4, h_high );
		out->leaf.firstLeafSurface = indexes - cms[cm].leafsurfaces;
		for ( j = 0 ; j < out->leaf.numLeafSurfaces ; j++ ) {
			indexes[j] = LittleLong( in->firstface ) + j;
		}
	}
}


/*
=================
CMod_LoadBrushes

=================
*/
void CMod_LoadBrushes2( lump_t *l ) {
	dBsp2Brush_t	*in;
	cbrush_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	}
	count = l->filelen / sizeof(*in);

	cms[cm].brushes = Hunk_Alloc( ( BOX_BRUSHES + count ) * sizeof( *cms[cm].brushes ), h_high );
	cms[cm].numBrushes = count;

	out = cms[cm].brushes;

	for ( i = 0; i < count; i++, out++, in++ ) {
		out->sides = cms[cm].brushsides + LittleLong( in->firstside );
		out->numsides = LittleLong( in->numsides );
		
		out->shaderNum = (*out->sides).shaderNum;
		if ( out->shaderNum < 0 || out->shaderNum >= cms[cm].numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushes: bad shaderNum: %i", out->shaderNum );
		}
		out->contents = cms[cm].shaders[out->shaderNum].contentFlags;

		CM_BoundBrush( out );
	}

}


/*
=================
CMod_LoadLeafs
=================
*/
void CMod_LoadLeafs2( lump_t *l )
{
	int			i;
	cLeaf_t		*out;
	dBsp2Leaf_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );

	count = l->filelen / sizeof(*in);

	if ( count < 1 )
		Com_Error( ERR_DROP, "Map with no leafs" );

	cms[cm].leafs = Hunk_Alloc( ( BOX_LEAFS + count ) * sizeof( *cms[cm].leafs ), h_high );
	cms[cm].numLeafs = count;

	out = cms[cm].leafs;
	for ( i = 0; i < count; i++, in++, out++ )
	{
		out->cluster = LittleLong( in->cluster );
		//out->area = LittleLong( in->area );
		out->firstLeafBrush = LittleLong( in->firstleafbrush );
		out->numLeafBrushes = LittleLong( in->numleafbrushes );
		out->firstLeafSurface = LittleLong( in->firstleafface );
		out->numLeafSurfaces = LittleLong( in->numleaffaces );

/*

	// get numClusters (have in dBsp2Vis_t, but Q2 recomputes this ...)
	cms[cm].numClusters = 0;
	for (i = 0; i < cms[cm].numLeafs; i++)
		if (cms[cm].leafs2[i].cluster >= cms[cm].numClusters)
			cms[cm].numClusters = cms[cm].leafs2[i].cluster + 1;

*/
		if ( out->cluster >= cms[cm].numClusters )
			cms[cm].numClusters = out->cluster + 1;
		if ( out->area >= cms[cm].numAreas )
			cms[cm].numAreas = out->area + 1;
	}

	cms[cm].areas = Hunk_Alloc( cms[cm].numAreas * sizeof( *cms[cm].areas ), h_high );
	cms[cm].areaPortals = Hunk_Alloc( cms[cm].numAreas * cms[cm].numAreas * sizeof( *cms[cm].areaPortals ), h_high );
}


/*
=================
CMod_LoadNodes

=================
*/
void CMod_LoadNodes2( lump_t *l ) {
	dBsp2Node_t	*in;
	int		child;
	cNode_t	*out;
	int		i, j, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );

	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error( ERR_DROP, "Map has no nodes" );
	cms[cm].nodes = Hunk_Alloc( count * sizeof( *cms[cm].nodes ), h_high );
	cms[cm].numNodes = count;

	out = cms[cm].nodes;

	for ( i = 0; i < count; i++, out++, in++ )
	{
		out->plane = cms[cm].planes + LittleLong( in->planeNum );
		for ( j = 0; j < 2; j++ )
		{
			child = LittleLong( in->children[j] );
			out->children[j] = child;
		}
	}

}


/*
=================
CMod_LoadPlanes
=================
*/
void CMod_LoadPlanes2( const lump_t *l )
{
	int			i, j;
	cplane_t	*out;
	dBsp2Plane_t 	*in;
	int			count;
	int			bits;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );

	count = l->filelen / sizeof(*in);

	if ( count < 1 )
		Com_Error( ERR_DROP, "Map with no planes" );

	cms[cm].planes = Hunk_Alloc( ( BOX_PLANES + count ) * sizeof( *cms[cm].planes ), h_high );
	cms[cm].numPlanes = count;

	out = cms[cm].planes;

	for ( i = 0; i < count; i++, in++, out++ )
	{
		bits = 0;
		for ( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if ( out->normal[j] < 0 )
				bits |= 1<<j;
		}

		out->dist = LittleFloat( in->dist );
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
	/*
	
		// silly check for correct lightmaps
		for (i = 0; i < cms[cm].numFaces; i++)
			if (cms[cm].faces2[i].lightofs > cms[cm].lightDataSize)
				cms[cm].faces2[i].lightofs = -1;

*/
}


/*
=================
CMod_LoadLeafBrushes
=================
*/
void CMod_LoadLeafBrushes2( const lump_t *l )
{
	int i;
	int *out;
	short *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );

	count = l->filelen / sizeof(*in);

	cms[cm].leafbrushes = Hunk_Alloc( (count + BOX_BRUSHES) * sizeof( *cms[cm].leafbrushes ), h_high );
	cms[cm].numLeafBrushes = count;

	out = cms[cm].leafbrushes;

	for ( i = 0; i < count; i++, in++, out++ ) {
		*out = LittleLong( *in );
	}
}


/*
=================
CMod_LoadLeafSurfaces
=================
*/
void CMod_LoadLeafSurfaces2( const lump_t *l )
{
	int i;
	int *out;
	short *in;
	int count;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) )
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );

	count = l->filelen / sizeof(*in);

	cms[cm].leafsurfaces = Hunk_Alloc( count * sizeof( *cms[cm].leafsurfaces ), h_high );
	cms[cm].numLeafSurfaces = count;

	out = cms[cm].leafsurfaces;

	for ( i = 0; i < count; i++, in++, out++ ) {
		*out = LittleLong( *in );
	}
}


/*
=================
CMod_LoadBrushSides
=================
*/
void CMod_LoadBrushSides2 (lump_t *l)
{
	int				i;
	cbrushside_t	*out;
	dBsp2Brushside_t	*in;
	int				count;
	int				num;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) ) {
		Com_Error( ERR_DROP, "MOD_LoadBmodel: funny lump size" );
	}
	count = l->filelen / sizeof(*in);

	cms[cm].brushsides = Hunk_Alloc( ( BOX_SIDES + count ) * sizeof( *cms[cm].brushsides ), h_high );
	cms[cm].numBrushSides = count;

	out = cms[cm].brushsides;

	for ( i= 0; i < count; i++, in++, out++ ) {
		num = LittleLong( in->planenum );
		out->plane = &cms[cm].planes[num];
		out->shaderNum = LittleLong( in->texinfo );
		if ( out->shaderNum < 0 ) out->shaderNum = 0;
		if ( out->shaderNum < 0 || out->shaderNum >= cms[cm].numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushSides2: bad shaderNum: %i", out->shaderNum );
		}
		out->surfaceFlags = cms[cm].shaders[out->shaderNum].surfaceFlags;
	}
}


/*
=================
CMod_LoadEntityString
=================
*/
void CMod_LoadEntityString2( lump_t *l, const char *name ) {
	CMod_LoadEntityString(l, name);
	
	// detect kingpin map entities
	if (strstr(cms[cm].entityString, "\"classname\" \"junior\"") ||
		strstr(cms[cm].entityString, "\"classname\" \"lightflare\"") ||
		strstr(cms[cm].entityString, "\"fogdensity2\""))
	{
		//TODO: bspfile.type = map_kp;
		Com_DPrintf("Kingpin map detected\n");
	}

}


static void DecompressVis(byte *dst, void *vis, int pos, int rowSize)
{
	if (pos == -1)
	{
		memset(dst, 0xFF, rowSize);	// all visible
		dst += rowSize;
		return;
	}

	byte *src = (byte*)vis + pos;
	// decompress vis
	for (int j = rowSize; j; /*empty*/)
	{
		byte c = *src++;
		if (c)
		{	// non-zero byte
			*dst++ = c;
			j--;
		}
		else
		{	// zero byte -- decompress RLE data (with filler 0)
			c = *src++;				// count
			c = min(c, j);			// should not be, but ...
			j -= c;
			while (c--)
				*dst++ = 0;
		}
	}
}

/*
=================
CMod_LoadVisibility
=================
*/
#define	VIS_HEADER	8
void CMod_LoadVisibility2( lump_t *l ) {
	int		len;
	byte	*buf;
	int rowSize;
	dBsp2Vis_t *in;
	in = (void *)(cmod_base + l->fileofs);

	len = l->filelen;
	if ( !len ) {
		cms[cm].numClusters = 1;
		cms[cm].clusterBytes = ( cms[cm].numClusters + 31 ) & ~31;
		cms[cm].visibility = Hunk_Alloc( cms[cm].clusterBytes, h_high );
		Com_Memset( cms[cm].visibility, 255, cms[cm].clusterBytes );
		return;
	}
	buf = cmod_base + l->fileofs;

	rowSize = (in->numClusters + 7) >> 3;

	cms[cm].vised = qtrue;
	cms[cm].visibility = Hunk_Alloc( len, h_high );
	cms[cm].numClusters = LittleLong( ((int *)buf)[0] );
	cms[cm].clusterBytes = LittleLong( ((int *)buf)[1] );
	for (int i = 0; i < in->numClusters; i++, cms[cm].visibility += rowSize)
		DecompressVis(cms[cm].visibility, in, in->bitOfs[i][PVS], rowSize);
}

//==================================================================


/*
=================
CMod_LoadPatches
=================
*/
#define	MAX_PATCH_VERTS		1024
void CMod_LoadPatches2( lump_t *surfs, lump_t *verts ) {
	vec3_t	*dv, *dv_p;
	dBspFace_t	*in;
	int			count;
	int			i, j;
	int			c;
	cPatch_t	*patch;
	vec3_t		points[MAX_PATCH_VERTS];
	int			width, height;
	int			shaderNum;

	in = (void *)(cmod_base + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadPatches2: funny lump size");
	cms[cm].numSurfaces = count = surfs->filelen / sizeof(*in);
	cms[cm].surfaces = Hunk_Alloc( cms[cm].numSurfaces * sizeof( cms[cm].surfaces[0] ), h_high );

	dv = (void *)(cmod_base + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		Com_Error (ERR_DROP, "CMod_LoadPatches2: funny lump size");

	// scan through all the surfaces, but only load patches,
	// not planar faces
	for ( i = 0 ; i < count ; i++, in++ ) {
		// FIXME: check for non-colliding patches

		cms[cm].surfaces[ i ] = patch = Hunk_Alloc( sizeof( *patch ), h_high );
		Com_Printf( "Loading map ------------------\n" );

		// load the full drawverts onto the stack
		width = 5; //LittleLong( in->patchWidth );
		height = 5; //LittleLong( in->patchHeight );
		c = LittleLong( in->numedges );
		if(c == 6) {
			width = 3;
			height = 3;
		}
		if ( c > MAX_PATCH_VERTS ) {
			Com_Error( ERR_DROP, "ParseMesh: MAX_PATCH_VERTS" );
		}
		if(!c) continue;
		//Com_Printf("CMod_LoadPatches2: %i, %f x %f x %f\n", c, 
		//	points[j][0], points[j][1], points[j][2]);

		dv_p = dv + LittleLong( in->firstedge );
		for ( j = 0 ; j < c ; j++, dv_p++ ) {
			points[j][0] = LittleFloat( *dv_p[0] );
			points[j][1] = LittleFloat( *dv_p[1] );
			points[j][2] = LittleFloat( *dv_p[2] );
		}

		shaderNum = LittleLong( in->texinfo );
		patch->contents = cms[cm].shaders[shaderNum].contentFlags;
		patch->surfaceFlags = cms[cm].shaders[shaderNum].surfaceFlags;
		
		//if(patch->surfaceFlags & SURF_SKY) {
		//	continue;
		//}

		// create the internal facet structure
		//patch->pc = CM_GeneratePatchCollide( width, height, points );
	}
}


static int CheckLump(lump_t *l, char *lumpName, int size)
{
	if (l->filelen % size)
		Com_Error(ERR_DROP, "LoadBSPFile: incorrect lump size (%s)", lumpName);

	//*ptr = (void *)(cmod_base + l->fileofs);

	return l->filelen / size;
}


void LoadQ2Map(const char *name) {
	dBsp2Hdr_t		header;

	header = *(dBsp2Hdr_t *)cmod_base;

#if !LITTLE_ENDIAN
	// swap the header
	for (int i = 0; i < sizeof(dBsp2Hdr_t) / 4; i++)
		((int *)cmod_base)[i] = LittleLong(((int *)cmod_base)[i]);
#endif

#define C(num,field,count,type) \
	CheckLump(&header.lumps[LUMP_Q2_##num], XSTRING(num), sizeof(type))
	C(LIGHTING, lighting, lightDataSize, byte);
	C(VERTEXES, vertexes2, numVertexes, vec3_t);
	C(PLANES, planes2, numPlanes, dBsp2Plane_t);
	C(LEAFS, leafs2, numLeafs, dBsp2Leaf_t);
	C(NODES, nodes2, numNodes, dBsp2Node_t);
	C(TEXINFO, texinfo2, numTexinfo, dBsp2Texinfo_t);
	C(FACES, faces2, numFaces, dBspFace_t);
	C(LEAFFACES, leaffaces2, numLeaffaces, unsigned short);
	C(LEAFBRUSHES, leafbrushes2, numLeafbrushes, unsigned short);
	C(SURFEDGES, surfedges, numSurfedges, int);
	C(EDGES, edges, numEdges, short[2]);
	C(BRUSHES, brushes2, numBrushes, dBsp2Brush_t);
	C(BRUSHSIDES, brushsides2, numBrushSides, dBsp2Brushside_t);
	C(ZONES, zones, numZones, dZone_t);
	C(ZONEPORTALS, zonePortals, numZonePortals, dZonePortal_t);
	C(MODELS, models2, numModels, dBsp2Model_t);
#undef C
	// load into heap
	CMod_LoadShaders2( &header.lumps[LUMP_Q2_TEXINFO] );
	CMod_LoadLeafs2 (&header.lumps[LUMP_Q2_LEAFS]);
	CMod_LoadLeafBrushes2 (&header.lumps[LUMP_Q2_LEAFBRUSHES]);
	CMod_LoadLeafSurfaces2 (&header.lumps[LUMP_Q2_LEAFFACES]);
	CMod_LoadPlanes2 (&header.lumps[LUMP_Q2_PLANES]);
	CMod_LoadBrushSides2 (&header.lumps[LUMP_Q2_BRUSHSIDES]);
	CMod_LoadBrushes2 (&header.lumps[LUMP_Q2_BRUSHES]);
	CMod_LoadSubmodels2 (&header.lumps[LUMP_Q2_MODELS]);
	CMod_LoadNodes2 (&header.lumps[LUMP_Q2_NODES]);
	CMod_LoadEntityString2 (&header.lumps[LUMP_Q2_ENTITIES], name);
	CMod_LoadVisibility2( &header.lumps[LUMP_Q2_VISIBILITY] );
	// TODO: area portals and area mask stuff
	//CMod_LoadAreas( &header.lumps[LUMP_Q2_ZONES] );
	//CMod_LoadAreaPortals( &header.lumps[LUMP_Q2_ZONEPORTALS] );
	//
	//;
	CMod_LoadPatches2( &header.lumps[LUMP_Q2_FACES], &header.lumps[LUMP_Q2_VERTEXES] );

}
