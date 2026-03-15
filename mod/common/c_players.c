/**
 * Filename: c_players.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * This is an implementation of player animations moved from
 * cgame to common so that it is accessible from both game and
 * cgame.
 *
 */

#include "c_cvars.h"
#include "c_players.h"
#include "c_syscalls.h"

animation_t g_playerAnims[ARIES_COUNT][MAX_PLAYER_ANIMATIONS];

void C_PlayerAnimationsInit(void)
{
	C_ParsePlayerAnimations("models/players/orion/animation.cfg",
		g_playerAnims[ARIES_ORION]);
	C_ParsePlayerAnimations("models/players/athena/animation.cfg",
		g_playerAnims[ARIES_ATHENA]);
}

/**
 * C_ParsePlayerAnimations
 */
qboolean C_ParsePlayerAnimations(char *filename, animation_t *animations)
{
	pc_token_t  token;
	int	    anim;
	int	    handle;

	/* Open up the source to parse */
	if (!(handle = trap_PC_LoadSource(filename))) {
		return qfalse;
	}

	/* read optional parameters */
	while (1) {
		/* Get the animation # */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		anim = token.intvalue;

		/* Get the first frame */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		animations[anim].firstFrame = token.intvalue;

		/* Get the frame count */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		animations[anim].numFrames = token.intvalue;

		/* Get the frame lerp */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		animations[anim].frameLerp   = 1000 / token.intvalue;
		animations[anim].initialLerp = 1000 / token.intvalue;

		/* Get the loop frames */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		animations[anim].loopFrames = token.intvalue;

		/* Get the reversed flag */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		animations[anim].reversed = token.intvalue;

		/* Get the upper flag and ignore it */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		/* Get the lower flag and ignore it */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		/* Get the hit detection flag and ignore it */
		if (!trap_PC_ReadToken(handle, &token)) {
			break;
		}

		/* calculate the total time of the animation */
		animations[anim].totalTime = animations[anim].initialLerp +
			animations[anim].frameLerp * animations[anim].numFrames;
	}

	trap_PC_FreeSource(handle);

	return qtrue;
}
/**
 * C_SetLerpFrameAnimation
 *
 * may include ANIM_TOGGLEBIT
 */
void C_SetLerpFrameAnimation(animation_t* pAnims, lerpFrame_t *lf, int newAnimation)
{
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if (newAnimation < 0 || newAnimation >= MAX_PLAYER_ANIMATIONS) {
		Com_Error(ERR_FATAL, "Bad animation number: %i", newAnimation);
	}

	anim = pAnims + newAnimation;

	lf->animation = anim;
	lf->end = qfalse;
	lf->animationTime = lf->frameTime + anim->initialLerp;

	if (c_debugAnim.integer) {
		Com_Printf("Anim: %i\n", newAnimation);
	}
}

/**
 * C_RunLerpFrame
 *
 * Sets cg.snap, cg.oldFrame, and cg.backlerp
 * time should be between oldFrameTime and frameTime after exit
 */
void C_RunLerpFrame(animation_t* pAnims, lerpFrame_t *lf, int newAnimation, float speedScale, int time)
{
	int			f, numFrames;
	animation_t	*anim;

	// debugging tool to get no animations
	if (c_animSpeed.integer == 0) {
		lf->oldFrame = lf->frame = lf->backlerp = 0;
		return;
	}

	// see if the animation sequence is switching
	if (newAnimation != lf->animationNumber || !lf->animation) {
		if (c_debugAnim.integer) {
			Com_Printf("Set LF animation: time:%d lfanim:%d, Newanim:%d\n", time, lf->animationNumber, newAnimation);
		}

		C_SetLerpFrameAnimation(pAnims, lf, newAnimation);
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if (time >= lf->frameTime) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		anim = lf->animation;

		if (!anim->frameLerp) {
			if (c_debugAnim.integer) {
				Com_Printf("Warning: no framelerp for anim!\n");
			}

			return; 	// shouldn't happen
		}

		if (time < lf->animationTime) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}

		f = (lf->frameTime - lf->animationTime) / anim->frameLerp;
		f *= speedScale;		// adjust for haste, etc

		numFrames = anim->numFrames;

		if (anim->flipflop) {
			numFrames *= 2;
		}

		if (f >= numFrames) {
			f -= numFrames;

			if (anim->loopFrames) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = time;

				lf->end = qtrue;
			}
		}

		if (anim->reversed) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - f;
		} else if (anim->flipflop && f>=anim->numFrames) {
			lf->frame = anim->firstFrame + anim->numFrames - 1 - (f%anim->numFrames);
		} else {
			lf->frame = anim->firstFrame + f;
		}

		if (time > lf->frameTime) {
			lf->frameTime = time;

			if (c_debugAnim.integer) {
				Com_Printf("Clamp lf->frameTime\n");
			}
		}
	}

	if (lf->frameTime > time + 200) {
		lf->frameTime = time;
	}

	if (lf->oldFrameTime > time) {
		lf->oldFrameTime = time;
	}

	// calculate current lerp value
	if (lf->frameTime == lf->oldFrameTime) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)(time - lf->oldFrameTime) / (lf->frameTime - lf->oldFrameTime);
	}
}


/**
 * C_ClearLerpFrame
 */
void C_ClearLerpFrame(animation_t* pAnims, lerpFrame_t *lf, int animationNumber, int time)
{
	lf->frameTime = lf->oldFrameTime = time;
	C_SetLerpFrameAnimation(pAnims, lf, animationNumber);
	lf->oldFrame = lf->frame = lf->animation->firstFrame;
}


/**
 * C_PlayerAnimation
 */
void C_PlayerAnimation(entityState_t *ent, animation_t *anim, lerpFrame_t *lf,
		       lerpFrame_t *tf, int *legsOld, int *legs, float *legsBackLerp,
		       int *torsoOld, int *torso, float *torsoBackLerp, int time)
{
	float			speedScale;
	float			legSpeedScale;

	if (c_noPlayerAnims.integer) {
		*legsOld = *legs = *torsoOld = *torso = 0;
		return;
	}

	legSpeedScale = 1;
	speedScale = 1;

	// do the shuffle turn frames locally
	if (lf->yawing && (ent->legsAnim & ~ANIM_TOGGLEBIT) == LEGS_IDLE) {
		C_RunLerpFrame(anim, lf, LEGS_TURN, speedScale, time);
	} else {
		C_RunLerpFrame(anim, lf, ent->legsAnim, legSpeedScale, time);
	}

	*legsOld = lf->oldFrame;
	*legs = lf->frame;
	*legsBackLerp = lf->backlerp;

	C_RunLerpFrame(anim, tf, ent->torsoAnim, speedScale, time);

	*torsoOld = tf->oldFrame;
	*torso = tf->frame;
	*torsoBackLerp = tf->backlerp;

	if ((c_debugAnim.integer) && (time & 0x100)) {
		Com_Printf("	  TFrame: %d[a:%d]\n", tf->frame,
			   tf->animationNumber);
		Com_Printf("	  LFrame: %d[a:%d]\n", lf->frame,
			   lf->animationNumber);
	}
}
