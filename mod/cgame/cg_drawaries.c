/**
 * Filename: cg_aries.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */
#include "cg_local.h"
#include "cg_drawaries.h"

/**
 * CG_AriesMeshDraw
 *
 * Aries mesh draw
 */
void CG_AriesMeshDraw(ariesModel_t *model, qboolean server)
{
	md3Triangle_t *tri;
	int i, j; // k, t;
	float matrix[4][4];
	md3_t *md3 = model->md3;
	md3Surface_t *surface = (md3Surface_t *)
		((char *)md3 + md3->ofsSurfaces);

	/* Create a matrix transform */
//	  C_AriesMatrixTransform(model, matrix);

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


#ifdef DEBUG_ARIES
	Com_Printf("CG O: {%f %f %f} R: {%f %f %f} {%f %f %f} {%f %f %f}\n",
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
			//float ft;
			//float fu;
			//float fv;
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
#if 0
		for (j = 0; j < surface->numTriangles; j++, tri++) {
			vec3_t triTest[3];
			for (t = 0; t < 3; t++) {
				vec3_t v;
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
#endif
			{
				polyVert_t verts[3];
				int k;

				VectorCopy(triTest[0], verts[0].xyz);
				VectorCopy(triTest[1], verts[1].xyz);
				VectorCopy(triTest[2], verts[2].xyz);

				if (server) {
					for (k = 0; k < 3; k++) {
						verts[k].modulate[0] = 0;
						verts[k].modulate[1] = 128 +
							sin(cg.time * 0.025)
							* 128;
						verts[k].modulate[2] = 0;
						verts[k].modulate[3] = 128;
					}
				} else {
					switch (j % 3) {
						case 0:
						for (k = 0; k < 3; k++) {
							verts[k].modulate[0] = 0;
							verts[k].modulate[1] = 128 +
								sin(cg.time * 0.025)
								* 128;
							verts[k].modulate[2] = 255;
							verts[k].modulate[3] = 128;
						}
						break;
						case 1:
						for (k = 0; k < 3; k++) {
							verts[k].modulate[0] = (j * 20)
								% 255;
							verts[k].modulate[1] = (j * 5)
								% 255;
							verts[k].modulate[2] = (j * 32)
								% 255;
							verts[k].modulate[3] = 128;
						}
						break;
						case 2:
						for (k = 0; k < 3; k++) {
							verts[k].modulate[0] = 128;
							verts[k].modulate[1] = 128 +
								sin(cg.time * 0.025)
								* 128;
							verts[k].modulate[2] = 255;
							verts[k].modulate[3] = 128;
						}
						break;
					}
				}
				verts[0].st[0] = 0;
				verts[0].st[1] = 1;
				verts[1].st[0] = 1;
				verts[1].st[1] = 1;
				verts[2].st[0] = 0;
				verts[2].st[1] = 0;

				trap_R_AddPolyToScene(cgs.media.whiteShader,
					3, verts);

			}
		}

		/*
		 * Iterate to next surface
		 */
		surface = (md3Surface_t *)((char *)surface + surface->ofsEnd);
	}
}

/**
 * CG_AriesDrawModel
 *
 * Draw aries model
 */
void CG_AriesDrawModel(ariesModel_t *model, qboolean server)
{
	md3Tag_t *tf1;
	md3Tag_t *tf2;
	md3Tag_t tf;
	int i;

	/* Check all attached md3's first */
	if (model->children)
	for (i = 0; i < model->md3->numTags; i++) {
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
		Com_Printf("%s ModelTagFrame(%d,%d,%d): O: {%f %f %f} R: {%f %f %f} {%f %f %f} {%f %f %f}\n",
			server ? "server" : "client",
			model->state.oldFrame, model->state.frame, model->state.backlerp,
			tf.origin[0], tf.origin[1], tf.origin[2],
			tf.axis[0][0], tf.axis[0][1], tf.axis[0][2],
			tf.axis[1][0], tf.axis[1][1], tf.axis[1][2],
			tf.axis[2][0], tf.axis[2][1], tf.axis[2][2]);
#endif
		if (model->children[i]->children) {
		 C_AriesApplyModelTagframe(model->children[i], &tf);
		 CG_AriesDrawModel(model->children[i], server);
		} else {
		 CG_AriesMeshDraw(model, server);
		}

	}

	/* Now draw the meshes for this model */
	CG_AriesMeshDraw(model, server);
}

/**
 * CG_DrawAries
 *
 * Draw the aries model in correct orientation
 */
void CG_DrawAries(int model, vec3_t origin, refEntity_t *legs,
	refEntity_t *torso, refEntity_t *head)
{
	memset(&(c_aries[model].lower->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].upper->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].head->state), 0, sizeof(ariesModelState_t));

	/* set the positions of the player model */
	VectorCopy(origin, c_aries[model].lower->state.origin);
	MatrixConvert34(legs->axis, c_aries[model].lower->state.rotation);
	MatrixConvert34(torso->axis, c_aries[model].upper->state.rotation);
	MatrixConvert34(head->axis, c_aries[model].head->state.rotation);
	MatrixConvert34(head->axis, c_aries[model].helmet->state.rotation);

	/* set the animations */
	c_aries[model].lower->state.oldFrame = legs->oldframe;
	c_aries[model].lower->state.frame = legs->frame;
	c_aries[model].lower->state.backlerp = legs->backlerp;
	c_aries[model].upper->state.oldFrame = torso->oldframe;
	c_aries[model].upper->state.frame = torso->frame;
	c_aries[model].upper->state.backlerp = torso->backlerp;
	CG_AriesDrawModel(c_aries[model].lower, qfalse);
}

/**
 * CG_DrawAriesEntity
 */
void CG_DrawAriesEntity(centity_t *ent)
{
		/* holds 3 vectors: angles for torso, legs and head */
	angleData_t		adata;
	/* not used here, but passed to the func for compat. with cgame */
	int			pryFlags;
	int			model;
	vec3_t			legsAxis[3];
	vec3_t			torsoAxis[3];
	vec3_t			headAxis[3];
	float			legsPitchAngle, legsYawAngle, torsoPitchAngle, torsoYawAngle;
	entityState_t		s = ent->nextState;

	model = s.eventParm;

	/*
	 * BG_PlayerAngles is an implementation of CG_Player angles and is
	 * now called by both cgame and here in game so that the results are
	 * the same.  Note that for the netcode this is all assumed to be
	 * re-wound so we need to use values that are accounted for in the
	 * history buffer
	 * the critical ones are s.apos.trBase, s.pos.trDelta, and the 4
	 * legs, torso, pitch yaws
	 */
	BG_PlayerAngles(s.apos.trBase,
		s.pos.trDelta,
		s.otherEntityNum,
		s.otherEntityNum2,
		s.frame,
		&adata,
		&legsPitchAngle,
		&legsYawAngle,
		&torsoPitchAngle,
		&torsoYawAngle,
		&pryFlags );

	/* clear the current animation state */
	memset(&(c_aries[model].lower->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].upper->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].head->state), 0, sizeof(ariesModelState_t));
	memset(&(c_aries[model].helmet->state), 0, sizeof(ariesModelState_t));
	/*
	Com_Printf("G adata: {%f %f %f} {%f %f %f} {%f %f %f}\n",
			adata.legAngles[0], adata.legAngles[1],
			adata.legAngles[2],
			adata.torsoAngles[0], adata.torsoAngles[1],
			adata.torsoAngles[2],
			adata.headAngles[0], adata.headAngles[1],
			adata.headAngles[2]);
	*/

	/* now set the positions of the player model */
	VectorCopy(s.pos.trBase, c_aries[model].lower->state.origin);
	AnglesToAxis(adata.legAngles, legsAxis);
	AnglesToAxis(adata.torsoAngles, torsoAxis);
	AnglesToAxis(adata.headAngles, headAxis);
	MatrixConvert34(legsAxis, c_aries[model].lower->state.rotation);
	MatrixConvert34(torsoAxis, c_aries[model].upper->state.rotation);
	MatrixConvert34(headAxis, c_aries[model].head->state.rotation);
	MatrixConvert34(headAxis, c_aries[model].helmet->state.rotation);

	/* set the animations */
	c_aries[model].lower->state.oldFrame = s.otherEntityNum;
	c_aries[model].lower->state.frame = s.otherEntityNum;
	c_aries[model].lower->state.backlerp = 0.0f;
	c_aries[model].upper->state.oldFrame = s.otherEntityNum2;
	c_aries[model].upper->state.frame = s.otherEntityNum2;
	c_aries[model].upper->state.backlerp = 0.0f;

	CG_AriesDrawModel(c_aries[model].lower, qtrue);
}
