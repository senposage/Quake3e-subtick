// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_event.c -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"

// for the voice chats
#include "../ui/menudef.h"

//==========================================================================

/*
===================
CG_PlaceString

Also called by scoreboard drawing
===================
*/
const char *CG_PlaceString( int rank )
{
	static char  str[64];
	char	     *s, *t;

	if ((rank & RANK_TIED_FLAG) && (cgs.gametype != GT_JUMP))
	{
		rank &= ~RANK_TIED_FLAG;
		t     = "Tied for ";
	}
	else
	{
		t = "";
	}

	if (rank == 1)
	{
		s = S_COLOR_BLUE "1st" S_COLOR_WHITE;		// draw in blue
	}
	else if (rank == 2)
	{
		s = S_COLOR_RED "2nd" S_COLOR_WHITE;		// draw in red
	}
	else if (rank == 3)
	{
		s = S_COLOR_YELLOW "3rd" S_COLOR_WHITE; 	// draw in yellow
	}
	else if (rank == 11)
	{
		s = "11th";
	}
	else if (rank == 12)
	{
		s = "12th";
	}
	else if (rank == 13)
	{
		s = "13th";
	}
	else if (rank % 10 == 1)
	{
		s = va("%ist", rank);
	}
	else if (rank % 10 == 2)
	{
		s = va("%ind", rank);
	}
	else if (rank % 10 == 3)
	{
		s = va("%ird", rank);
	}
	else
	{
		s = va("%ith", rank);
	}

	Com_sprintf( str, sizeof(str), "%s%s", t, s );
	return str;
}

/*
=============
CG_Obituary
=============
*/
static void CG_Obituary( entityState_t *ent )
{
	int	          mod;
	int	          target;
	int	          attacker;
	char	      *message;
	char	      *message2;
	char		  *weapon = NULL;
	clientInfo_t  *ciTarget;
	clientInfo_t  *ciAttacker;
	char	      nameTarget[36];
	char	      nameAttacker[36];

	target	 = ent->otherEntityNum;
	attacker = ent->otherEntityNum2;
	mod	 = ent->eventParm;
	message2 = ".";
	message  = NULL;

	// Target out of range, this is bad news
	if ((target < 0) || (target >= MAX_CLIENTS)) {
		CG_Error( "CG_Obituary: target out of range" );
	}
#ifdef ENCRYPTED
	if (target == cg.predictedPlayerState.clientNum && cg.predictedPlayerState.persistant[PERS_TEAM] != TEAM_SPECTATOR) SendHaxValues(); // r00t: send to server only when you die
#endif
	//@Barbatos
	//@Fenix - check also negative values
	if(cg_drawKillLog.integer <= 0) {
		return;
	}

	// Get the target client info
	ciTarget = &cgs.clientinfo[target];
	Q_strncpyz ( nameTarget, CG_GetColoredClientName ( ciTarget ), sizeof(nameTarget) - 1  );

	// Set some client info dead stuff
	ciTarget->ghost       = qtrue;
	ciTarget->timeofdeath = cg.time;
	ciTarget->bleedCount  = 0;

	// Get the attacker info
	if ((attacker < 0) || (attacker >= MAX_CLIENTS)) {
		attacker   = ENTITYNUM_WORLD;
		ciAttacker = NULL;
		strcpy ( nameAttacker, "unknown" );
	} else {
		ciAttacker = &cgs.clientinfo[attacker];
		Q_strncpyz ( nameAttacker, CG_GetColoredClientName ( ciAttacker ), sizeof(nameAttacker));

		if (target == cg.snap->ps.clientNum) {
			Q_strncpyz( cg.killerName, nameAttacker, sizeof(cg.killerName));
		}
	}

	// If means of death was the bomb.
	if (mod == UT_MOD_BOMBED) {
		CG_Printf("%s was bombed.\n", nameTarget);
		return;
	}

	// If they killed themselves then show it as such
	if (attacker == target) {

		switch(mod) {

			case MOD_CHANGE_TEAM:
				return;

			case UT_MOD_HEGRENADE:
			case UT_MOD_HK69:

				switch (ciTarget->gender) {

					case GENDER_FEMALE:

						if (rand() % 200 == 1) {
							message = "lit her own fart.";
						} else {
							message = "stepped on her own grenade.";
						}
						break;

					default:

						if (rand() % 200 == 1) {
							message = "lit his own fart.";
						} else {
							message = "stepped on his own grenade.";
						}
						break;
				}

				break;

			case UT_MOD_SPLODED:
				message = "'sploded.";
				break;

			case UT_MOD_SACRIFICE:
				message = "sacrificed himself.";
				break;

			case UT_MOD_SLAPPED:
				message = "was SLAPPED to Death by the Server Admin.";
				break;

			//@Barbatos
			case UT_MOD_SMITED:
				message = "was struck down by the Server Admin.";
				break;

			case UT_MOD_NUKED:
				message = "was nuked by the Server Admin.";
				break;

			case UT_MOD_FLAG:
				message = "was killed by a flag.";
				break;

			case UT_MOD_HK69_HIT:
				message = "wtf...";
				break;

			case MOD_FALLING:
				message = "did the lemming thing.";
				break;

			case UT_MOD_FLASHGRENADE:
			case UT_MOD_SMOKEGRENADE:
			default:

				switch (ciTarget->gender) {
					case GENDER_FEMALE:
						message = "killed herself.";
						break;

					case GENDER_MALE:
						message = "killed himself.";
						break;

					default:
						message = "killed itself.";
						break;
				}
				break;
		}
	}
	else {
		switch(mod) {

			case UT_MOD_FLAG:
				message = "was killed by a flag.";
				break;

			case MOD_SUICIDE:
				message = "decided it was all too much.";
				break;

			case MOD_FALLING:
				message = "did the lemming thing.";
				break;

			case MOD_CRUSH:
				message = "was crushed to death.";
				break;

			case MOD_WATER:
				message = "decided to swim with the fishes, permanently.";
				break;

			case MOD_SLIME:
				message = "melted.";
				break;

			case MOD_LAVA:
				message = "does a back flip into the lava.";
				break;

			case MOD_TARGET_LASER:
				message = "saw the light.";
				break;

			case MOD_TRIGGER_HURT:
				message = "was killed by some well placed HURT!";
				break;

			default:
				message = NULL;
				break;
		}
	}

	// If we have a single message then display it and return
	if (message) {
		CG_Printf( "%s %s\n", nameTarget, message);
		return;
	}

	if (attacker != ENTITYNUM_WORLD) {

		switch (mod) {

			case UT_MOD_GOOMBA:
				message = "was curb-stomped by";
				weapon = "Curb-stomp";
				break;

			case MOD_TELEFRAG:
				message  = "tried to invade";
				message2 = "'s personal space.";
				break;

			case UT_MOD_KNIFE:
				message = "was sliced a new orifice by";
				weapon = "Knife";
				break;

			case UT_MOD_KNIFE_THROWN:
				message  = "managed to sheath";
				message2 = "'s flying knife in their flesh.";
				weapon = "Knife thrown";
				break;

			case UT_MOD_BERETTA:
			//case UT_MOD_MAGNUM:
				message = "was pistol whipped by";
				weapon = "Beretta";
				break;

			case UT_MOD_GLOCK:
				message = "got plastic surgery with";
				message2 = "'s Glock.";
				weapon = "Glock";
				break;

			case UT_MOD_DEAGLE:
				message  = "got a whole lot of hole from";
				message2 = "'s DE round.";
				weapon = "Desert Eagle";
				break;

			case UT_MOD_COLT1911:
                message = "was given a new breathing hole by";
                message2 = "'s Colt 1911.";
                weapon = "Colt 1911";
                break;

			case UT_MOD_SPAS:
				message  = "was turned into peppered steak by";
				message2 = "'s SPAS blast.";
				weapon = "SPAS";
				break;

			/*case UT_MOD_BENELLI:
				message  = "was turned into grated cheese by";
				message2 = "'s Benelli.";
				break;
			*/
			case UT_MOD_UMP45:
				message  = "danced the UMP tango to";
				message2 = "'s sweet sweet music.";
				weapon = "UMP45";
				break;

			case UT_MOD_MP5K:
				message = "was MP5K spammed without mercy by";
				weapon = "MP5K";
				break;

			case UT_MOD_MAC11:
                message = "was minced to death by";
                message2 = "'s Mac 11.";
                weapon = "Mac 11";
                break;

			/*case UT_MOD_P90:
				message = "was filled with P90 rounds by";
				break;
			*/
			case UT_MOD_LR:
				message  = "played 'catch the shiny bullet' with";
				message2 = "'s LR-300 rounds.";
				weapon = "LR300";
				break;

			case UT_MOD_G36:
				message  = "was on the wrong end of";
				message2 = "'s G36.";
				weapon = "G36";
				break;

			case UT_MOD_AK103:
				message  = "was torn asunder by";
				message2 = "'s crass AK103.";
				weapon = "AK103";
				break;

			case UT_MOD_M4:
				message  = "got a lead enema from";
				message2 = "'s retro M4.";
				weapon = "M4";
				break;

			case UT_MOD_PSG1:
				message  = "was taken out by";
				message2 = "'s PSG1. Plink!";
				weapon = "PSG1";
				break;

			case UT_MOD_HK69:

				if (ent->torsoAnim == 0) {
					message  = "HEARD";
					message2 = "'s HK69 gren... didn't AVOID it. Sucka.";
				} else {
					message  = "suffered";
					message2 = "'s HK69 Grenade Enema.";
				}

				weapon = "HK69";
				break;

			case UT_MOD_KICKED:

				if (ent->torsoAnim == 0) {
					message  = "got himself some lovin' from";
					message2 = "'s boot o' passion.";
				} else {
					message  = "got";
					message2 = "'s foot broken off in their ass.";
				}

				weapon = "Boot";
				break;

			case UT_MOD_FLASHGRENADE:
				message  = "was flashed by ";
				message2 = " and dropped dead.";
				break;
			
			case UT_MOD_SMOKEGRENADE:
				message  = "was smoked by";
				message2 = "and dropped dead.";
				weapon = "Smoke";
				break;

			case UT_MOD_HEGRENADE:

				if (ent->torsoAnim == 0) {
					message  = "has become a nasty stain thanks to";
					message2 = "'s HE gren.";
				} else {
					message  = "was BBQ'ed by";
					message2 = "'s exploding monkey.";
				}

				weapon = "HE";
				break;

			case UT_MOD_SR8:

				if (ent->torsoAnim == 0) {
					message  = "managed to slow down";
					message2 = "'s SR-8 round just a little.";
				} else {
					message  = "managed to slow down";
					message2 = "'s SR-8 round just a little. NEEP NEEP!";
				}

				weapon = "SR8";
				break;

			case UT_MOD_BLED:
				message  = "bled to death from";
				message2 = "'s attacks.";
				weapon = "Bleed";
				break;

			case UT_MOD_NEGEV:

				if (ent->torsoAnim == 0) {
					message  = "got shredded to pieces by";
					message2 = "'s Negev.";
				} else {
					message  = "got nailed to the wall by";
					message2 = "'s Negev.";
				}

				weapon = "Negev";
				break;

			case UT_MOD_HK69_HIT:

				if (ent->torsoAnim == 0) {
					message  = "was shot down by";
					message2 = "'s guided HK69 'nade.";
				} else {
					message  = "thought it was";
					message2 = "'s pet flying squirrel and tried to catch it. BAD SQUIRREL, BAD.";
				}

				weapon = "HK69";
				break;

			case UT_MOD_NUKED:
				message = "was nuked by accident, intented target was";
				break;

			default:
				message = "was retired from the field of combat by";
				break;
		}
	}

	// Long message
	if ((cg_drawKillLog.integer == 1) && (message)) {

		CG_Printf("%s %s %s%s%s\n", nameTarget, message, nameAttacker, S_COLOR_WHITE, (message2) ? message2 : ".");

		//if we're in the eyes.
		if (target == cg.predictedPlayerState.clientNum) {
			if (ent->generic1 != 255) {
				CG_Printf( "%s had %d%% health.\n", nameAttacker, ent->generic1);
			}
		}

		return;
	}

	// Short message
	if ((cg_drawKillLog.integer == 2) && (weapon)) {

		CG_Printf("%s [%s] %s%s.\n", nameAttacker, weapon, nameTarget, S_COLOR_WHITE);

		//if we're in the eyes.
		if (target == cg.predictedPlayerState.clientNum) {
			if (ent->generic1 != 255) {
				CG_Printf("%s had %d%% health.\n", nameAttacker, ent->generic1);
			}
		}

		return;
	}

	// @Fenix - new message! Minimal info.
	// Will be printed in the top left screen.
	if ((cg_drawKillLog.integer == 3) && (weapon)) {
		CG_Printf("%s has been killed by %s%s.\n", nameTarget, nameAttacker, S_COLOR_WHITE);
		return;
	}

	// @Fenix - new message! Minimal info.
	// Will be printed in the middle-center screen.
	// Actions that doesn't not involve ourselves are shown in the top-left screen.
	if ((cg_drawKillLog.integer == 4) && (weapon)) {

		if (attacker == cg.predictedPlayerState.clientNum) {
			// We are the attacker
			CG_CenterPrint( va("You killed %s\n", nameTarget), SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
		} else	if (target == cg.predictedPlayerState.clientNum) {
			// We are the target
			CG_CenterPrint( va("You have been killed by %s\n", nameAttacker), SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH );
		} else {
			// Not directly involving ourselves
			CG_Printf("%s has been killed by %s%s.\n", nameTarget, nameAttacker, S_COLOR_WHITE);
		}

		return;
	}


	// @Fenix - new message! Minimal info, regarding just ourself.
	// Will be printed in the middle-center screen, like a bigtext.
	// Will contains also some details like in Quake3.
	if ((cg_drawKillLog.integer >= 5) && (weapon)) {

		int i;
		int position = 1;
		clientInfo_t *ci;
		clientInfo_t *us;
		char *suffix;
		char *teamColor;

		if (attacker == cg.predictedPlayerState.clientNum) {
			us = ciAttacker;
		} else if (target == cg.predictedPlayerState.clientNum) {
			us = ciTarget;
		} else {
			// Not directly involving ourselves
			CG_Printf("%s has been killed by %s%s.\n", nameTarget, nameAttacker, S_COLOR_WHITE);
			return;
		}

		// We need to compute the player position in the scoreboard.
		// We'll loop through all the clients assuming we are in position 1.
		// We'll add a +1 everytime we find a player with an higher score than ours.
		// The match will be done according to the player team.
		for (i = 0; i < MAX_CLIENTS; i++) {
			ci = &cgs.clientinfo[i];
			if (!ci) continue; // Not a valid pointer
			if (ci->team != us->team) continue; // Not in our team
			if (ci->score > us->score) position++; // Better score than ours
		}

		// Computing position suffix
		switch (position) {
			case 1:
			case 21:
			case 31:
				suffix = "st";
				break;
			case 2:
			case 22:
			case 32:
				suffix = "nd";
				break;
			case 3:
			case 23:
				suffix = "rd";
				break;
			default:
				suffix = "th";
				break;
		}

		// Computing team color
		switch (us->team) {
			case TEAM_RED:
				teamColor = S_COLOR_RED;
				break;
			case TEAM_BLUE:
				teamColor = S_COLOR_BLUE;
				break;
			default:
				teamColor = S_COLOR_YELLOW;
				break;
		}

		if (attacker == cg.predictedPlayerState.clientNum) {
			// We are the attacker
			CG_CenterPrint(va("You killed %s\n%s%d%s %splace with %d", nameTarget, teamColor, position, suffix, S_COLOR_WHITE, us->score), SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH);
		} else	if (target == cg.predictedPlayerState.clientNum) {
			// We are the target
			CG_CenterPrint(va("You have been killed by %s\n%s%d%s %splace with %d", nameAttacker, teamColor, position, suffix, S_COLOR_WHITE, us->score), SCREEN_HEIGHT * 0.25f, BIGCHAR_WIDTH);
		}

		return;
	}


	// we don't know what it was
	CG_Printf( "%s died.\n", nameTarget );
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : CG_Shot
// Description: Displays who hit who with what
//				* eventparm is damage location
//				* otherentitynumber2 is player who was wounded
//				* otherentitynumber is player who did the wounding
// Author     : Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
static void CG_Shot( entityState_t *es, vec3_t normal, int damage )
{
	//@Barbatos - disable for the jump mode
	if(cgs.gametype == GT_JUMP)
		return;

	//@B1n - don't display 0pc hits
	if(damage <= 0) 
		return;

	// First see if the current client was hit by another client.
	if (cg_showBulletHits.integer == 1)
	{
		if (es->otherEntityNum2 == cg.snap->ps.clientNum)
		{
			if (es->otherEntityNum < MAX_CLIENTS)
			{
				if (cg.snap->ps.pm_flags & PMF_FOLLOW)
				{
					CG_Printf("%s was hit in the %s by %s%s.\n",
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
						c_ariesHitLocationNames[es->time2],
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
						S_COLOR_WHITE);
				}
				else
				{
					CG_Printf("You were hit in the %s by %s%s.\n",
						c_ariesHitLocationNames[es->time2],
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
						S_COLOR_WHITE);
				}
			}
			else
			{
				CG_Printf("You were shot in the %s by Mr Sentry.\n",
						c_ariesHitLocationNames[es->time2]);
			}
		}
		else if (es->otherEntityNum == cg.snap->ps.clientNum)
		{
			if (cg.snap->ps.pm_flags & PMF_FOLLOW)
			{
				CG_Printf("%s hit %s%s in the %s.\n",
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
					S_COLOR_WHITE,
					c_ariesHitLocationNames[es->time2]);
			}
			else
			{
				CG_Printf("You hit %s%s in the %s.\n",
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
					S_COLOR_WHITE,
					c_ariesHitLocationNames[es->time2]);
			}
		}
	}

	if (cg_showBulletHits.integer == 2)
	{
		if (es->otherEntityNum2 == cg.snap->ps.clientNum)
		{
			if (es->otherEntityNum < MAX_CLIENTS)
			{
				if (cg.snap->ps.pm_flags & PMF_FOLLOW)
				{
					CG_Printf("%s was hit in the %s by %s%s for %d%% damage.\n",
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
						c_ariesHitLocationNames[es->time2],
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
						S_COLOR_WHITE,
						damage);
				}
				else
				{
					CG_Printf("You were hit in the %s by %s%s for %d%% damage.\n",
						c_ariesHitLocationNames[es->time2],
						CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
						S_COLOR_WHITE,
						damage);
				}
			}
			else
			{
				CG_Printf("You were shot in the %s by Mr Sentry for %d%% damage.\n",
					c_ariesHitLocationNames[es->time2],
					damage);
			}
		}
		else if (es->otherEntityNum == cg.snap->ps.clientNum)
		{
			if (cg.snap->ps.pm_flags & PMF_FOLLOW)
			{
				CG_Printf("%s hit %s%s in the %s for %d%% damage.\n",
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]),
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
					S_COLOR_WHITE,
					c_ariesHitLocationNames[es->time2],
					damage);
			}
			else
			{
				CG_Printf("You hit %s%s in the %s for %d%% damage.\n",
					CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum2]),
					S_COLOR_WHITE,
					c_ariesHitLocationNames[es->time2],
					damage);
			}
		}
	}

	trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.UT_bodyImpactSound );

#ifdef ENCRYPTED
// r00t: Log hit stats for hax reporting
	if (es->otherEntityNum == cg.snap->ps.clientNum && !(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
		int h = hits_num[0] + 1;
		hits_num[0]=h;
		if (es->time2>0 && es->time2<HL_MAX) hits_num[es->time2]++;
		if (!(h%50)) SendHaxValues(); // send report after 50 hits, just in case
	}
#endif

}

/*
================
CG_ItemPickup

A new item was picked up this frame
================
*/
static void CG_ItemPickup( int itemNum )
{
	cg.itemPickup	       = itemNum;
	cg.itemPickupTime      = cg.time;
	cg.itemPickupBlendTime = cg.time;
	// see if it should be the grabbed weapon

/*
	if ( bg_itemlist[itemNum].giType == IT_WEAPON ) {
		// select it immediately
		if ( cg_autoswitch.integer && bg_itemlist[itemNum].giTag != WP_MACHINEGUN ) {
			cg.weaponSelectTime = cg.time;
			cg.weaponSelect = bg_itemlist[itemNum].giTag;
		}
	}
*/
}

/*
================
CG_PainEvent

Also called by playerstate transition
================
*/
void CG_PainEvent( centity_t *cent, int health )
{
	char	*snd;
	int	contents;
	vec3_t	origin;

	// don't do more than two pain sounds a second
	if (cg.time - cent->pe.painTime < 500)
	{
		return;
	}

	// Is this a bleeding sound?
	if (health & 0x80)
	{
//		cent->pe.painTime = cg.time;
		trap_S_StartSound( NULL, cent->currentState.number, CHAN_VOICE,
				   CG_CustomSound( cent->currentState.number, va("*bleeding%i.wav", 1 + (rand() % 3))));
		return;
	}

	// Are they under water?
	VectorCopy ( cent->lerpOrigin, origin );
	origin[2] += (((cent->currentState.time >> 16) & 0xFF) - 128);
	contents   = trap_CM_PointContents( origin, 0 );

	if (contents & CONTENTS_WATER)
	{
		if		(health < 25)
		{
			snd = "*pain25_1_h2o.wav";
		}
		else if (health < 50)
		{
			snd = "*pain50_1_h2o.wav";
		}
		else if (health < 75)
		{
			snd = "*pain75_1_h2o.wav";
		}
		else
		{
			snd = "*pain100_1_h2o.wav";
		}
	}
	else
	{
		if		(health < 25)
		{
			snd = "*pain25_1.wav";
		}
		else if (health < 50)
		{
			snd = "*pain50_1.wav";
		}
		else if (health < 75)
		{
			snd = "*pain75_1.wav";
		}
		else
		{
			snd = "*pain100_1.wav";
		}
	}

	trap_S_StartSound( NULL, cent->currentState.number, CHAN_VOICE,
			   CG_CustomSound( cent->currentState.number, snd ));

	// save pain time for programitic twitch animation
	cent->pe.painTime	= cg.time;
	cent->pe.painDirection ^= 1;
}

/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
#define DEBUGNAME(x)

/**
 * $(function)
 */
void CG_EntityEvent( centity_t *cent, vec3_t position )
{
	entityState_t  *es;
//	entityState_t	ts;
	int	       event;
	vec3_t	       dir, tvec;
	const char     *s;
	int	       clientNum;
	clientInfo_t   *ci;
	int	       index;
	int	       i;
//   float angle;
	qhandle_t      shader = 0;
	sfxHandle_t    BeepSFX;
	int	       Animation = WEAPON_FIRE;
	localEntity_t  *le, *ex;

	es    = &cent->currentState;
	event = CSE_DEC(es->event & ~EV_EVENT_BITS);

#ifndef ENCRYPTED
	if (cg_debugEvents.integer)
	{
		CG_Printf( "ent:%3i  event:%3i ", es->number, event );
	}
#endif

	if (!event)
	{
		DEBUGNAME("ZEROEVENT");
		return;
	}

	clientNum = es->clientNum;

	if ((clientNum < 0) || (clientNum >= MAX_CLIENTS))
	{
		clientNum = 0;
	}
	ci = &cgs.clientinfo[clientNum];

	switch (event)
	{
		//
		// movement generated events
		//
		case EV_FOOTSTEP:

			if (cg_footsteps.integer)
			{
				surfaceFootstep ( es->number, es->eventParm );
			}
			break;

		case EV_FOOTSTEP_METAL:
			DEBUGNAME("EV_FOOTSTEP_METAL");

			if (cg_footsteps.integer)
			{
				trap_S_StartSound (NULL, es->number, CHAN_BODY,
						   cgs.media.footsteps[FOOTSTEP_METAL][rand() & 3] );
			}
			break;

		case EV_FOOTSPLASH:
			DEBUGNAME("EV_FOOTSPLASH");

			if (cg_footsteps.integer)
			{
				trap_S_StartSound (NULL, es->number, CHAN_BODY,
						   cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3] );
			}
			break;

		case EV_FOOTWADE:
			DEBUGNAME("EV_FOOTWADE");

			if (cg_footsteps.integer)
			{
				trap_S_StartSound (NULL, es->number, CHAN_BODY,
						   cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3] );
			}
			break;

		case EV_SWIM:
			DEBUGNAME("EV_SWIM");

			if (cg_footsteps.integer)
			{
				trap_S_StartSound (NULL, es->number, CHAN_BODY,
						   cgs.media.footsteps[FOOTSTEP_SPLASH][rand() & 3] );
			}
			break;

		 case EV_FALL_SHORT:

            DEBUGNAME("EV_FALL_SHORT");

            // Fenix: normal footstep sound for jump mode if g_noDamage is activated
            // and bypass changes applied to the player Point Of View
            if ((cgs.gametype == GT_JUMP) && (cgs.g_noDamage > 0)) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.footsteps[FOOTSTEP_NORMAL][rand() & 3]);
                break;
            }

            trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.landSound);

            if (clientNum == cg.predictedPlayerState.clientNum) {
                // Smooth landing z changes
                cg.landChange = -8;
                cg.landTime   = cg.time;
            }

            break;

        case EV_FALL_MEDIUM:

            DEBUGNAME("EV_FALL_MEDIUM");

            // Fenix: normal footstep sound for jump mode if g_noDamage is activated
            // and bypass changes applied to the player Point Of View
            if ((cgs.gametype == GT_JUMP) && (cgs.g_noDamage > 0)) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.footsteps[FOOTSTEP_NORMAL][rand() & 3]);
                break;
            }

            trap_S_StartSound(NULL, es->number, CHAN_AUTO, CG_CustomSound(es->number, "*fall1.wav"));

            // Use normal pain sound
            if (clientNum == cg.predictedPlayerState.clientNum) {
                // Smooth landing z changes
                cg.landChange = -16;
                cg.landTime   = cg.time;
            }

            break;

        case EV_FALL_FAR:

            DEBUGNAME("EV_FALL_FAR");

            // Fenix: normal footstep sound for jump mode if g_noDamage is activated
            // and bypass changes applied to the player Point Of View
            if ((cgs.gametype == GT_JUMP) && (cgs.g_noDamage > 0)) {
                trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.footsteps[FOOTSTEP_NORMAL][rand() & 3]);
                break;
            }

            trap_S_StartSound(NULL, es->number, CHAN_AUTO, CG_CustomSound(es->number, "*fall1.wav"));

            // Don't play a pain sound right after this
            cent->pe.painTime = cg.time;

            if (clientNum == cg.predictedPlayerState.clientNum) {
                // Smooth landing z changes
                cg.landChange = -24;
                cg.landTime   = cg.time;
            }

            break;

		case EV_STEP:
			DEBUGNAME("EV_STEP");
			{
				float  oldStep;
				int    delta;

				if (clientNum != cg.predictedPlayerState.clientNum)
				{
					break;
				}

				// if we are interpolating, we don't need to smooth steps
				if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) ||
				    cg_nopredict.integer || cg_synchronousClients.integer)
				{
					return;
				}

				// check for stepping up before a previous step is completed
				delta	      = cg.time - cg.stepTime;
				oldStep       = (delta < STEP_TIME) ? (cg.stepChange * (STEP_TIME - delta) / STEP_TIME) : 0;

				cg.stepChange = oldStep + es->eventParm;

				if (cg.stepChange > MAX_STEP_CHANGE)
				{
					cg.stepChange = MAX_STEP_CHANGE;
				}
				cg.stepTime = cg.time;
				break;
			}

		case EV_STEP_4:
		case EV_STEP_8:
		case EV_STEP_12:
		case EV_STEP_16:	// smooth out step up transitions
			DEBUGNAME("EV_STEP");
			{
				/*; //ARq0n says !
				float	oldStep;
				int		delta;
				int		step;

				if ( clientNum != cg.predictedPlayerState.clientNum ) {
					break;
				}
				// if we are interpolating, we don't need to smooth steps
				if ( cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) ||
					cg_nopredict.integer || cg_synchronousClients.integer ) {
					break;
				}
				// check for stepping up before a previous step is completed
				delta = cg.time - cg.stepTime;
				if (delta < STEP_TIME) {
					oldStep = cg.stepChange * (STEP_TIME - delta) / STEP_TIME;
				} else {
					oldStep = 0;
				}

				// add this amount
				step = 4 * (event - EV_STEP_4 + 1 );
				cg.stepChange = oldStep + step;
				if ( cg.stepChange > MAX_STEP_CHANGE ) {
					cg.stepChange = MAX_STEP_CHANGE;
				}
				cg.stepTime = cg.time;
				*/
				break;
			}

		case EV_JUMP:
			DEBUGNAME("EV_JUMP");
/* URBAN TERROR - no jump sound
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*jump1.wav" ) );
*/
			break;

		case EV_TAUNT:
			DEBUGNAME("EV_TAUNT");
/* URBAN TERROR - no taunt
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*taunt.wav" ) );
*/
			break;

#ifdef MISSIONPACK
/*	case EV_TAUNT_YES:
		DEBUGNAME("EV_TAUNT_YES");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_YES);
		break;
	case EV_TAUNT_NO:
		DEBUGNAME("EV_TAUNT_NO");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_NO);
		break;
	case EV_TAUNT_FOLLOWME:
		DEBUGNAME("EV_TAUNT_FOLLOWME");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_FOLLOWME);
		break;
	case EV_TAUNT_GETFLAG:
		DEBUGNAME("EV_TAUNT_GETFLAG");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONGETFLAG);
		break;
	case EV_TAUNT_GUARDBASE:
		DEBUGNAME("EV_TAUNT_GUARDBASE");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONDEFENSE);
		break;
	case EV_TAUNT_PATROL:
		DEBUGNAME("EV_TAUNT_PATROL");
		CG_VoiceChatLocal(SAY_TEAM, qfalse, es->number, COLOR_CYAN, VOICECHAT_ONPATROL);
		break; */
#endif
		case EV_WATER_TOUCH:
			DEBUGNAME("EV_WATER_TOUCH");
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.watrInSound );
			break;

		case EV_WATER_TOUCH_FAST:
			DEBUGNAME("EV_WATER_TOUCH_FAST");
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.watrInSoundFast );
			break;

		case EV_WATER_LEAVE:
			DEBUGNAME("EV_WATER_LEAVE");
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.watrOutSound );
			break;

		case EV_WATER_UNDER:
			DEBUGNAME("EV_WATER_UNDER");
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.watrUnSound );
			break;

		case EV_WATER_CLEAR:
			DEBUGNAME("EV_WATER_CLEAR");
			// Make sure the gasp isnt played too much
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, CG_CustomSound( es->number, "*gasp.wav" ));
			trap_S_StopLoopingSound(cent->currentState.number);
			break;

		case EV_ITEM_PICKUP:
			DEBUGNAME("EV_ITEM_PICKUP");
			{
				gitem_t  *item;
				int	 index;

				index = es->eventParm;	// player predicted

				if ((index < 1) || (index >= bg_numItems))
				{
					break;
				}
				item = &bg_itemlist[index];

				// show icon and name on status bar
				if (es->number == cg.snap->ps.clientNum)
				{
					if (item->pickup_sound)
					{
						if (cg.time > cg.lastPickupSound + 500)
						{
							trap_S_StartSound (NULL, es->number, CHAN_AUTO, trap_S_RegisterSound( item->pickup_sound, qfalse ));
							cg.lastPickupSound = cg.time;
						}
					}

					CG_ItemPickup( index );
				}
			}
			break;

		case EV_GLOBAL_ITEM_PICKUP:
			DEBUGNAME("EV_GLOBAL_ITEM_PICKUP");
			{
				gitem_t  *item;
				int	 index;

				index = es->eventParm;	// player predicted

				if ((index < 1) || (index >= bg_numItems))
				{
					break;
				}
				item = &bg_itemlist[index];

				// show icon and name on status bar
				if (es->number == cg.snap->ps.clientNum)
				{
					trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, trap_S_RegisterSound( item->pickup_sound, qfalse ));
					CG_ItemPickup( index );
				}
			}
			break;

		//
		// weapon events
		//
		case EV_NOAMMO:
			DEBUGNAME("EV_NOAMMO");

			if (es->number == cg.snap->ps.clientNum)
			{
				trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.noAmmoSound);
//			CG_OutOfAmmoChange();
			}
			break;

		case EV_CHANGE_WEAPON:
			DEBUGNAME("EV_CHANGE_WEAPON");

			if (es->eventParm == 0)
			{
				trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.selectSound );
			}

			if (es->number == cg.snap->ps.clientNum)
			{
				CG_SetViewAnimation ( WEAPON_IDLE );

				if (es->eventParm == 0)
				{
					cg.weaponChangeTime = cg.time + UT_WEAPONHIDE_TIME;
				}
			}

			break;

//   case ET_UT_WALLSLIDE:
		/*		surfaceFootstep ( es->number, SURF_TYPE_WALLSLIDE );
				i=rand()%100;
		      dir[0]=sin(i);
		      dir[1]=cos(i);
		      dir[2]=50;

		      es->pos.trBase[0]+=((rand()%20)-10);
		      es->pos.trBase[1]+=((rand()%20)-10);
		      le=CG_SmokePuff( es->pos.trBase, dir, 3, 1, 1,1, 0.25f, 1000+rand()%1000, cg.time, 0, LEF_SHOWLASER|LEF_ALLOWOVERDRAW|LEF_DIEONIMPACT, cgs.media.grenadeSmokePuffShader );
		      le->pos.trType=TR_GRAVITY;
		      le=CG_SmokePuff( es->pos.trBase, dir, 10+rand()%40, 1, 1,1, 0.25f, 1000+rand()%1000, cg.time, 0, LEF_SHOWLASER|LEF_ALLOWOVERDRAW|LEF_DIEONIMPACT, cgs.media.grenadeSmokePuffShader );
		      break;*/
		case ET_UT_WALLJUMP:

			surfaceFootstep ( es->number, 3 >> 20 );
			ByteToDir(es->eventParm, dir);
			dir[0]		  *= 10;
			dir[1]		  *= 10;
			dir[2]		   = 0;
			es->pos.trBase[0] -= dir[0];
			es->pos.trBase[1] -= dir[1];
			dir[0]		  *= 2;
			dir[1]		  *= 2;
			//dir[2]=150;

			for (i = 0; i < 5; i++)
			{
				tvec[0] = dir[0] + (((rand() % 100) - 50));
				tvec[1] = dir[1] + (((rand() % 100) - 50));
				tvec[2] = dir[2] + (((rand() % 100) - 50));

				le	= CG_SmokePuff( es->pos.trBase, tvec, 1 + rand() % 10, 1, 1, 1, 0.5f, 500 + rand() % 500, cg.time, 0, LEF_ALLOWOVERDRAW, cgs.media.grenadeSmokePuffShader );
				//le->pos.trType=TR_GRAVITY;
			}

			break;

		case EV_FIRE_WEAPON:

			DEBUGNAME("EV_FIRE_WEAPON");

			if (es->weapon == UT_WP_BOMB)
			{
				Animation = WEAPON_IDLE;

				// Loop through our two bomb sites.
				for (i = 0; i < 2; i++)
				{
																						   // If we're within planting range.
					if ((Distance(es->pos.trBase, cgs.cg_BombSites[i].Position) < BOMB_RADIUS) && (cg.predictedPlayerState.groundEntityNum != ENTITYNUM_NONE)) // cgs.cg_BombSites[i].Range)
					{
						Animation = WEAPON_FIRE;
						break;
					}
				}

				if (Animation == WEAPON_FIRE)
				{
					// Adjust the plant animation to match the planting time.
					/*cg_weapons[UT_WP_BOMB].viewAnims[WEAPON_FIRE].frameLerp   = (1000 * cg.bombPlantTime) / cg_weapons[UT_WP_BOMB].viewAnims[WEAPON_FIRE].numFrames;
					cg_weapons[UT_WP_BOMB].viewAnims[WEAPON_FIRE].initialLerp = cg_weapons[UT_WP_BOMB].viewAnims[WEAPON_FIRE].frameLerp;
					bg_weaponlist[UT_WP_BOMB].modes[0].fireTime		  = cg.bombPlantTime;*/

					CG_FireWeapon(cent);
				}
				else
				{
					cg.predictedPlayerState.weaponTime  = 0;
					cg.predictedPlayerState.weaponstate = WEAPON_READY;
				}
			}
			else
			{
				CG_FireWeapon(cent);
			}

			if (es->number == cg.snap->ps.clientNum)
			{
			  int bullets = utPSGetWeaponBullets(&cg.predictedPlayerState);

			  // Unzoom if last bullet is fired.
				if (bullets == 0)
				{
					cg.zoomed = 0;
				}

				CG_SetViewAnimation(Animation);

				cg.weaponSelectTime   = 0;
				cg.weaponHidden       = 0;
				cg.weaponHideShowTime = 0;
			}

			break;

		case EV_UT_READY_FIRE:

			if (es->number == cg.snap->ps.clientNum)
			{
				CG_SetViewAnimation ( WEAPON_READY_FIRE );
			}
			break;

		case EV_UT_RELOAD:

			if (es->number == cg.snap->ps.clientNum)
			{
				cg.zoomed = 0;

				if (cg.reloading)
				{
					CG_SetViewAnimation ( WEAPON_RELOAD );
				}
				else
				{
					cg.reloading = qtrue;
					CG_SetViewAnimation ( WEAPON_RELOAD_START );
				}
			}
			break;

		case EV_UT_WEAPON_MODE:
			break;

		case EV_UT_BOMB_BASE:
			cg.bombExplodeCount = 4;
			cg.bombExplodeTime  = cg.time;
			VectorCopy ( cent->lerpOrigin, cg.bombExplodeOrigin );
			break;

		case EV_UT_CAPTURE_SCORE:

			if(es->time || es->time2)
			{
				CG_Printf ( "%sRed%s scores: %i   %sBlue%s scores: %i\n", S_COLOR_RED, S_COLOR_WHITE, es->time, S_COLOR_BLUE, S_COLOR_WHITE, es->time2 );
			}
			break;

		// otherEntityNum2:  Location that was captured
		// time2	  :  entity number of the flag that was captured
		// time 	  :  team that now owns the flag
		case EV_UT_CAPTURE:
			{
				const char  *location = CG_ConfigString(CS_LOCATIONS + es->otherEntityNum2 );

				if(location && *location)
				{
					CG_Printf ( "%s%s captured the zone: %s\n", CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]), S_COLOR_WHITE, location );
				}
				else
				{
					CG_Printf ( "%s%s captured a zone\n", CG_GetColoredClientName(&cgs.clientinfo[es->otherEntityNum]), S_COLOR_WHITE );
				}

				break;
			}

        case EV_JUMP_START: // @r00t:This is now also used to sync starttime to spectator, sound is not played int that case
			cgs.clientinfo[es->otherEntityNum].jumpStartTime = es->time;
            if (es->loopSound) trap_S_StartLocalSound( cgs.media.UT_JumpTimerStart, CHAN_LOCAL_SOUND );
			break;

		case EV_JUMP_STOP:

			// update the best time
			if(cgs.clientinfo[es->otherEntityNum].jumpStartTime > 0) {
				// we did a better time than our oldest best time or it is our first time
				if(((cgs.clientinfo[es->otherEntityNum].jumpBestTime > 0) &&
					((es->time - cgs.clientinfo[es->otherEntityNum].jumpStartTime) < cgs.clientinfo[es->otherEntityNum].jumpBestTime))
					|| (cgs.clientinfo[es->otherEntityNum].jumpBestTime == 0)) {
					cgs.clientinfo[es->otherEntityNum].jumpBestTime = (es->time - cgs.clientinfo[es->otherEntityNum].jumpStartTime);
				}
			}

			// stop the jump timer
			cgs.clientinfo[es->otherEntityNum].jumpLastEstablishedTime = es->time - cgs.clientinfo[es->otherEntityNum].jumpStartTime;
			cgs.clientinfo[es->otherEntityNum].jumpStartTime = 0;
			trap_S_StartLocalSound( cgs.media.UT_JumpTimerStop, CHAN_LOCAL_SOUND );
			break;

		case EV_JUMP_CANCEL:
			cgs.clientinfo[es->otherEntityNum].jumpStartTime = 0;
			trap_S_StartLocalSound( cgs.media.UT_JumpTimerCancel, CHAN_LOCAL_SOUND );
			break;

		case EV_UT_LADDER:
			surfaceFootstep ( es->number, SURF_TYPE_LADDER );
			break;

		case EV_UT_HEAL_OTHER:
			break;

		case EV_UT_BANDAGE:
		case EV_UT_TEAM_BANDAGE:

			if (es->number == cg.snap->ps.clientNum)
			{
				cg.zoomed      = 0;
				cg.bandageTime = cg.time;
				CG_SetViewAnimation ( WEAPON_IDLE );
			}
			else
			{
				trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.UT_bandageSound );
			}

			break;

		case EV_UT_C4_DEFUSE:

			if (es->otherEntityNum == cg.snap->ps.clientNum)
			{
				cg.bombDefuseTime = cg.time + es->time2;
			}
			trap_S_StartSound ( NULL, es->otherEntityNum, CHAN_AUTO, cgs.media.UT_bombDisarm );
			break;

		case EV_UT_C4_PLANT:

			if (es->otherEntityNum == cg.snap->ps.clientNum)
			{
				cg.bombPlantTime = cg.time + es->time2;
			}
			trap_S_StartSound ( NULL, es->otherEntityNum, CHAN_AUTO, cgs.media.UT_bombPlant );
			break;

		case EV_UT_SP_SNIPERSTART:
			cg.sniperGameRoundTime = cg.time + es->time2;
			break;

		case EV_UT_SP_SNIPEREND:
			cg.sniperGameRoundTime = 0;
			break;

		case EV_UT_SP_SNIPERSCORE:
			cgs.spSniperKills = es->time2;
			break;

		case EV_UT_RACESTART:
			cg.raceStartTime = cg.time;
			break;

		case EV_UT_RACEEND:
			cg.raceStartTime = 0;
			break;

		case EV_UT_SP_SOUND:

			if (es->otherEntityNum == cg.snap->ps.clientNum)
			{
				trap_S_StartSound (NULL, cg.predictedPlayerState.clientNum, CHAN_AUTO, cgs.media.UT_SP_Intro1 );
			}
			break;

		//=================================================================

		//
		// other events
		//
		case EV_PLAYER_TELEPORT_IN:

			DEBUGNAME("EV_PLAYER_TELEPORT_IN");

			// Clear the bleed count for the respawing entity
			cgs.clientinfo[es->clientNum].bleedCount = 0;
//			cgs.clientinfo[es->clientNum].ghost = qfalse;

			break;

		case EV_PLAYER_TELEPORT_OUT:
			DEBUGNAME("EV_PLAYER_TELEPORT_OUT");
			break;

		case EV_UT_STARTRECORD:
			DEBUGNAME("EV_UT_STARTRECORD");

			//@Barbatos: if we are in matchmode and both teams are ready, auto record a demo for new players
			if (cg_autoRecordMatch.integer &&
				(cg.matchState & UT_MATCH_ON) &&
				(cg.matchState & UT_MATCH_RR) &&
				(cg.matchState & UT_MATCH_BR))
			{
				// Start recording a demo.
				CG_RecordDemo();
			}
			break;

		case EV_ITEM_RESPAWN:
			DEBUGNAME("EV_ITEM_RESPAWN");
			cent->miscTime = cg.time; // scale up from this
			break;

		case EV_GRENADE_BOUNCE:
			DEBUGNAME("EV_GRENADE_BOUNCE");

			// Try to play a surface souns first, if not then play the standard bounce sounds
//		surfaceImpactSound ( NULL, es->number, es->eventParm );

			switch (es->eventParm)
			{
				case SURF_TYPE_GRASS:
				case SURF_TYPE_DIRT:
				case SURF_TYPE_MUD:
				case SURF_TYPE_SNOW:
				case SURF_TYPE_SAND:
				case SURF_TYPE_RUG:
				case SURF_TYPE_CARDBOARD:

					if (rand() % 2)
					{
						trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb1bSound );
					}
					else
					{
						trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb2bSound );
					}
					break;

				default:

					if (rand() % 2)
					{
						trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb1aSound );
					}
					else
					{
						trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.hgrenb2aSound );
					}
					break;
			}
			break;

		case EV_UT_BOMBBOUNCE:
			DEBUGNAME("EV_UT_BOMBBOUNCE");

			// Select a sound.
			if (rand() % 2)
			{
				trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.bombBounce1);
			}
			else
			{
				trap_S_StartSound(NULL, es->number, CHAN_AUTO, cgs.media.bombBounce2);
			}

			break;

		case EV_SCOREPLUM:
			DEBUGNAME("EV_SCOREPLUM");
			CG_ScorePlum( cent->currentState.otherEntityNum, cent->lerpOrigin, cent->currentState.time );
			break;

		//
		// missile impacts
		//
		case EV_MISSILE_HIT:
			DEBUGNAME("EV_MISSILE_HIT");
			ByteToDir( es->eventParm, dir );
			CG_MissileHitPlayer( es->weapon, position, dir, es->otherEntityNum );
			break;

		case EV_MISSILE_MISS:
			DEBUGNAME("EV_MISSILE_MISS");
			ByteToDir( es->eventParm, dir );
			CG_MissileHitWall( es->weapon, 0, position, dir, IMPACTSOUND_DEFAULT );
			break;

		case EV_MISSILE_MISS_METAL:
			DEBUGNAME("EV_MISSILE_MISS_METAL");
			ByteToDir( es->eventParm, dir );
			CG_MissileHitWall( es->weapon, 0, position, dir, IMPACTSOUND_METAL );
			break;

		case EV_BULLET_HIT_WALL:
			DEBUGNAME("EV_BULLET_HIT_WALL");
			ByteToDir( es->eventParm, dir );
			CG_Bullet( es->pos.trBase, es->otherEntityNum, dir, qfalse, ENTITYNUM_WORLD, es->time2, es->generic1, es->weapon );

			break;

		case EV_KNIFE_HIT_WALL:
			DEBUGNAME("EV_KNIFE_HIT_WALL");
			trap_S_StartSound (  NULL, es->number, CHAN_FX, cgs.media.hgrenb1aSound );
			break;

		case EV_BULLET_HIT_BREAKABLE:
			DEBUGNAME("EV_BULLET_HIT_BREAKABLE");

			if (CG_ENTITIES(es->otherEntityNum2)->currentState.eType != ET_INVISIBLE)
			{
				ByteToDir(es->eventParm, dir);

				// all this event does is register that the bullet hit a breakable like
				// glass... don't call CG_Bullet because the bullet will generate another
				// event when it hits a wall or a person.
				// es->otherEntityNum is the "thickness" of the glass.	If set to 0, the
				// mark will only be on one side

				switch (es->time2)
				{
					case 0: // Glass (windows).
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_GLASS_BREAKABLE << 20, 1, CG_ENTITIES(es->otherEntityNum2));

						if (es->otherEntityNum)
						{
							VectorMA(es->pos.trBase, (float)es->otherEntityNum, dir, es->pos.trBase);
							VectorInverse(dir);

							surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_GLASS_BREAKABLE << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						}

						break;

					case 1: // Wood.
					case 9:
					case 10:
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_SOFTWOOD << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						break;

					case 2: // Ceramic (vases).
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_CERAMICTILE << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						break;

					case 3: // Plastic.
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_PLASTIC << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						break;

					case 4: // Metal.
					case 5:
					case 6:
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_STEEL << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						break;

					case 7: // Stone.
					case 8:
						surfaceBulletHit(es->pos.trBase, dir, dir, SURF_TYPE_ROCK << 20, 1, CG_ENTITIES(es->otherEntityNum2));
						break;

					default:
						break;
				}
			}

			break;

		case EV_UT_BULLET_HIT_PROTECTED:

			if (cg.clientNum == es->number)
			{
				CG_Printf ( "You hit %s, but %s is currently protected by the respawn gods\n", CG_GetColoredClientName ( &cgs.clientinfo[es->eventParm] ), cgs.clientinfo[es->eventParm].gender == GENDER_MALE ? "he" : "she" );
			}
			else if (cg.clientNum == es->eventParm)
			{
				CG_Printf ( "You were hit by %s, but it reflected off your respawn protection\n", CG_GetColoredClientName ( &cgs.clientinfo[es->number] ));
			}

			break;

		case EV_BULLET_HIT_FLESH:
			{
				int  surface;

				// @Fenix - if we are playing jump mode
				// bypass visible player hits.
				if (cgs.gametype == GT_JUMP) {
					break;
				}

				DEBUGNAME("EV_BULLET_HIT_FLESH");

				if (es->legsAnim == UT_WP_KICK)
				{
					trap_S_StartSound( NULL, es->otherEntityNum, CHAN_AUTO, cgs.media.kickSound );
				}

				switch (es->time2)
				{
					default:
						surface = SURFACE_SET_TYPE(0, SURF_TYPE_FLESH);
						CG_BleedPlayer(es->pos.trBase, es->pos.trDelta, -700);
						break;

					case HL_HEAD:
						surface = SURFACE_SET_TYPE(0, SURF_TYPE_HEAD);
						CG_BleedPlayer(es->pos.trBase, es->pos.trDelta, -700);
						break;

					case HL_HELMET:
						surface = SURFACE_SET_TYPE(0, SURF_TYPE_HELMET);
						break;

						//	case HL_VEST:
						//		surface = SURFACE_SET_TYPE(0,SURF_TYPE_VEST);
						//		break;
				}

				//if we're doing a head explodie

				//if (es->angles2[0]==1000)
/*if ((int)es->angles2[0] & UT_NO_HEAD_BIT)
		{
			CG_Bleed( es->pos.trBase,-1 );
		  //s->pos.trBase[2]+=0.5f;
	 for (j=0;j<20;j++)
			{
				angle=rand()*1000;
				dir[0]=sin(angle)*5;
				dir[1]=cos(angle)*5;
				dir[2]=(rand()%1000)/200;


				//surfaceEffectFlesh ( es->pos.trBase, es->pos.trBase, dir, 30 );

				CG_BleedPlayer(es->pos.trBase,dir,20+rand()%70);
			}

		} */

				//
				ByteToDir( es->eventParm, dir );
				CG_Bullet( es->pos.trBase, es->otherEntityNum, dir, qtrue, es->eventParm, surface, es->generic1, es->legsAnim );
				CG_Shot(es, dir, es->generic1);

				// Set the bleeeding from var for the hud icon if this is the current player
				// and its not a helmet or vest shot.
				if (es->otherEntityNum2 == cg.snap->ps.clientNum)
				{
					if ((es->time2 != HL_HELMET) && (es->time2 != HL_VEST))
					{
						cg.bleedingFromCount++;
						cg.bleedingFrom[es->time2] = qtrue;
					}

					//if the weapons the sr8, unzoom it if the damage was high enough
					//and its an sr8
					//Note: this'll also be confirmed by the server by treating shots as unzoomed for the next
					//200ms or so

					if (es->generic1 >= 20) {

					    if ((es->time2 != HL_LEGLL) && (es->time2 != HL_LEGLR) && (es->time2 != HL_FOOTL) &&  (es->time2 != HL_FOOTR)) {

					        int  weaponID = utPSGetWeaponID(&cg.snap->ps);

							if (weaponID == UT_WP_SR8) {
								CG_UTZoomReset();
							}

						}
					}
				}

				if (cg_visibleBleeding.integer) {

					// add a "bleeding" wound to the player
					if(cgs.clientinfo[es->otherEntityNum2].bleedCount < 5) {
						index = cgs.clientinfo[es->otherEntityNum2].bleedCount;
						cgs.clientinfo[es->otherEntityNum2].bleedTriangle[index] = (es->time >> 16);
						cgs.clientinfo[es->otherEntityNum2].bleedHitLoc[index]	 = (es->time & 0x00FF);
						cgs.clientinfo[es->otherEntityNum2].bleedTime[index]	 = cg.time + 1;
						cgs.clientinfo[es->otherEntityNum2].bleedCount++;
					}
				}

				if (cg_hitsound.integer)
				{
					if (es->otherEntityNum == cg.snap->ps.clientNum)
					{
						trap_S_StartSound ( NULL, es->otherEntityNum, CHAN_AUTO, cgs.media.hit );
					}
				}

				//bloody core
				ex		     = CG_AllocLocalEntity();
				ex->leType	     = LE_SPRITE_EXPLOSION;
				ex->refEntity.radius = 40;

				// randomly rotate sprite orientation
				ex->refEntity.rotation = cg.time % 360;
				ex->startTime	       = cg.time;
				ex->endTime	       = ex->startTime + 266;

				// bias the time so all shader effects start correctly
				ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
				ex->refEntity.hModel	   = 0;
				ex->refEntity.customShader = cgs.media.bloodBoom;

				// set origin
				VectorCopy( es->pos.trBase, ex->refEntity.origin );
				VectorCopy( es->pos.trBase, ex->refEntity.oldorigin );
				ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

				if (es->time2 == HL_HELMET)
				{
					//sparky core
					ex		     = CG_AllocLocalEntity();
					ex->leType	     = LE_SPRITE_EXPLOSION;
					ex->refEntity.radius = 40;

					// randomly rotate sprite orientation
					ex->refEntity.rotation = cg.time % 360;
					ex->startTime	       = cg.time;
					ex->endTime	       = ex->startTime + 266;

					// bias the time so all shader effects start correctly
					ex->refEntity.shaderTime   = ex->startTime / 1000.0f;
					ex->refEntity.hModel	   = 0;
					ex->refEntity.customShader = cgs.media.sparkBoom;

					// set origin
					VectorCopy( es->pos.trBase, ex->refEntity.origin );
					VectorCopy( es->pos.trBase, ex->refEntity.oldorigin );
					ex->color[0] = ex->color[1] = ex->color[2] = 1.0;
				}

				break;
			}

		case EV_SHOTGUN:
			DEBUGNAME("EV_SHOTGUN");
			CG_ShotgunFire( es );
			break;

		case EV_GENERAL_SOUND:
			DEBUGNAME("EV_GENERAL_SOUND");

			if (cgs.gameSounds[es->eventParm])
			{
				trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.gameSounds[es->eventParm] );
			}
			else
			{
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );

				if ((s != NULL) && *s)
				{
					trap_S_StartSound (NULL, es->number, CHAN_AUTO, CG_CustomSound( es->number, s ));
				}
			}
			break;

		case EV_GLOBAL_SOUND: // play from the player's head so it never diminishes
			DEBUGNAME("EV_GLOBAL_SOUND");

			if (cgs.gameSounds[es->eventParm])
			{
				trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[es->eventParm] );
			}
			else
			{
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );

				if ((s != NULL) && *s)
				{
					trap_S_StartSound (NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( es->number, s ));
				}
			}
			break;

//Iain
		case EV_GLOBAL_TEAM_SOUND:
			{
				switch (es->eventParm)
				{
					case GTS_RED_TAKEN:
					case GTS_BLUE_TAKEN:
							trap_S_StartLocalSound(cgs.media.UT_CTF_FlagTaken, CHAN_ANNOUNCER);
							break;

					case GTS_RED_RETURN:
					case GTS_BLUE_RETURN:
							trap_S_StartLocalSound(cgs.media.UT_CTF_FlagReturn, CHAN_ANNOUNCER);
							break;

					case GTS_BLUE_CAPTURE:
						trap_S_StartLocalSound(cgs.media.UT_CTF_CaptureBlue, CHAN_ANNOUNCER);
						break;

					case GTS_RED_CAPTURE:
						trap_S_StartLocalSound(cgs.media.UT_CTF_CaptureRed, CHAN_ANNOUNCER);
						break;
				}
				break;
			}

		case EV_PAIN:
			// local player sounds are triggered in CG_CheckLocalSounds,
			// so ignore events on the player
			DEBUGNAME("EV_PAIN");
//		if ( cent->currentState.number != cg.snap->ps.clientNum ) {
			CG_PainEvent( cent, es->eventParm );
//		}
			break;

		case EV_DEATH1:
		case EV_DEATH2:
		case EV_DEATH3:
		case EV_DEATH4:
		case EV_DEATH5:
		case EV_DEATH6:
		case EV_DEATH7:
		case EV_DEATH8:
			{
				vec3_t	origin;
				char	*water;
				int  delay, delta;

				DEBUGNAME("EV_DEATHx");

				if (cent->currentState.number == cg.snap->ps.clientNum)
				{
					if (!cgs.survivor)
					{
						// If we're doing wave respawns.
						if (cgs.waveRespawns && (cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP) && ((cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED) || (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_BLUE)))
						{

							if (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_RED)
							{
								delay = cgs.redWaveRespawnDelay;
							}
							else
							{
								delay = cgs.blueWaveRespawnDelay;
							}

							if (delay <= 2)
							{
								delay = 2;
							}

							/*	delta = (int)((cg.time / 1000) - (cgs.levelStartTime / 1000));

								if (delta <= delay)
								{
									delta = (delay - delta) * 1000;
								}
								else
								{
									delta = (delay - (delta % delay)) * 1000;
								}

								cg.respawnTime = cg.time + delta; */
							delta	       = (cg.time) + ((delay * 1000) - ((cg.time - cg.pausedTime) % (delay * 1000)));
							cg.respawnTime = delta;

							// If death was the result of a suicide.
							if (es->eventParm == MOD_SUICIDE)
							{
								// Increase respawn delay.
								cg.respawnTime += delay * 1000;
							}
						}


						else if (cgs.respawnDelay > 2)
						{
							cg.respawnTime = cg.time + cgs.respawnDelay * 1000;
						}
						else
						{
							cg.respawnTime = 0;
						}
					}
					else
					{
						cg.respawnTime = 0;
					}

					cg.zoomed     = 0;
					cg.zoomedFrom = 0;

					if (!(cg.snap->ps.pm_flags & PMF_FOLLOW))
					{
//				CG_SetColorOverlay ( 0, 0, 0, 1, 5000, UT_OCFADE_LINEAR_OUT, 4000 );

						cg.lastDeathTime = cg.time;
					}
				}

				// Are they under water?
				VectorCopy ( cent->lerpOrigin, origin );
				origin[2] += (((cent->currentState.time >> 16) & 0xFF) - 128);

				if (trap_CM_PointContents( origin, 0 ) & CONTENTS_WATER)
				{
					water = "_h2o";
				}
				else
				{
					water = "";
				}

				trap_S_StartSound( NULL, es->number, CHAN_VOICE,
						   CG_CustomSound( es->number, va("*death%i%s.wav", (rand() % 3) + 1, water)));

				break;
			}

		case EV_OBITUARY:
			DEBUGNAME("EV_OBITUARY");
			CG_Obituary( es );
			break;

		//
		// powerup events
		//
		case EV_POWERUP_QUAD:
			DEBUGNAME("EV_POWERUP_QUAD");
/* URBAN TERROR - No quad
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_QUAD;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.quadSound );
*/
			break;

		case EV_POWERUP_BATTLESUIT:
			DEBUGNAME("EV_POWERUP_BATTLESUIT");
/* URBAN TERROR - No battlesuit
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_BATTLESUIT;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.protectSound );
*/
			break;

		case EV_POWERUP_REGEN:
			DEBUGNAME("EV_POWERUP_REGEN");
/* URBAN TERROR - No regen
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = PW_REGEN;
			cg.powerupTime = cg.time;
		}
		trap_S_StartSound (NULL, es->number, CHAN_ITEM, cgs.media.regenSound );
*/
			break;

		case EV_GIB_PLAYER:
			DEBUGNAME("EV_GIB_PLAYER");
			break;

		case EV_STOPLOOPINGSOUND:
			DEBUGNAME("EV_STOPLOOPINGSOUND");
			trap_S_StopLoopingSound( es->number );
			es->loopSound = 0;
			break;

		case EV_DEBUG_LINE:
			DEBUGNAME("EV_DEBUG_LINE");
			CG_Beam( cent );
			break;

		case EV_UT_BREAK: // break a breakable object
			DEBUGNAME("EV_UT_BREAK");

			ByteToDir(es->time2, dir);

			// If bombable.
			if (es->frame > 1)
			{
				// Calculate origin.
				tvec[0] = (es->origin[0] + es->origin2[0]) / 2.0f;
				tvec[1] = (es->origin[1] + es->origin2[1]) / 2.0f;
				tvec[2] = (es->origin[2] + es->origin2[2]) / 2.0f;

				// If origin is under water.
				if (CG_PointContents(tvec, 0) & CONTENTS_WATER)
				{
					BeepSFX = cgs.media.UT_explodeH2OSound;
				}
				else
				{
					// Select sound file at random.
					switch (rand() & 3)
					{
						case 1:
							BeepSFX = trap_S_RegisterSound("sound/bomb/Explo_1.wav", qfalse);
							break;

						case 2:
							BeepSFX = trap_S_RegisterSound("sound/bomb/Explogas_1.wav", qfalse);
							break;

						case 3:
							BeepSFX = trap_S_RegisterSound("sound/bomb/Explogas_2.wav", qfalse);
							break;

						default:
							BeepSFX = trap_S_RegisterSound("sound/bomb/bombexplode.wav", qfalse);
							break;
					}
				}

				// Play explosion sound.
				trap_S_StartSound(tvec, ENTITYNUM_WORLD, CHAN_AUTO, BeepSFX);

				// Generate explosion.
				CG_FragExplosion(tvec, dir, es->frame * 2, cgs.media.dishFlashModel,
						 cgs.media.grenadeExplosionShader, 600);
			}

			switch (es->generic1)
			{
				case 0: // Glass (windows).
					shader = cgs.media.UT_glassFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_glassShatterSound);

					break;

				case 1: // Wood.
				case 9:
				case 10:
					shader = cgs.media.UT_woodFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_woodShatterSound);

					break;

				case 2: // Ceramic (vases).
					shader = cgs.media.UT_glassFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_ceramicShatterSound);

					break;

				case 3: // Plastic.
					shader = cgs.media.UT_glassFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_plasticShatterSound);

					break;

				case 4: // Metal.
				case 5:
				case 6:
					shader = cgs.media.UT_metalFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_metalShatterSound);

					break;

				case 7: // Stone.
				case 8:
					shader = cgs.media.UT_stoneFragment;
					trap_S_StartSound(es->origin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.UT_stoneShatterSound);

					break;

				default:
					break;
			}

			// loop through mark entities and remove any that have the
			// breakable ent marked as parent
			CG_RemoveChildMarks(es->number);

			CG_BreakBreakable(es, dir, shader);

			break;

		case EV_UT_KICK: // FIX ME: remove this as a client event
			break;

		case EV_UT_USE:
			break;

		case EV_UT_CLEARMARKS: // for round restarts
			CG_ClearAllMarks();
			break;

		case EV_UT_GRAB_LEDGE:
			trap_S_StartSound( NULL, es->number, CHAN_AUTO, cgs.media.ledgegrab);
			break;
//
// Below DensiCode: Ph34r it (for its bugs haha :p)
//

		case EV_UT_REDWINSROUND_SOUND:
			DEBUGNAME( "EV_UT_REDWINSROUND_SOUND" );

			if(/* ( cg.snap->ps.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_GHOST )  ||*/ cent->currentState.number == cg.snap->ps.clientNum)
			{
				trap_S_StartLocalSound( cgs.media.UT_RedWinsRound, CHAN_LOCAL_SOUND );
			}

			switch(es->eventParm)
			{
				case 0: // Team not owned
					cg.IsOwnedSound = qfalse;
					cg.OwnedType	= 0;
					break;

				case 1: // Team Owned
					cg.IsOwnedSound        = qtrue;
					cg.OwnedSoundTimeStart = cg.time + 2000;
					cg.OwnedType	       = 1;
					break;

				case 2: // Special Corn Ownage
					cg.IsOwnedSound        = qtrue;
					cg.OwnedSoundTimeStart = cg.time + 2000;
					cg.OwnedType	       = 2;
					break;

				default:
					cg.OwnedType = 0;
					break;
			}
			cg.RoundFadeStart = cg.time + 4300;

			break;

		case EV_UT_BLUEWINSROUND_SOUND:
			DEBUGNAME( "EV_UT_BLUEWINSROUND_SOUND" );

			if(/*( cg.snap->ps.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_GHOST ) || */ cent->currentState.number == cg.snap->ps.clientNum)
			{
				trap_S_StartLocalSound( cgs.media.UT_BlueWinsRound, CHAN_LOCAL_SOUND );
			}

			switch(es->eventParm)
			{
				case 0: // Team not owned
					cg.IsOwnedSound = qfalse;
					cg.OwnedType	= 0;
					break;

				case 1: // Team Owned
					cg.IsOwnedSound        = qtrue;
					cg.OwnedSoundTimeStart = cg.time + 2000;
					cg.OwnedType	       = 1;
					break;

				case 2: // Special Cone Ownage
					cg.IsOwnedSound        = qtrue;
					cg.OwnedSoundTimeStart = cg.time + 2000;
					cg.OwnedType	       = 2;
					break;

				default:
					cg.OwnedType = 0;
					break;
			}
			cg.RoundFadeStart = cg.time + 4300;

			break;

		case EV_UT_DRAWNROUND_SOUND:
			DEBUGNAME( "EV_UT_DRAWNROUND_SOUND" );

			if(/*( cg.snap->ps.stats[ STAT_UTPMOVE_FLAGS ] & UT_PMF_GHOST )  ||*/ cent->currentState.number == cg.snap->ps.clientNum)
			{
				trap_S_StartLocalSound(cgs.media.UT_DrawnRound, CHAN_LOCAL_SOUND );
			}
			cg.IsOwnedSound   = qfalse; // just to make sure
			cg.OwnedType	  = 0;

			cg.RoundFadeStart = cg.time + 4300;

			break;

		case EV_UT_SLAP_SOUND:
			DEBUGNAME( "EV_UT_SLAP_SOUND" );
			trap_S_StartSound( es->pos.trBase, es->clientNum, CHAN_AUTO, trap_S_RegisterSound( "sound/misc/slap2.wav", qfalse ));
			break;

		case EV_UT_NUKE_SOUND:
			DEBUGNAME("EV_UT_NUKE_SOUND");
			trap_S_StartSound( es->pos.trBase, es->clientNum, CHAN_AUTO, trap_S_RegisterSound("sound/nuke/airplane.wav", qfalse));
			break;

		case EV_UT_FADEOUT:
			DEBUGNAME( "EV_UT_FADEOUT" );
			CG_SetColorOverlay( 0, 0, 0, 0, 5000, UT_OCFADE_LINEAR_OUT, 500 );
			break;

		// Bomb Beep ticks
		case EV_UT_BOMBBEEP:
			DEBUGNAME( "EV_UT_BOMBBEEP" );

			if(es->eventParm == 2)
			{
				BeepSFX = cgs.media.ut_BombBeep[2];
			}
			else if(es->eventParm == 1)
			{
				BeepSFX = cgs.media.ut_BombBeep[1];
			}
			else
			{
				BeepSFX = cgs.media.ut_BombBeep[0];
			}
			// Highsea NO NO NO! - DensitY
			// we don't want this to be heard by everyone..
//		if (es->eventParm == 2)
//			trap_S_StartLocalSound(BeepSFX, CHAN_AUTO);
//		else
			trap_S_StartSound( es->pos.trBase, es->number, CHAN_AUTO, BeepSFX );

			break;

		case EV_UT_BOMBDEFUSE_START:
			DEBUGNAME("EV_UT_BOMBDEFUSE_START");

			cg.BombDefused	   = qfalse;
			cg.DefuseLength    = es->eventParm * 1000;
			cg.DefuseStartTime = cg.time;
			cg.DefuseEndTime   = 0;

			break;

		case EV_UT_BOMBDEFUSE_STOP:
			DEBUGNAME("EV_UT_BOMBDEFUSE_STOP");

			// If defuse was successful.
			if (es->eventParm)
			{
				cg.DefuseOverlay   = qfalse;
				cg.BombDefused	   = qtrue;
				cg.DefuseStartTime = 0;
				cg.DefuseEndTime   = 0;

				// Playback disarm sound.
				trap_S_StartSound(cg.bombPlantOrigin, ENTITYNUM_WORLD, CHAN_AUTO, cgs.media.bombDisarmSound);
			}
			else
			{
				cg.DefuseOverlay   = qfalse;
				cg.BombDefused	   = qfalse;
				cg.DefuseStartTime = 0;
				cg.DefuseEndTime   = 0;
			}

			break;

		case EV_UT_GOOMBA:
			DEBUGNAME("EV_UT_GOOMBA");

			trap_S_StartSound( es->pos.trBase, es->number, CHAN_AUTO, cgs.media.stomp);

			break;

		default:
			DEBUGNAME("UNKNOWN");
			CG_Error( "Unknown event: %i", event );
			break;
	}
}

/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents( centity_t *cent )
{
	// check for event-only entities
	if (cent->currentState.eType > ET_EVENTS)
	{
		if (cent->previousEvent)
		{
			return; // already fired
		}

		// if this is a player event set the entity number of the client entiyt number
		if (cent->currentState.eFlags & EF_PLAYER_EVENT)
		{
			cent->currentState.number = cent->currentState.otherEntityNum;
		}

		cent->previousEvent	 = 1;

		cent->currentState.event = cent->currentState.eType - ET_EVENTS;
	}
	else
	{
		// check for events riding with another entity
		if (cent->currentState.event == cent->previousEvent)
		{
			return;
		}
		cent->previousEvent = cent->currentState.event;

		if ((CSE_DEC(cent->currentState.event & ~EV_EVENT_BITS)) == 0)
		{
			return;
		}
	}

	// calculate the position at exactly the frame time
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
	CG_SetEntitySoundPosition( cent );

	CG_EntityEvent( cent, cent->lerpOrigin );
}
