/**
 * Filename: c_players.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#ifndef _C_PLAYERS_H_
#define _C_PLAYERS_H_

#include "../game/q_shared.h"
#include "../game/animdef.h"

#define ARIES_ATHENA 0
#define ARIES_ORION 1
#define ARIES_COUNT 2

/*
 * flip the togglebit every time an animation
 * changes so a restart of the same anim can be detected
 */
#define ANIM_TOGGLEBIT 128

typedef struct animation_s {
	int firstFrame;
	int numFrames;
	int loopFrames;  /* 0 to numFrames */
	int frameLerp;	 /* msec between frames */
	int initialLerp; /* msec to get to first frame */
	int reversed;	 /* true if animation is reversed */
	int flipflop;	 /* true if animation should flipflop back to base */
	int totalTime;	 /* initialLerp + numFrames * frameLerp */
} animation_t;

/*
 * player entities need to track more information
 * than any other type of entity.
 *
 * note that not every player entity is a client entity,
 * because corpses after respawn are outside the normal
 * client numbering range
 *
 * when changing animation, set animationTime to frameTime + lerping time
 * The current lerp will finish out, then it will lerp to the new animation
 */
typedef struct {
	int 		 oldFrame;
	int 		 oldFrameTime; /* time when ->oldFrame was exactly on */

	int 		 frame;
	int 		 frameTime;    /* time when ->frame will be exactly on */

	float		 backlerp;

	float		 yawAngle;
	qboolean	 yawing;
	float		 pitchAngle;
	qboolean	 pitching;

	int 		 animationNumber; /* may include ANIM_TOGGLEBIT */
	animation_t  *animation;
	/* time when the first frame of the animation will be exact */
	int 		 animationTime;

	qboolean	 end;
} lerpFrame_t;

typedef struct {
	int 	 legsyawing;
	int 	 legsAnim;
	int 	 torsoAnim;
	lerpFrame_t  legs;
	lerpFrame_t  torso;
	int 	 clientnum;
} anim_t;

void C_PlayerAnimationsInit(void);
qboolean C_ParsePlayerAnimations(char *filename, animation_t *animations);
void C_SetLerpFrameAnimation(animation_t* pAnims, lerpFrame_t *lf, int newAnimation);
void C_RunLerpFrame(animation_t* pAnims, lerpFrame_t *lf, int newAnimation, float speedScale, int time);
void C_ClearLerpFrame(animation_t* pAnims, lerpFrame_t *lf, int animationNumber, int time);
void C_PlayerAnimation(entityState_t *ent, animation_t *anim, lerpFrame_t *lf,
			   lerpFrame_t *tf, int *legsOld, int *legs, float *legsBackLerp,
			   int *torsoOld, int *torso, float *torsoBackLerp, int time);

extern animation_t g_playerAnims[ARIES_COUNT][MAX_PLAYER_ANIMATIONS];
#endif /* _C_PLAYERS_H_ */
