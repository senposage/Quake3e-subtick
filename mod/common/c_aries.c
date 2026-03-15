/**
 * Filename: c_aries.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * This is the implementation for Aries 3
 */

#include "c_aries.h"
#include "c_bmalloc.h"
#include "c_arieshl.h"

//static ariesTraceHit_t c_traceHits[ARIES_MAX_HITS];

ariesPlayer_t c_aries[2];

/**
 * C_AriesInit
 *
 * Initialise Aries
 */
void C_AriesInit(void)
{
	c_aries[ARIES_ATHENA].upper =
		C_AriesLoadModel("models/players/athena/upper_3.md3");
	c_aries[ARIES_ATHENA].lower =
		C_AriesLoadModel("models/players/athena/lower_3.md3");
	c_aries[ARIES_ATHENA].head =
		C_AriesLoadModel("models/players/athena/head_3.md3");
	c_aries[ARIES_ATHENA].helmet =
		C_AriesLoadModel("models/players/athena/helmet_3.md3");
	C_AriesAttachModels(c_aries[ARIES_ATHENA].lower,
			c_aries[ARIES_ATHENA].upper, "tag_torso");
	C_AriesAttachModels(c_aries[ARIES_ATHENA].upper,
			c_aries[ARIES_ATHENA].head, "tag_head");
	C_AriesAttachModels(c_aries[ARIES_ATHENA].head,
			c_aries[ARIES_ATHENA].helmet, "tag_head");
	c_aries[ARIES_ORION].upper =
		C_AriesLoadModel("models/players/orion/upper_3.md3");
	c_aries[ARIES_ORION].lower =
		C_AriesLoadModel("models/players/orion/lower_3.md3");
	c_aries[ARIES_ORION].head =
		C_AriesLoadModel("models/players/orion/head_3.md3");
	c_aries[ARIES_ORION].helmet =
		C_AriesLoadModel("models/players/orion/helmet_3.md3");
	C_AriesAttachModels(c_aries[ARIES_ORION].lower,
			c_aries[ARIES_ORION].upper, "tag_torso");
	C_AriesAttachModels(c_aries[ARIES_ORION].upper,
			c_aries[ARIES_ORION].head, "tag_head");
	C_AriesAttachModels(c_aries[ARIES_ORION].head,
			c_aries[ARIES_ORION].helmet, "tag_head");
}

/**
 * C_AriesLoadModel
 *
 * Load a model for use in Aries Hitmesh detection
 */
ariesModel_t * C_AriesLoadModel(const char *filename)
{
	/* Allocate some memory for the Aries model */
	ariesModel_t *model = bmalloc(sizeof(ariesModel_t));
	memset(model, 0, sizeof(ariesModel_t));

	/* Load the md3 file */
	model->md3 = C_LoadMD3(filename);

	return model;
}

/**
 * C_AriesAttachModels
 *
 * Attach a child model to a parent model via a tag
 */
qboolean C_AriesAttachModels(ariesModel_t *parent, ariesModel_t *child,
		const char *tagName)
{
	md3Tag_t *tag;
	int i;

	/* Validate pointers */
	if (!parent || !child)
		return qfalse;

	/*
	 * Check if memory has already allocated for children
	 * if not allocate it.
	 */
	if (!parent->children)
	{
		int size = sizeof(ariesModel_t *) * parent->md3->numTags;
		parent->children = bmalloc(size);
		memset(parent->children, 0, size);
	}

	/* Point to the md3 tags in memory */
	tag = (md3Tag_t *)((char *)parent->md3 + parent->md3->ofsTags);
	/* Add the child model associating it with the tag */
	for (i = 0; i < parent->md3->numTags; i++, tag++)
	{
		if (!strcmp(tagName, tag->name))
		{
			parent->children[i] = child;
			child->parent = parent;
			return qtrue;
		}
	}

	return qfalse;
}

/**
 * C_AriesMatrixTransform
 *
 * Creates a matrix compounding of the transformations for a given vertex
 */

#if 0
//INLINED
static float identity44[4][4] = {{1.0f,0,0,0},{0,1.0f,0,0},{0,0,1.0f,0},{0,0,0,1.0f}};

void C_AriesMatrixTransform(ariesModel_t *model, float out[4][4])
{
	ariesModel_t* mdl;
	float temp[4][4];
//	  int i;

	/* Set the output matrix to the identity matrix */
//	  memset(out, 0, sizeof(vec4_t) * 4);
//	  out[0][0] = 1.0f;
//	  out[1][1] = 1.0f;
//	  out[2][2] = 1.0f;
//	  out[3][3] = 1.0f;
	memcpy(out,identity44,sizeof(temp));
	memcpy(temp, out, sizeof(temp));

	/* Apply the matrix rotation in sequence */
	for (mdl = model; mdl; mdl = mdl->parent) {
		MatrixMultiply4(temp, mdl->state.rotation, out);
		memcpy(temp, out, sizeof(temp));
	}

	/* Translate the matrix */
//	  for (i = 0; i < 3; i++) out[3][i] = model->state.origin[i];

	out[3][0] = model->state.origin[0];
	out[3][1] = model->state.origin[1];
	out[3][2] = model->state.origin[2];

//	  memcpy(&out[3][0],&model->state.origin[0],3*sizeof(float));
}
#endif


/**
 * C_AriesApplyTransform
 *
 * Apply the transformation on the vertex
 */
void C_AriesApplyTransform(float matrix[4][4], vec3_t vertex, vec3_t out)
{
	int i;
	for (i = 0; i < 3; i++) {
		out[i] = vertex[0] * matrix[0][i] +
			 vertex[1] * matrix[1][i] +
			 vertex[2] * matrix[2][i] +
			 matrix[3][i];
	}
}

/**
 * C_AriesApplyModelTagframe
 *
 * Apply the tag frame to the aries model's matrix
 */
void C_AriesApplyModelTagframe(ariesModel_t *model, md3Tag_t *t)
{
	vec4_t mat[4];
	vec4_t mat2[4];
	ariesModel_t *parent = model->parent;
	ariesModel_t *mdl;

	mat[0][0] = t->axis[0][0];
	mat[0][1] = t->axis[0][1];
	mat[0][2] = t->axis[0][2];
	mat[0][3] = 0.0f;

	mat[1][0] = t->axis[1][0];
	mat[1][1] = t->axis[1][1];
	mat[1][2] = t->axis[1][2];
	mat[1][3] = 0.0f;

	mat[2][0] = t->axis[2][0];
	mat[2][1] = t->axis[2][1];
	mat[2][2] = t->axis[2][2];
	mat[2][3] = 0.0f;

	mat[3][0] = 0.0f;
	mat[3][1] = 0.0f;
	mat[3][2] = 0.0f;
	mat[3][3] = 1.0f;

	VectorCopy(t->origin, model->state.origin);

	if (!parent)
		return;

	/* Calculate the translation */
	for (mdl = parent; mdl; mdl = mdl->parent) {
		vec3_t temp2;
/*
		int i;
		for (i = 0; i < 3; i++) {
			temp2[i] = model->state.origin[0] * mdl->state.rotation[0][i] +
				   model->state.origin[1] * mdl->state.rotation[1][i] +
				   model->state.origin[2] * mdl->state.rotation[2][i];
		}
*/
		temp2[0] = model->state.origin[0] * mdl->state.rotation[0][0] +
			   model->state.origin[1] * mdl->state.rotation[1][0] +
			   model->state.origin[2] * mdl->state.rotation[2][0];
		temp2[1] = model->state.origin[0] * mdl->state.rotation[0][1] +
			   model->state.origin[1] * mdl->state.rotation[1][1] +
			   model->state.origin[2] * mdl->state.rotation[2][1];
		temp2[2] = model->state.origin[0] * mdl->state.rotation[0][2] +
			   model->state.origin[1] * mdl->state.rotation[1][2] +
			   model->state.origin[2] * mdl->state.rotation[2][2];
		VectorCopy(temp2, model->state.origin);
	}
	VectorAdd(model->state.origin, parent->state.origin,
		model->state.origin);
	MatrixMultiply4(mat, model->state.rotation, mat2);

	model->state.rotation[0][0]=mat2[0][0];model->state.rotation[0][1]=mat2[0][1];model->state.rotation[0][2]=mat2[0][2];model->state.rotation[0][3]=mat2[0][3];
	model->state.rotation[1][0]=mat2[1][0];model->state.rotation[1][1]=mat2[1][1];model->state.rotation[1][2]=mat2[1][2];model->state.rotation[1][3]=mat2[1][3];
	model->state.rotation[2][0]=mat2[2][0];model->state.rotation[2][1]=mat2[2][1];model->state.rotation[2][2]=mat2[2][2];model->state.rotation[2][3]=mat2[2][3];
	model->state.rotation[3][0]=mat2[3][0];model->state.rotation[3][1]=mat2[3][1];model->state.rotation[3][2]=mat2[3][2];model->state.rotation[3][3]=mat2[3][3];

//	  memcpy(model->state.rotation, mat2, sizeof(mat2));
}

/**
 * C_AriesMeshTrace
 *
 * Traces a ray through the md3 mesh and sees what it hits.
 */
static void C_AriesMeshTrace(ariesModel_t *model,
		vec3_t origin, vec3_t dir,
		ariesTraceResult_t *result)
{
	md3Triangle_t *tri;
	int i, j;
	float matrix[4][4];
	ariesHitLocation_t hit = HL_UNKNOWN;
	md3_t *md3 = model->md3;
	md3Surface_t *surface = (md3Surface_t *)
		((char *)md3 + md3->ofsSurfaces);
	ariesHitLocationMap_t *map;
	float dn = 10000000.0f;
	float df = 0.0f;

	/* Create a matrix transform */
//	C_AriesMatrixTransform(model, matrix);
	// INLINED:
	  ariesModel_t* mdl;
	  float temp[4][4];

	  temp[0][0]=1.0f;temp[0][1]=0;temp[0][2]=0;temp[0][3]=0;
	  temp[1][0]=0;temp[1][1]=1.0f;temp[1][2]=0;temp[1][3]=0;
	  temp[2][0]=0;temp[2][1]=0;temp[2][2]=1.0f;temp[2][3]=0;
	  temp[3][0]=0;temp[3][1]=0;temp[3][2]=0;temp[3][3]=1.0f;

	  matrix[0][0]=1.0f;matrix[0][1]=0;matrix[0][2]=0;matrix[0][3]=0;
	  matrix[1][0]=0;matrix[1][1]=1.0f;matrix[1][2]=0;matrix[1][3]=0;
	  matrix[2][0]=0;matrix[2][1]=0;matrix[2][2]=1.0f;matrix[2][3]=0;
	  matrix[3][0]=0;matrix[3][1]=0;matrix[3][2]=0;matrix[3][3]=1.0f;

//	    memcpy(matrix,identity44,sizeof(temp));
//	    memcpy(temp, matrix, sizeof(temp));

	  /* Apply the matrix rotation in sequence */
	  for (mdl = model; mdl; mdl = mdl->parent) {
		MatrixMultiply4(temp, mdl->state.rotation, matrix);

		temp[0][0]=matrix[0][0];temp[0][1]=matrix[0][1];temp[0][2]=matrix[0][2];temp[0][3]=matrix[0][3];
		temp[1][0]=matrix[1][0];temp[1][1]=matrix[1][1];temp[1][2]=matrix[1][2];temp[1][3]=matrix[1][3];
		temp[2][0]=matrix[2][0];temp[2][1]=matrix[2][1];temp[2][2]=matrix[2][2];temp[2][3]=matrix[2][3];
		temp[3][0]=matrix[3][0];temp[3][1]=matrix[3][1];temp[3][2]=matrix[3][2];temp[3][3]=matrix[3][3];

//		  memcpy(temp, matrix, sizeof(temp));
	  }
	  matrix[3][0] = model->state.origin[0];
	  matrix[3][1] = model->state.origin[1];
	  matrix[3][2] = model->state.origin[2];
	// END OF INLINE



#ifdef DEBUG_ARIES
	Com_Printf("O: {%f %f %f} R: {%f %f %f} {%f %f %f} {%f %f %f}\n",
		matrix[3][0], matrix[3][1], matrix[3][2],
		matrix[0][0], matrix[0][1], matrix[0][2],
		matrix[1][0], matrix[1][1], matrix[1][2],
		matrix[2][0], matrix[2][1], matrix[2][2]);
#endif

	for (i = 0; i < md3->numSurfaces; i++) {

		tri = (md3Triangle_t *)((char *)surface +
				surface->ofsTriangles);

		for (j = 0; j < surface->numTriangles; j++, tri++) {
			vec3_t triTest[3];
			float ft;
			float fu;
			float fv;
			vec3_t v;
			md3Vertex_t *ind_base = ((md3Vertex_t *)((char *)surface + surface->ofsXYZNormal));
			md3Vertex_t *ind1 = ind_base + surface->numVerts * model->state.oldFrame;
			md3Vertex_t *ind2 = ind_base + surface->numVerts * model->state.frame;

			{
			 md3Vertex_t *v1 = ind1 + tri->indices[0];
			 md3Vertex_t *v2 = ind2 + tri->indices[0];
			 v[0] = ((float)v2->xyz[0] + ((float)(v1->xyz[0] - v2->xyz[0])) * model->state.backlerp) * (1.0f/64.0f);
			 v[1] = ((float)v2->xyz[1] + ((float)(v1->xyz[1] - v2->xyz[1])) * model->state.backlerp) * (1.0f/64.0f);
			 v[2] = ((float)v2->xyz[2] + ((float)(v1->xyz[2] - v2->xyz[2])) * model->state.backlerp) * (1.0f/64.0f);
			 //C_AriesApplyTransform(matrix, v, triTest[0]);
			   triTest[0][0] = v[0] * matrix[0][0] + v[1] * matrix[1][0] + v[2] * matrix[2][0] + matrix[3][0];
			   triTest[0][1] = v[0] * matrix[0][1] + v[1] * matrix[1][1] + v[2] * matrix[2][1] + matrix[3][1];
			   triTest[0][2] = v[0] * matrix[0][2] + v[1] * matrix[1][2] + v[2] * matrix[2][2] + matrix[3][2];
			}
			{
			 md3Vertex_t *v1 = ind1 + tri->indices[1];
			 md3Vertex_t *v2 = ind2 + tri->indices[1];
			 v[0] = ((float)v2->xyz[0] + ((float)(v1->xyz[0] - v2->xyz[0])) * model->state.backlerp) * (1.0f/64.0f);
			 v[1] = ((float)v2->xyz[1] + ((float)(v1->xyz[1] - v2->xyz[1])) * model->state.backlerp) * (1.0f/64.0f);
			 v[2] = ((float)v2->xyz[2] + ((float)(v1->xyz[2] - v2->xyz[2])) * model->state.backlerp) * (1.0f/64.0f);
//			 C_AriesApplyTransform(matrix, v, triTest[1]);
			   triTest[1][0] = v[0] * matrix[0][0] + v[1] * matrix[1][0] + v[2] * matrix[2][0] + matrix[3][0];
			   triTest[1][1] = v[0] * matrix[0][1] + v[1] * matrix[1][1] + v[2] * matrix[2][1] + matrix[3][1];
			   triTest[1][2] = v[0] * matrix[0][2] + v[1] * matrix[1][2] + v[2] * matrix[2][2] + matrix[3][2];
			}
			{
			 md3Vertex_t *v1 = ind1 + tri->indices[2];
			 md3Vertex_t *v2 = ind2 + tri->indices[2];
			 v[0] = ((float)v2->xyz[0] + ((float)(v1->xyz[0] - v2->xyz[0])) * model->state.backlerp) * (1.0f/64.0f);
			 v[1] = ((float)v2->xyz[1] + ((float)(v1->xyz[1] - v2->xyz[1])) * model->state.backlerp) * (1.0f/64.0f);
			 v[2] = ((float)v2->xyz[2] + ((float)(v1->xyz[2] - v2->xyz[2])) * model->state.backlerp) * (1.0f/64.0f);
//			 C_AriesApplyTransform(matrix, v, triTest[2]);
			   triTest[2][0] = v[0] * matrix[0][0] + v[1] * matrix[1][0] + v[2] * matrix[2][0] + matrix[3][0];
			   triTest[2][1] = v[0] * matrix[0][1] + v[1] * matrix[1][1] + v[2] * matrix[2][1] + matrix[3][1];
			   triTest[2][2] = v[0] * matrix[0][2] + v[1] * matrix[1][2] + v[2] * matrix[2][2] + matrix[3][2];
			}
/*
			for (t = 0; t < 3; t++) {
				md3Vertex_t *v1 = ((md3Vertex_t *)((char *)
						surface +
						surface->ofsXYZNormal)) +
						surface->numVerts *
						model->state.oldFrame +
						tri->indices[t];
				md3Vertex_t *v2 = ((md3Vertex_t *)((char *)
						surface +
						surface->ofsXYZNormal)) +
						surface->numVerts *
						model->state.frame +
						tri->indices[t];
				for (k = 0; k < 3; k++) {
					v[k] = ((float)v2->xyz[k] +
					       ((float)(v1->xyz[k] -
						       v2->xyz[k])) *
					       model->state.backlerp) *
						(1.0f/64.0f);
				}
				C_AriesApplyTransform(matrix, v, triTest[t]);
			}
*/


//			  if (utTestIntersection(triTest, origin, dir, &fu, &fv, &ft))
//			  {

			   { // NastyInline(tm)
			    vec3_t  _edge[2];
			    vec3_t  _pvec;
			    vec3_t  _tvec;
			    vec3_t  _qvec;
			    float   _det;
			    float   _inv_det;
			    VectorSubtract ( triTest[1], triTest[0], _edge[0]);
			    VectorSubtract ( triTest[2], triTest[0], _edge[1]);
			    CrossProduct ( dir, _edge[1], _pvec );
			    _det = DotProduct ( _edge[0], _pvec );
			    if ((_det <= -0.000001) || (_det >= 0.000001))
			    {
			     _inv_det = 1.0 / _det;
			     VectorSubtract ( origin, triTest[0], _tvec );
			     fu = DotProduct(_tvec, _pvec) * _inv_det;
			     if ((fu >= 0.0) && (fu <= 1.0))
			     {
			      CrossProduct ( _tvec, _edge[0], _qvec );
			      fv = DotProduct ( dir, _qvec ) * _inv_det;
			      if ((fv >= 0.0) && (fu + fv <= 1.0))
			      {
			       ft = DotProduct (_edge[1], _qvec ) * _inv_det;
			       { // end of NastyInline(tm)

				vec3_t a, b, n, r;
				/*
				 * Calculate normal of triangle.
				 */
				VectorSubtract(triTest[0], triTest[1], a);
				VectorSubtract(triTest[1], triTest[2], b);
				CrossProduct(a, b, n);
				VectorNormalizeNR(n);

				/* Normalize the two vectors */
				VectorNormalizeNR(a);
				VectorNormalizeNR(b);
				/* Copy the first corner into the result */
				VectorCopy(triTest[0], r);

				/* Set result to point of intersection */
//				  for (t = 0; t < 3; t++) r[t] += a[t] * fu + b[t] * fv;
				r[0] += a[0] * fu + b[0] * fv;
				r[1] += a[1] * fu + b[1] * fv;
				r[2] += a[2] * fu + b[2] * fv;

				map = in_word_set(surface->name,
					strlen(surface->name));

				if (ft < dn) {
					VectorCopy(r, result->vecEntry);
					VectorCopy(n, result->vecEntryNormal);
					result->entryTriangle = i;
					dn = ft;
				}
				if (ft > df) {
					VectorCopy(r, result->vecExit);
					VectorCopy(n, result->vecExitNormal);
					result->exitTriangle = i;
					df = ft;
				}
				hit = map->location;

				/* Add hit to results */
				if (hit != HL_UNKNOWN) {
					ariesTraceHit_t *th = &result->hits[result->hitCount++];
					th->location = hit;
					VectorCopy(r, th->vecEntry);
					th->distance = ft;
				}
			}}}}}
		}

		/*
		 * Iterate to next surface
		 */
		surface = (md3Surface_t *)((char *)surface + surface->ofsEnd);
	}

	result->distEntry = dn;
	result->distExit = df;
}

/**
 * C_AriesTraceInternal
 *
 * Traces a ray through the md3 and see what it hits.
 */



void C_AriesTraceInternal(ariesModel_t *model, vec3_t origin, vec3_t dir,
		ariesTraceResult_t *result)
{
	md3Tag_t *tf1;
	md3Tag_t *tf2;
	md3Tag_t tf;
	int i;

	/* Check all attached md3's first */
	if (model->children)
	for (i = 0; i < model->md3->numTags; i++)
	{
		float backlerp = model->state.backlerp;
//		  if (!model->children)
//			  break;
		if (!model->children[i])
			continue;
		tf1 = &((md3Tag_t *)((char *)model->md3 + model->md3->ofsTags))
			[model->md3->numTags * model->state.oldFrame + i];
		tf2 = &((md3Tag_t *)((char *)model->md3 + model->md3->ofsTags))
			[model->md3->numTags * model->state.frame + i];


//		  C_MD3InterpolateTags(tf1, tf2, &tf, model->state.backlerp);
		tf.origin[0]  = tf2->origin[0]	+ (tf1->origin[0]  - tf2->origin[0])  * backlerp;
		tf.axis[0][0] = tf2->axis[0][0] + (tf1->axis[0][0] - tf2->axis[0][0]) * backlerp;
		tf.axis[0][1] = tf2->axis[0][1] + (tf1->axis[0][1] - tf2->axis[0][1]) * backlerp;
		tf.axis[0][2] = tf2->axis[0][2] + (tf1->axis[0][2] - tf2->axis[0][2]) * backlerp;
		tf.origin[1]  = tf2->origin[1]	+ (tf1->origin[1]  - tf2->origin[1])  * backlerp;
		tf.axis[1][0] = tf2->axis[1][0] + (tf1->axis[1][0] - tf2->axis[1][0]) * backlerp;
		tf.axis[1][1] = tf2->axis[1][1] + (tf1->axis[1][1] - tf2->axis[1][1]) * backlerp;
		tf.axis[1][2] = tf2->axis[1][2] + (tf1->axis[1][2] - tf2->axis[1][2]) * backlerp;
		tf.origin[2]  = tf2->origin[2]	+ (tf1->origin[2]  - tf2->origin[2])  * backlerp;
		tf.axis[2][0] = tf2->axis[2][0] + (tf1->axis[2][0] - tf2->axis[2][0]) * backlerp;
		tf.axis[2][1] = tf2->axis[2][1] + (tf1->axis[2][1] - tf2->axis[2][1]) * backlerp;
		tf.axis[2][2] = tf2->axis[2][2] + (tf1->axis[2][2] - tf2->axis[2][2]) * backlerp;



#ifdef DEBUG_ARIES
		Com_Printf("C ModelTagFrame(%d,%d): O: {%f %f %f} R: {%f %f %f} {%f %f %f} {%f %f %f}\n",
			model->state.oldFrame, model->state.frame,
			tf.origin[0], tf.origin[1], tf.origin[2],
			tf.axis[0][0], tf.axis[0][1], tf.axis[0][2],
			tf.axis[1][0], tf.axis[1][1], tf.axis[1][2],
			tf.axis[2][0], tf.axis[2][1], tf.axis[2][2]);
#endif
		C_AriesApplyModelTagframe(model->children[i], &tf);
		if (model->children[i]->children) {
		 C_AriesTraceInternal(model->children[i], origin, dir, result);
		} else {
		 C_AriesMeshTrace(model->children[i], origin, dir, result);
		}
	}

	/* Now check all the meshes for this model */
	C_AriesMeshTrace(model, origin, dir, result);
}

void C_AriesSortResult(ariesTraceResult_t *result)
{
	int k;
	/* Sort according to distance */
	for (k = 1; k < result->hitCount; k++) {
		ariesTraceHit_t d = result->hits[k];
		int i = k;
		for (; i > 0 && d.distance < result->hits[i - 1].distance; i--)
			result->hits[i] = result->hits[i - 1];
		result->hits[i] = d;
	}
	/* If arms or legs are closest and torso is in list, prefer torso */
	if (result->hitCount > 1 &&
           (result->hits[0].location == HL_ARML ||
            result->hits[0].location == HL_ARMR)) {
		for (k = 1; k < result->hitCount; k++)
			if (result->hits[k].location == HL_TORSO)
				break;
		/* Swap the hit around */
		if (k < result->hitCount) {
			ariesTraceHit_t hit = result->hits[0];
			result->hits[0] = result->hits[k];
			result->hits[k] = hit;
		}
	}
}

/**
 * C_AriesTrace
 *
 * Aries trace
 */
/* INLINED
void C_AriesTrace(ariesModel_t *model, vec3_t origin,
		vec3_t dir, ariesTraceResult_t *result)
{
	result->hitCount = 0;

	C_AriesTraceInternal(model, origin, dir, result);
	C_AriesSortResult(result);
}
*/
