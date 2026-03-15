// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"

gentity_t  *Groups[MAX_GROUPS_PER_TEAM * 2];
int 	   TSnumGroups = 0;
int 	   TSredGroup  = -1;
int 	   TSblueGroup = -1;
qboolean   TSswapped;

typedef struct teamgame_s
{
	float		  last_flag_capture;
	int 		  last_capture_team;
	flagStatus_t  redStatus;	// CTF
	flagStatus_t  blueStatus;	// CTF
	flagStatus_t  flagStatus;	// One Flag CTF
	int 		  redTakenTime;
	int 		  blueTakenTime;

	int 		  redBaseTime;
	int 		  blueBaseTime;

	int 		  redObeliskAttackedTime;
	int 		  blueObeliskAttackedTime;

	int 		  hotpotato;
} teamgame_t;

teamgame_t	teamgame;

gentity_t	*neutralObelisk;

void Team_SetFlagStatus (team_t team, flagStatus_t status);

/**
 * RushFlagTouched
 */
static void RushFlagTouched (gentity_t *flag, gentity_t *other, trace_t *trace)
{
	if (!other->client->sess.teamLeader)
	{
		return;
	}

	// TODO: Assumes that team numbers match flag model index
	if ((int)other->client->sess.sessionTeam == flag->s.modelindex)
	{
		return;
	}

	switch (other->client->sess.sessionTeam)
	{
		case TEAM_RED:
			level.redCapturePointsHeld = 1;
			break;

		case TEAM_BLUE:
			level.blueCapturePointsHeld = 1;
			break;
		default:
			Com_Error(ERR_FATAL, "Team %d touched flag.");
			break;
	}
}

/**
 * Team_InitGame
 */
void Team_InitGame (void)
{
	gentity_t  *spot;

	memset (&teamgame, 0, sizeof(teamgame));

	// add the flags as goals
	spot = NULL;

	while ((spot = G_FindClass (spot, bg_itemlist[UT_ITEM_NEUTRALFLAG].classname)) != NULL)
	{
		if(level.numLTGItems < MAX_BOT_ITEM_GOALS)
		{
			level.LTGItems[level.numLTGItems++] = spot;
		}
		else
		{
			break;
		}
	}

	spot = NULL;

	while ((spot = G_FindClass (spot, bg_itemlist[UT_ITEM_REDFLAG].classname)) != NULL)
	{
		if(level.numLTGItems < MAX_BOT_ITEM_GOALS)
		{
			level.LTGItems[level.numLTGItems++] = spot;
		}
		else
		{
			break;
		}
	}

	spot = NULL;

	while ((spot = G_FindClass (spot, bg_itemlist[UT_ITEM_BLUEFLAG].classname)) != NULL)
	{
		if(level.numLTGItems < MAX_BOT_ITEM_GOALS)
		{
			level.LTGItems[level.numLTGItems++] = spot;
		}
		else
		{
			break;
		}
	}

	switch(g_gametype.integer)
	{
		//@Fenix - spawn team based flags also if we are playing JUMP mode
		case GT_JUMP:
			teamgame.redStatus = teamgame.blueStatus = 4; // Invalid to force update
			Team_SetFlagStatus (TEAM_RED, FLAG_ATBASE);
			Team_SetFlagStatus (TEAM_BLUE, FLAG_ATBASE);
			break;

		case GT_CTF:
			teamgame.redStatus = teamgame.blueStatus = 4; // Invalid to force update
			Team_SetFlagStatus (TEAM_RED, FLAG_ATBASE);
			Team_SetFlagStatus (TEAM_BLUE, FLAG_ATBASE);
			break;

		case GT_UT_CAH:
			//@Barbatos
			level.nextScoreTime = level.time + g_cahTime.integer * 1000;
			if(level.nextScoreTime <= 0)
				level.nextScoreTime = 0;

			//level.nextScoreTime = level.time + 1000;
			trap_SetConfigstring (CS_CAPTURE_SCORE_TIME, va ("%i", level.nextScoreTime));
			break;

		case GT_UT_BOMB:
			utBombInitRound ();
			break;

		case GT_UT_ASSASIN:
			{
				vec3_t	   origin;
				vec3_t	   angles;
				gentity_t  *flag;

				// Spawn the red flag
				SelectUTSpawnPoint (TEAM_RED, TEAM_ACTIVE, origin, angles);
				flag		= utFlagSpawn (origin, TEAM_RED);
				flag->touch = RushFlagTouched;
				trap_SetConfigstring (CS_FLAGS + 0, va ("%i %i %f %f %f",
									flag->s.number,
									TEAM_RED,
									flag->s.origin[0],
									flag->s.origin[1],
									flag->s.origin[2]));

				// Spawn the blue flag
				SelectUTSpawnPoint (TEAM_BLUE, TEAM_ACTIVE, origin, angles);
				flag		= utFlagSpawn (origin, TEAM_BLUE);
				flag->touch = RushFlagTouched;
				trap_SetConfigstring (CS_FLAGS + 1, va ("%i %i %f %f %f",
									flag->s.number,
									TEAM_BLUE,
									flag->s.origin[0],
									flag->s.origin[1],
									flag->s.origin[2]));

				break;
			}

		default:
			break;
	}
}

/**
 * OtherTeam
 */
int OtherTeam (team_t team)
{
	if (team == TEAM_RED)
	{
		return TEAM_BLUE;
	}
	else if (team == TEAM_BLUE)
	{
		return TEAM_RED;
	}
	return team;
}

/**
 * ColoredTeamName
 */
const char *ColoredTeamName (team_t team)
{
	if (team == TEAM_RED)
	{
		return S_COLOR_RED "Red";
	}
	else if (team == TEAM_BLUE)
	{
		return S_COLOR_BLUE "Blue";
	}
	else if (team == TEAM_SPECTATOR)
	{
		return "SPECTATOR";
	}
	return "FREE";
}

/**
 * TeamName
 */
const char *TeamName (team_t team)
{
	if (team == TEAM_RED)
	{
		return "RED";
	}
	else if (team == TEAM_BLUE)
	{
		return "BLUE";
	}
	else if (team == TEAM_SPECTATOR)
	{
		return "SPECTATOR";
	}
	return "FREE";
}

/**
 * OtherTeamName
 */
const char *OtherTeamName (team_t team)
{
	if (team == TEAM_RED)
	{
		return "BLUE";
	}
	else if (team == TEAM_BLUE)
	{
		return "RED";
	}
	else if (team == TEAM_SPECTATOR)
	{
		return "SPECTATOR";
	}
	return "FREE";
}

/**
 * TeamColorString
 */
const char *TeamColorString (team_t team)
{
	if (team == TEAM_RED)
	{
		return S_COLOR_RED;
	}
	else if (team == TEAM_BLUE)
	{
		return S_COLOR_BLUE;
	}
	else if (team == TEAM_SPECTATOR)
	{
		return S_COLOR_YELLOW;
	}
	else if (team == TEAM_SPECTATOR)
	{
		return S_COLOR_YELLOW;
	}
	return S_COLOR_WHITE;
}

/**
 * PrintMsg
 */
void QDECL PrintMsg (gentity_t *ent, const char *fmt, ...)
{
	char	 msg[1024];
	va_list  argptr;
	char	 *p;

	va_start (argptr, fmt);

	if (vsprintf (msg, fmt, argptr) > (int)sizeof(msg))
	{
		G_Error ("PrintMsg overrun");
	}
	va_end (argptr);

	// double quotes are bad
	while ((p = strchr (msg, '"')) != NULL)
	{
		*p = '\'';
	}

	// NULL for everyone
	trap_SendServerCommand (((ent == NULL) ? -1 : ent - g_entities), va ("print \"%s\"", msg));
}

/*
==============
AddTeamScore

 used for gametype > GT_TEAM
 for gametype GT_TEAM the level.teamScores is updated in AddScore in g_combat.c
==============
*/
void AddTeamScore (team_t team, int score)
{
	level.teamScores[team] += score;
	//Iain:
	trap_Cvar_Set ("g_teamScores", va ("%d:%d", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
	SendScoreboardGlobalMessageToAllClients ();
}

/*
==============
OnSameTeam
==============
*/

#if 0
//inlined
qboolean OnSameTeam (gentity_t *ent1, gentity_t *ent2)
{
	if (!ent1->client || !ent2->client)
	{
		return qfalse;
	}

	if ((g_gametype.integer < GT_TEAM) || (g_gametype.integer == GT_JUMP))
	{
		return qfalse;
	}

	if (ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam)
	{
		return qtrue;
	}

	return qfalse;
}
#endif
static char  ctfFlagStatusRemap[] = { '0', '1', '*', '*', '2' };
static char  oneFlagStatusRemap[] = { '0', '1', '2', '3', '4' };

/**
 * Team_SetFlagStatus
 */
void Team_SetFlagStatus (team_t team, flagStatus_t status)
{
	qboolean  modified = qfalse;

	switch(team)
	{
		case TEAM_RED:	// CTF

			if(teamgame.redStatus != status)
			{
				if ((teamgame.redStatus == 0) && (status == 1))
				{
					teamgame.redBaseTime = level.time;
				}

				if (status == 0)
				{
					teamgame.redBaseTime = 0;
				}

				teamgame.redStatus = status;
				modified	   = qtrue;
			}
			break;

		case TEAM_BLUE: // CTF

			if(teamgame.blueStatus != status)
			{
				if ((teamgame.blueStatus == 0) && (status == 1))
				{
					teamgame.blueBaseTime = level.time;
				}

				if (status == 0)
				{
					teamgame.blueBaseTime = 0;
				}

				teamgame.blueStatus = status;
				modified		= qtrue;
			}
			break;

		case TEAM_FREE: // One Flag CTF

			if(teamgame.flagStatus != status)
			{
				teamgame.flagStatus = status;
				modified		= qtrue;
			}
			break;
		default:
			Com_Error(ERR_FATAL, "Tried to set flag status on team %d.", team);
			break;
	}

	if(modified)
	{
		char  st[4];

		//@Fenix - remap flags also if we are playing JUMP mode
		//we will not mark the hot potato since the flag is just a plus
		//in jump mode and is not the main goal of the gametype.
		if((g_gametype.integer == GT_CTF) || (g_gametype.integer == GT_JUMP))
		{
			st[0] = ctfFlagStatusRemap[teamgame.redStatus];
			st[1] = ctfFlagStatusRemap[teamgame.blueStatus];
			st[2] = '0';
			st[3] = 0;
		}
		else		// GT_1FCTF
		{
			st[0] = oneFlagStatusRemap[teamgame.flagStatus];
			st[1] = 0;
		}

		//If we're in ctf, mark the hot potato
		if (g_gametype.integer == GT_CTF)
		{
			if ((teamgame.redStatus > 0) && (teamgame.blueStatus > 0) && (g_hotpotato.value > 0))
			{
				int  bt;
				level.hotpotato = 1;
				st[2]		= '1';

				//Find our most recently taken flag, and start hotpotato from there
				if (teamgame.blueBaseTime > teamgame.redBaseTime)
				{
					bt = teamgame.blueBaseTime;
				}
				else
				{
					bt = teamgame.redBaseTime;
				}

				level.hotpotatotime = (bt + (int)(g_hotpotato.value * 60000.0f));

				trap_SetConfigstring (CS_HOTPOTATOTIME, va ("%i", level.hotpotatotime));
			}
			else
			{
				level.hotpotato = 0;
				st[2]		= '0';
			}
		}

		trap_SetConfigstring (CS_FLAGSTATUS, st);
	}
}

/**
 * Team_CheckDroppedItem
 */
void Team_CheckDroppedItem (gentity_t *dropped)
{
	if(dropped->item->giTag == PW_REDFLAG)
	{
		Team_SetFlagStatus (TEAM_RED, FLAG_DROPPED);
	}
	else if(dropped->item->giTag == PW_BLUEFLAG)
	{
		Team_SetFlagStatus (TEAM_BLUE, FLAG_DROPPED);
	}
	else if(dropped->item->giTag == PW_NEUTRALFLAG)
	{
		Team_SetFlagStatus (TEAM_FREE, FLAG_DROPPED);
	}
}

/*
================
Team_ForceGesture
================
*/
void Team_ForceGesture (team_t team)
{
	int    i;
	gentity_t  *ent;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];

		if (!ent->inuse)
		{
			continue;
		}

		if (!ent->client)
		{
			continue;
		}

		if (ent->client->sess.sessionTeam != team)
		{
			continue;
		}

		ent->flags |= FL_FORCE_GESTURE;
	}
}

/*
================
Team_CheckHurtCarrier

Check to see if attacker hurt the flag carrier.  Needed when handing out bonuses for assistance to flag
carrier defense.
================
*/
void Team_CheckHurtCarrier (gentity_t *targ, gentity_t *attacker)
{
	int  flag_pw;

	if (!targ->client || !attacker->client)
	{
		return;
	}

	if (targ->client->sess.sessionTeam == TEAM_RED)
	{
		flag_pw = PW_BLUEFLAG;
	}
	else
	{
		flag_pw = PW_REDFLAG;
	}

	// flags
	if (utPSHasItem (&targ->client->ps, flag_pw) &&
		(targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam))
	{
		attacker->client->pers.teamState.lasthurtcarrier = level.time;
	}
}

/**
 * Team_ResetFlag
 */
gentity_t *Team_ResetFlag (team_t team, int explode)
{
//	  char		 *c;
	gentity_t  *ent, *rent = NULL;
	int classhash;

	switch (team)
	{
		case TEAM_RED:
//			  c = "team_CTF_redflag";
			classhash = HSH_team_CTF_redflag;
			break;

		case TEAM_BLUE:
//			  c = "team_CTF_blueflag";
			classhash = HSH_team_CTF_blueflag;
			break;

		case TEAM_FREE:
//			  c = "team_CTF_neutralflag";
			classhash = HSH_team_CTF_neutralflag;
			break;

		default:
			return NULL;
	}

	ent = NULL;

//	  while ((ent = G_Find (ent, FOFS (classname), c)) != NULL)
	while ((ent = G_FindClassHash (ent, classhash)) != NULL)
	{
		if (ent->flags & FL_DROPPED_ITEM)
		{
			if (explode == 1)
			{
				gentity_t  *grenade;
				//detonate

				grenade 						= G_Spawn ();
				grenade->classname				= "grenade";
				grenade->classhash				= HSH_grenade;
				grenade->nextthink				= level.time;
				grenade->think					= G_ExplodeMissile;
				grenade->s.eType				= ET_MISSILE;
				grenade->r.svFlags				= SVF_USE_CURRENT_ORIGIN;
				grenade->s.weapon				= UT_WP_HK69;
				grenade->r.ownerNum 			= -1;
				grenade->parent 				= NULL;
				grenade->damage 				= 200;
				grenade->splashDamage			= 200;
				grenade->splashRadius			= 300;
				grenade->methodOfDeath			= UT_MOD_FLAG;
				grenade->splashMethodOfDeath	= UT_MOD_FLAG;
				grenade->clipmask				= MASK_SHOT;
				grenade->target_ent 			= NULL;
				grenade->s.pos.trType			= TR_LINEAR;
				VectorCopy (ent->r.currentOrigin, grenade->s.pos.trBase);
				grenade->r.svFlags			   |= SVF_BROADCAST;

				G_ExplodeMissile (grenade);
			}
			G_FreeEntity (ent);
		}
		else
		{
			rent = ent;
			RespawnItem (ent);
		}
	}

	Team_SetFlagStatus (team, FLAG_ATBASE);

	return rent;
}

/**
 * Team_ResetFlags
 */
void Team_ResetFlags (void)
{
	//@Fenix - reset flags status also for JUMP mode
	if((g_gametype.integer == GT_CTF) || (g_gametype.integer == GT_JUMP)) {
		Team_ResetFlag (TEAM_RED, 0);
		Team_ResetFlag (TEAM_BLUE, 0);
	}
}

/**
 * Team_ReturnFlagSound
 */
void Team_ReturnFlagSound (gentity_t *ent, team_t team)
{
	gentity_t  *te;
	vec3_t	   pos = { 0, 0, 0 };

	if (ent != NULL) {
		te = G_TempEntity (ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);
	} else {
		te = G_TempEntity (pos, EV_GLOBAL_TEAM_SOUND);
	}

	if(team == TEAM_BLUE) {
		te->s.eventParm = GTS_RED_RETURN;
	} else {
		te->s.eventParm = GTS_BLUE_RETURN;
	}

	te->r.svFlags |= SVF_BROADCAST;
}

/**
 * Team_TakeFlagSound
 */
void Team_TakeFlagSound (gentity_t *ent, gentity_t *other, team_t team)
{
	gentity_t  *te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_TakeFlagSound\n");
		return;
	}

	// only play sound when the flag was at the base
	// or not picked up the last 10 seconds
	switch(team) {
		case TEAM_RED:

			if(teamgame.blueStatus != FLAG_ATBASE) {
				if (teamgame.blueTakenTime > level.time - 10000) {
					return;
				}
			}
			teamgame.blueTakenTime = level.time;
			break;

		case TEAM_BLUE: // CTF

			if(teamgame.redStatus != FLAG_ATBASE) {
				if (teamgame.redTakenTime > level.time - 10000) {
					return;
				}
			}
			teamgame.redTakenTime = level.time;
			break;
		default:
			Com_Error(ERR_FATAL, "Tried to do take flag sound on team %d.", team);
			break;
	}

	te = G_TempEntity (ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);

	if(team == TEAM_BLUE) {
		te->s.eventParm = GTS_RED_TAKEN;
	} else{
		te->s.eventParm = GTS_BLUE_TAKEN;
	}

	//@Fenix - in Jump mode send the sound only to the
	//client who picked the flag to reduce spamming.
	if (g_gametype.integer == GT_JUMP) {
		te->r.svFlags		|= SVF_SINGLECLIENT;
		te->r.singleClient	 = other->s.number;

	} else {
		te->r.svFlags |= SVF_BROADCAST;
	}

}

/**
 * Team_CaptureFlagSound
 */
void Team_CaptureFlagSound (gentity_t *ent, gentity_t *other, team_t team)
{
	gentity_t  *te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_CaptureFlagSound\n");
		return;
	}

	te = G_TempEntity (ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND);

	if(team == TEAM_BLUE) {
		te->s.eventParm = GTS_BLUE_CAPTURE;
	} else {
		te->s.eventParm = GTS_RED_CAPTURE;
	}

	//@Fenix - in Jump mode send the sound only to the
	//client who capped the flag to reduce spamming.
	if (g_gametype.integer == GT_JUMP) {
		te->r.svFlags		|= SVF_SINGLECLIENT;
		te->r.singleClient	 = other->s.number;

	} else {
		te->r.svFlags |= SVF_BROADCAST;
	}
}

/**
 * Team_ReturnFlag
 */
void Team_ReturnFlag (team_t team)
{

	Team_ReturnFlagSound (Team_ResetFlag (team, 0), team);

	if(team == TEAM_FREE) {
		PrintMsg (NULL, "The flag has returned!\n");
	} else {
		trap_SendServerCommand (-1, va ("ccprint \"%d\" \"%d\"", CCPRINT_FLAG_RETURN, team));
		//trap_SendServerCommand (-1, va ("cp \"The %s ^7flag has returned!\"", ColoredTeamName (team)));
	}
}

/**
 * Team_FreeEntity
 */
void Team_FreeEntity (gentity_t *ent)
{
	if(ent->item->giTag == PW_REDFLAG)
	{
		Team_ReturnFlag (TEAM_RED);
	}
	else if(ent->item->giTag == PW_BLUEFLAG)
	{
		Team_ReturnFlag (TEAM_BLUE);
	}
	else if(ent->item->giTag == PW_NEUTRALFLAG)
	{
		Team_ReturnFlag (TEAM_FREE);
	}
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
==============
*/
void Team_DroppedFlagThink (gentity_t *ent)
{
	int  team = TEAM_FREE;

	if(ent->item->giTag == PW_REDFLAG) {
		team = TEAM_RED;
	} else if(ent->item->giTag == PW_BLUEFLAG) {
		team = TEAM_BLUE;
	} else if(ent->item->giTag == PW_NEUTRALFLAG) {
		team = TEAM_FREE;
	}

	Team_ReturnFlagSound (Team_ResetFlag (team, 0), team);
	// Reset Flag will delete this entity
	trap_SendServerCommand (-1, va ("ccprint \"%d\" \"%d\"", CCPRINT_FLAG_RETURN, team));
	//PrintMsg (NULL, S_COLOR_WHITE "The %s%s flag has returned!\n", ColoredTeamName(team), S_COLOR_WHITE);

	G_LogPrintf ("Flag Return: %s\n", TeamName (team));
}

/*
==============
Team_DroppedFlagThink
==============
*/
int Team_TouchOurFlag (gentity_t *ent, gentity_t *other, team_t team)
{
	int    i;
	gclient_t  *cl = other->client;
	int    itemSlot;
	int    enemy_flag;
	int 	   slot = -1;

	if (cl->sess.sessionTeam == TEAM_RED) {
		enemy_flag = UT_ITEM_BLUEFLAG;
	} else {
		enemy_flag = UT_ITEM_REDFLAG;
	}

	//@Fenix - workaround for JUMP mode.
	if (g_gametype.integer == GT_JUMP) {
		if (team == TEAM_RED) {
			enemy_flag = UT_ITEM_BLUEFLAG;
		} else {
			enemy_flag = UT_ITEM_REDFLAG;
		}
	}

	if (ent->flags & FL_DROPPED_ITEM)
	{
		// hey, its not home.  return it by teleporting it back
		PrintMsg (NULL, "%s" S_COLOR_WHITE " returned the %s%s flag!\n", cl->pers.netname, ColoredTeamName(team), S_COLOR_WHITE);
		G_LogPrintf ("Flag: %i 1: %s\n", other->s.number, ent->classname);

		//Iain
		if(g_gametype.integer != GT_JUMP) {
			//@Fenix - do not add score for return flag in JUMP mode
			AddScore (other, other->r.currentOrigin, CTF_RECOVERY_BONUS);
			Stats_AddScore (other, ST_RETURN_FLAG);
		}
		//~Iain

		other->client->pers.teamState.flagrecovery++;
		other->client->pers.teamState.lastreturnedflag = level.time;
		//ResetFlag will remove this entity!  We must return zero
		Team_ReturnFlagSound (Team_ResetFlag (team, 0), team);
		SendScoreboardSingleMessageToAllClients (other);
		return 0;
	}

	// the flag is at home base.  if the player has the enemy
	// flag, he's just won!
	if (-1 == (itemSlot = utPSGetItemByID (&cl->ps, enemy_flag)))
	{
		return 0; // We don't have the flag
	}

	if(ent->client && ent->client->maparrow)
	{
		ent->client->maparrow->nextthink = level.time;
	}

	//@Barbatos: if this is warmup time don't allow to capture the flag to avoid scoring problems
	//@Fenix - check also correct gametype -> allow cap in JUMP mode
	if ((level.warmupTime > 0) && (g_gametype.integer != GT_JUMP)) {
		return 0;
	}

	//@Fenix - if we are playing Jump mode send the message just to the client
	//who made the flag cap in order to reduce spamming.
	if (g_gametype.integer == GT_JUMP) {
		slot = other->client->ps.clientNum;
	}

	// 'X' captured the 'Color' flag!
	trap_SendServerCommand (slot, va ("ccprint \"%d\" \"%d\" \"%s\" \"%d\"", CCPRINT_FLAG_CAPTURED, other->client->sess.sessionTeam, cl->pers.netname, OtherTeam (team)));

	if(-1 != itemSlot) {
		UT_ITEM_SETID (cl->ps.itemData[itemSlot], 0);
	}

	teamgame.last_flag_capture = level.time;
	teamgame.last_capture_team = team;

	//@Fenix - different scoring mechanism for JUMP mode
	//if (g_gametype.integer == GT_JUMP) {
	//	//We add +1 for each capped flag without giving score to
	//	//the other connected players.
	//	AddScore (other, other->r.currentOrigin, 1);
	//	Stats_AddScore (other, ST_CAPTURE_FLAG);
	//}
	//else {

	if (g_gametype.integer != GT_JUMP) {
		AddTeamScore (other->client->sess.sessionTeam, 1);
		AddScore (other, other->r.currentOrigin, CTF_CAPTURE_BONUS);
		Stats_AddScore (other, ST_CAPTURE_FLAG);

		for (i = 0; i < level.maxclients; i++)
		{
			if((g_entities[i].client->sess.sessionTeam == other->client->sess.sessionTeam) && (g_entities[i].client->ps.clientNum != other->client->ps.clientNum))
			{
				AddScore (&g_entities[i], g_entities[i].r.currentOrigin, CTF_TEAM_BONUS);
			}
		}
	}

	Team_ForceGesture (other->client->sess.sessionTeam);
	other->client->pers.teamState.captures++;
	other->client->rewardTime = level.time + REWARD_SPRITE_TIME;

	Team_CaptureFlagSound (ent, other, team);
	G_LogPrintf ("Flag: %i 2: %s\n", other->s.number, ent->classname);

	Team_ResetFlags ();

	CalculateRanks ();
	SendScoreboardMessageToAllClients ();

	return 0; // Do not respawn this automatically
}

/**
 * Team_TouchEnemyFlag
 */
int Team_TouchEnemyFlag (gentity_t *ent, gentity_t *other, team_t team)
{
	int slot = -1;
	gclient_t  *cl = other->client;

	//@Fenix if we are playing jump mode send the message
	//just to the client who picked the flag
	if (g_gametype.integer == GT_JUMP) {
		slot = other->client->ps.clientNum;
	}

	trap_SendServerCommand (slot, va ("ccprint \"%d\" \"%d\" \"%s\" \"%d\"", CCPRINT_FLAG_TAKEN, other->client->sess.sessionTeam, other->client->pers.netname, team));

	switch (team)
	{
		case TEAM_RED:
			utPSGiveItem (&cl->ps, UT_ITEM_REDFLAG);
			break;

		case TEAM_BLUE:
			utPSGiveItem (&cl->ps, UT_ITEM_BLUEFLAG);
			break;

		default:
			Com_Error (ERR_FATAL, "Unknown team %d.", team);
			break;
	}

	if(other->client && other->client->maparrow) {
		other->client->maparrow->nextthink = level.time ; //we want to think now so the gfx update speedily.
	}
	Team_SetFlagStatus (team, FLAG_TAKEN);

	cl->pers.teamState.flagsince = level.time;
	Team_TakeFlagSound (ent, other, team);

	return -1; // Do not respawn this automatically, but do delete it if it was FL_DROPPED
}

// called from touch_item when the player picks up a team item (IT_TEAM),
// such as a flag
int Pickup_Team (gentity_t *ent, gentity_t *other)
{
	int    team;
	gclient_t  *cl = other->client;

	// figure out what team this flag is
//	  if(strcmp (ent->classname, "team_CTF_redflag") == 0)
	if(ent->classhash==HSH_team_CTF_redflag)
	{
		team = TEAM_RED;
	}
//	  else if(strcmp (ent->classname, "team_CTF_blueflag") == 0)
	else if(ent->classhash==HSH_team_CTF_blueflag)
	{
		team = TEAM_BLUE;
	}
//	  else if(strcmp (ent->classname, "team_CTF_neutralflag") == 0)
	else if(ent->classhash==HSH_team_CTF_neutralflag)
	{
		team = TEAM_FREE;
	}
//	  else if (strcmp (ent->classname, "ut_item_bomb") == 0)
	else if (ent->classhash==HSH_ut_item_bomb)
	{
		team = TEAM_FREE;
	}
	else
	{
		PrintMsg (other, "Don't know what team the flag is on.\n");
		return 0;
	}

	if (g_gametype.integer == GT_UT_ASSASIN)
	{
		return 0;
	}
	else if (g_gametype.integer == GT_UT_CAH)
	{
		return 0;
	}

	//@Fenix - workaround for JUMP mode
	if (g_gametype.integer == GT_JUMP) {
		if (utPSHasItem(&cl->ps, UT_ITEM_BLUEFLAG)) {
			// We are considering the player as a RED one.
			// Allow to cap the BLUE flag but do not pick another RED flag.
			if (team == TEAM_BLUE) return 0;
			else return Team_TouchOurFlag(ent, other, team);
		}
		else if (utPSHasItem (&cl->ps, UT_ITEM_REDFLAG)) {
			// We are considering the player as a BLUE one.
			// Allow to cap the RED flag but do not pick another BLUE flag.
			if (team == TEAM_RED) return 0;
			else return Team_TouchOurFlag(ent, other, team);
		}
		else {
			// The player is not carrying any flags. We can pick one.
			// Since JUMP is not a team based gametype may happen to capture a flag and instantly
			// pick the other one (will be considered as enemy flag). In order to prevent this
			// we add a little delay of 2 seconds between a CAP and future flag PICK.
			if ((level.time - teamgame.last_flag_capture) < 2000) return 0;
			else return Team_TouchEnemyFlag(ent, other, team);
		}
	}

	// GT_CTF
	if(team == (int)cl->sess.sessionTeam) {
		return Team_TouchOurFlag (ent, other, team);
	}

	return Team_TouchEnemyFlag (ent, other, team);
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
gentity_t *Team_GetLocation (gentity_t *ent)
{
	gentity_t  *eloc, *best;
	float	   bestlen, len;
	vec3_t	   origin;

	// Location triggers
	if (ent->triggerLocation && (level.time < ent->triggerLocation->timestamp))
	{
		return ent->triggerLocation;
	}

	best	= NULL;
	bestlen = 3 * 8192.0 * 8192.0;

	VectorCopy (ent->r.currentOrigin, origin);

	for (eloc = level.locationHead; eloc; eloc = eloc->mover->nextTrain)
	{
		len = (origin[0] - eloc->r.currentOrigin[0]) * (origin[0] - eloc->r.currentOrigin[0])
			  + (origin[1] - eloc->r.currentOrigin[1]) * (origin[1] - eloc->r.currentOrigin[1])
			  + (origin[2] - eloc->r.currentOrigin[2]) * (origin[2] - eloc->r.currentOrigin[2]);

		if (len > bestlen)
		{
			continue;
		}

		if (!trap_InPVS (origin, eloc->r.currentOrigin))
		{
			continue;
		}

		bestlen = len;
		best	= eloc;
	}

	return best;
}

/*
===========
Team_GetLocation

Report a location for the player. Uses placed nearby target_location entities
============
*/
qboolean Team_GetLocationMsg (gentity_t *ent, char *loc, int loclen)
{
	gentity_t  *best;

	best = Team_GetLocation (ent);

	if (!best)
	{
		return qfalse;
	}

	if (best->count)
	{
		if (best->count < 0)
		{
			best->count = 0;
		}

		if (best->count > 7)
		{
			best->count = 7;
		}
		Com_sprintf (loc, loclen, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, best->count + '0', best->message);
	}
	else
	{
		Com_sprintf (loc, loclen, "%s", best->message);
	}

	return qtrue;
}

/*---------------------------------------------------------------------------*/

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define MAX_TEAM_SPAWN_POINTS 32

static gentity_t *SelectRandomTeamSpawnPoint (playerTeamStateState_t teamstate, team_t team) {

    int    count;
    int    selection;
    int    classhash;
    gentity_t  *spot;
    gentity_t  *spots[MAX_TEAM_SPAWN_POINTS];
    //char     *classname;

    if (teamstate == TEAM_BEGIN) {

        if (team == TEAM_RED) {
            //classname = "team_CTF_redplayer";
            classhash = HSH_team_CTF_redplayer;
        } else if (team == TEAM_BLUE) {
            //classname = "team_CTF_blueplayer";
            classhash = HSH_team_CTF_blueplayer;
        } else {
            return NULL;
        }

    } else {

        if (team == TEAM_RED) {
            //classname = "team_CTF_redspawn";
            classhash = HSH_team_CTF_redspawn;
        } else if (team == TEAM_BLUE) {
            //classname = "team_CTF_bluespawn";
            classhash = HSH_team_CTF_bluespawn;
        } else {
            return NULL;
        }
    }

    count = 0;
    spot  = NULL;

    //while ((spot = G_Find (spot, FOFS (classname), classname)) != NULL)
    while ((spot = G_FindClassHash (spot, classhash)) != NULL) {

        if (SpotWouldTelefrag (spot)) {
            continue;
        }

        spots[count] = spot;

        if (++count == MAX_TEAM_SPAWN_POINTS) {
            break;
        }

    }

    // no spots that won't telefrag
    if (!count) {
        //return G_Find( NULL, FOFS(classname), classname);
        return NULL;
    }

    selection = rand () % count;
    return spots[selection];
}

/*
===========
SelectCTFSpawnPoint

============
*/
gentity_t *SelectCTFSpawnPoint(team_t team, playerTeamStateState_t teamstate, vec3_t origin, vec3_t angles) {

    gentity_t  *spot;

    spot = SelectRandomTeamSpawnPoint(teamstate, team);

    if(!spot) {
        return SelectSpawnPoint(vec3_origin, origin, angles);
    }

    VectorCopy (spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy (spot->s.angles, angles);
    return spot;
}

/**
 * $(function)
 */
qboolean G_InitUTSpawnPoints (int gametype) // Fixed: DensitY - Also changed indenting since it pisses you guys off
{
	qboolean  res;

	level.spawnsfrommode = gametype;

	switch(gametype)
	{
		case GT_UT_SURVIVOR:
			return G_InitUTSpawnPointsTeamSurvivor (GT_UT_SURVIVOR, qtrue);

			break;

		case GT_UT_ASSASIN:
			return G_InitUTSpawnPointsTeamSurvivor (GT_UT_ASSASIN, qtrue);

			break;

		case GT_UT_CAH:
			{
				res = G_InitUTSpawnPointsTeamSurvivor (GT_UT_CAH, qfalse);

				if (TSblueGroup == -1)
				{
					TSblueGroup = TSredGroup;
				}
				return res;

				break;
			}

		case GT_UT_BOMB:

			if(level.BombMapAble == qtrue)
			{
				return G_InitUTSpawnPointsTeamSurvivor (GT_UT_BOMB, qfalse);
			}
			else // if we don't actually have bombsites then we don't offically have Bomb spawns as well
			{
				return G_InitUTSpawnPointsTeamSurvivor (GT_UT_SURVIVOR, qtrue);
			}
			break;

		default:

			if(g_survivor.integer)
			{
				return G_InitUTSpawnPointsTeamSurvivor (gametype, qtrue);
			}
			else
			{
				return G_InitUTSpawnPointsTeamSurvivor (gametype, qfalse);
			}
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : G_InitUTSpawnPointsTeamSurvivor
// Description: Intializes urban terror spawn points
// Author	  : Apoxol
// Fixed By   : 27 - basically gave it a total rewrite
// More Fixes : DensitY - Smarter Distance checking now
//////////////////////////////////////////////////////////////////////////////////////////

qboolean G_InitUTSpawnPointsTeamSurvivor (int gametype, qboolean allowswaps)
{
	gentity_t  *spot	   = NULL;
	int    i;
	float	   oldDistance = 0.0f;

	//We only need to do this on a map_restart.
	if(TSnumGroups == 0)
	{
		// Build the list of groups
		while ((spot = G_FindClassHash (spot, HSH_info_ut_spawn)) != NULL)
		{
			// No spawn group no spawn there, thats not good
			if(!spot->spawn)
			{
				G_FreeEntity (spot);
				continue;
			}

			// Skip the spawn if its not used by this gametyp
			if(!(spot->spawn->gametypes & (1 << gametype)))
			{
				continue;
			}

			// See if its already in the list, we dont need it twice
			for(i = 0; i < TSnumGroups; i++)
			{
				if(!Q_stricmp (spot->spawn->group, Groups[i]->spawn->group) && (spot->spawn->team == Groups[i]->spawn->team))
				{
					break;
				}
			}

			// If we found a new group then add it
			if(i >= TSnumGroups)
			{
				Groups[TSnumGroups++] = spot;
			}
		}

		// If we dont have at least 2 groups then we are hosed
		if(gametype == GT_FFA)
		{
			return qtrue;
		}

		if(TSnumGroups < 2)
		{
			if(g_gametype.integer == gametype)
			{
				G_Printf (S_COLOR_YELLOW  "Warning: Not enough spawn groups to for gametype (%i on this map, trying another gametype)\n", g_gametype.integer);
			}
			return qfalse;
		}
	}
	TSredGroup	= -1;
	TSblueGroup = -1;
	// Find ANY Spawn group for red  (can be a blue one..)
	TSredGroup	= (rand () % TSnumGroups);

	for(i = 0; i < TSnumGroups; i++)
	{
		float	newDistance = 0.0f;
		vec3_t	Avg1, Avg2;
		int Avg1count	= 0, Avg2count = 0;

		// Make Sure its clear
		Avg1[0] = Avg1[1] = Avg1[2] = 0.0f;
		Avg2[0] = Avg2[1] = Avg2[2] = 0.0f;

		// loop through all spawns in that group
		if(i == TSredGroup)
		{
			continue;
		}

		if((allowswaps != qtrue) && (Groups[i]->spawn->team == Groups[TSredGroup]->spawn->team))
		{
			continue;
		}
		spot = NULL;

		while((spot = G_FindClassHash (spot, HSH_info_ut_spawn)) != NULL)
		{
			if(!spot->spawn)
			{
				continue;
			}

			if(!Q_stricmp (spot->spawn->group, Groups[TSredGroup]->spawn->group))
			{
				Avg1[0] += spot->s.origin[0];
				Avg1[1] += spot->s.origin[1];
				Avg1[2] += spot->s.origin[2];
				Avg1count++;
			}
			else if(!Q_stricmp (spot->spawn->group, Groups[i]->spawn->group))
			{
				Avg2[0] += spot->s.origin[0];
				Avg2[1] += spot->s.origin[1];
				Avg2[2] += spot->s.origin[2];
				Avg2count++;
			}
		}

		// the counts should ALWAYS be greater or equal to 1
		if(Avg1count > 0)
		{
			Avg1[0] = Avg1[0] / Avg1count;
			Avg1[1] = Avg1[1] / Avg1count;
			Avg1[2] = Avg1[2] / Avg1count;
		}

		if(Avg2count > 0)
		{
			Avg2[0] = Avg2[0] / Avg2count;
			Avg2[1] = Avg2[1] / Avg2count;
			Avg2[2] = Avg2[2] / Avg2count;
		}
		newDistance = Distance (Avg1, Avg2);

		if(newDistance > oldDistance)
		{
			oldDistance = newDistance;
			TSblueGroup = i;
		}
	}

	// Check in case of this, else game will crash!
	if(TSblueGroup == -1) // ok this could actually pick a red team but you get the idea

	{
		G_Printf ("G_InitUTSpawnPointsTeamSurvivor: No blue groups found!");
		return qfalse;
	}

	if (allowswaps == qtrue)
	{
		TSswapped = qfalse;

		if (rand () % 100 >= 50)
		{
			int  swap;
			swap		= TSredGroup;
			TSredGroup	= TSblueGroup;
			TSblueGroup = swap;
			TSswapped	= qtrue;
		}
	}
	else
	{
		//Check to see if we're the wrong way around, and if so, fix it
		if (Groups[TSblueGroup]->spawn->team == TEAM_RED) //oops, swap
		{
			int  swap;
			swap		= TSredGroup;
			TSredGroup	= TSblueGroup;
			TSblueGroup = swap;
			TSswapped	= qtrue;
		}
	}
	return qtrue;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name          : SelectUTSpawnPoint
// Description   : Selects a random Urban terror spawn point
// Author        : GottaBeKD
// Changes/Fixes : DensitY, Fenix
///////////////////////////////////////////////////////////////////////////////////////////
gentity_t *SelectUTSpawnPoint (team_t team, playerTeamStateState_t teamstate, vec3_t origin, vec3_t angles) {

    int        count  = 0;
    int        picked = 0;
    int        thegametype;
    int        j, ii;
    gentity_t  *spots[MAX_TEAM_SPAWN_POINTS];
    gentity_t  *spot  = NULL;

    for (j = 0; j < MAX_TEAM_SPAWN_POINTS; j++) {
        spots[j] = NULL;
    }

    //thegametype = g_gametype.integer;
    thegametype = level.spawnsfrommode;

    if ((thegametype == GT_UT_BOMB) && !level.BombMapAble) {
        thegametype = GT_UT_SURVIVOR;
    }

    while ((spot = G_FindClassHash(spot, HSH_info_ut_spawn)) != NULL) {

        // DensitY 2.6b: Added Check which game mode we are in
        if (!spot->spawn) {
            G_FreeEntity(spot);
            continue;
        }

        if(!(spot->spawn->gametypes & (1 << thegametype))) {
            continue;
        }

        // Make sure this spot wont telefrag
        if (SpotWouldTelefrag(spot)) {
            continue;
        }

        //if( team == spot->spawn->team ) {
        //    spots[ count++ ] = spot;
        //}
        //else {
        // Small Change for Bomb Spawning

        if (thegametype == GT_UT_BOMB) { //wont be in here for old TS only maps

            if (team != level.AttackingTeam) {

                if (spot->spawn->team == Groups[TSblueGroup]->spawn->team) {

                    ii = Q_stricmp (spot->spawn->group, Groups[TSblueGroup]->spawn->group);

                    if (ii == 0) {
                        spots[count++] = spot;
                    }
                }
            }

            if (team == level.AttackingTeam) {

                if (spot->spawn->team == Groups[TSredGroup]->spawn->team) {

                    ii = Q_stricmp (spot->spawn->group, Groups[TSredGroup]->spawn->group);

                    if (ii == 0) {
                        spots[count++] = spot;
                    }
                }
            }
        }
        else {

            if (team == TEAM_BLUE) {

                //Weird logic. Some spawn groups are mixed up
                if (spot->spawn->team == Groups[TSblueGroup]->spawn->team) {

                    ii = Q_stricmp (spot->spawn->group, Groups[TSblueGroup]->spawn->group);

                    if (ii == 0) {
                        spots[count++] = spot;
                    }
                }
            }

            if (team == TEAM_RED) {

                //Weird logic. Some spawn groups are mixed up
                if (spot->spawn->team == Groups[TSredGroup]->spawn->team) {

                    ii = Q_stricmp (spot->spawn->group, Groups[TSredGroup]->spawn->group);

                    if (ii == 0) {
                        spots[count++] = spot;
                    }
                }
            }
        }

        // if (team == spot->spawn->team) {
        //      Make sure the group for this spawn is the same as selected for this team
        //      if (Q_stricmp (spot->spawn->group, Groups[team]->spawn->group)) {
        //          // Add the spot to the list
        //          spots[count++] = spot;
        //      }
        // }

        // Cant have too many
        if (count == MAX_TEAM_SPAWN_POINTS) {
            break;
        }

    }

    // If we didnt find any
    // then move on to CTF spawns
    if (!count) {
        return SelectCTFSpawnPoint(team, teamstate, origin, angles);
    }

    // pick a spot and spawn there
    picked = rand () % count;
    spot   = spots[picked];

    // Set the player location and angles
    VectorCopy (spot->s.origin, origin);
    origin[2] += 9;
    VectorCopy (spot->s.angles, angles);

    return spot;
}

/*---------------------------------------------------------------------------*/

static int QDECL SortClients (const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

/*
==================
TeamplayLocationsMessage

Format:
clientNum location health armor weapon powerups

==================
*/
void TeamplayInfoMessage (gentity_t *ent)
{
	char	   entry[1024];
	char	   string[8192];
	int    stringlength;
	int    i, j;
	gentity_t  *player;
	int    cnt;
	int    h, a;
	int    clients[TEAM_MAXOVERLAY];

	if (!ent->client->pers.teamInfo)
	{
		return;
	}

	// figure out what client should be on the display
	// we are limited to 8, but we want to use the top eight players
	// but in client order (so they don't keep changing position on the overlay)
	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++)
	{
		player = g_entities + level.sortedClients[i];

		if (player->inuse && (player->client->sess.sessionTeam ==
					  ent->client->sess.sessionTeam))
		{
			clients[cnt++] = level.sortedClients[i];
		}
	}

	// We have the top eight players, sort them by clientNum
	qsort (clients, cnt, sizeof(clients[0]), SortClients);

	// send the latest information on all clients
	string[0]	 = 0;
	stringlength = 0;

	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++)
	{
		player = g_entities + i;

		if (player->inuse && (player->client->sess.sessionTeam ==
					  ent->client->sess.sessionTeam))
		{
			h = player->client->ps.stats[STAT_HEALTH];
			a = 0;

			if (h < 0)
			{
				h = 0;
			}

			if (a < 0)
			{
				a = 0;
			}

			Com_sprintf (entry, sizeof(entry),
					 " %i %i %i %i %i %i",
//				level.sortedClients[i], player->client->pers.teamState.location, h, a,
					 i, player->client->pers.teamState.location, h, a,
					 utPSGetWeaponID (&player->client->ps), player->s.powerups);
			j = strlen (entry);

			if (stringlength + j > sizeof(string))
			{
				break;
			}
			strcpy (string + stringlength, entry);
			stringlength += j;
			cnt++;
		}
	}

	trap_SendServerCommand (ent - g_entities, va ("tinfo %i %s", cnt, string));
}

/**
 * CapturePointTouch
 */
static void CapturePointTouch (gentity_t *self, gentity_t *other, trace_t *trace)
{
	gentity_t  *te;
	gentity_t  *loc;

	// No client, no capture
	if(!other->client)
	{
		return;
	}

	// See if this team already owns the capture point
	if(self->s.modelindex == (int)other->client->sess.sessionTeam)
	{
		return;
	}

	te			 = G_TempEntity (self->s.pos.trBase, EV_UT_CAPTURE);
	te->s.otherEntityNum = other->s.clientNum;

	switch (self->s.modelindex)
	{
		case TEAM_BLUE:
			level.blueCapturePointsHeld--;
			break;

		case TEAM_RED:
			level.redCapturePointsHeld--;
			break;
	}

	// Change teams
	self->s.modelindex = other->client->sess.sessionTeam;

	switch (self->s.modelindex)
	{
		case TEAM_BLUE:
			level.blueCapturePointsHeld++;
			break;

		case TEAM_RED:
			level.redCapturePointsHeld++;
			break;
	}

	// Set the location of capture
	loc 		  = Team_GetLocation (other);
	te->s.otherEntityNum2 = loc ? loc->health : 0;
	te->s.time2 	  = self->s.number;
	te->s.time		  = self->s.modelindex;
	te->r.svFlags		 |= SVF_BROADCAST;

	trap_SetConfigstring (CS_FLAGS + self->s.time2,
				  va ("%i %i %f %f %f",
				  self->s.number,
				  self->s.modelindex,
				  self->s.origin[0],
				  self->s.origin[1],
				  self->s.origin[2]));
}

/**
 * SP_team_CAH_capturepoint
 */
void SP_team_CAH_capturepoint (gentity_t *ent)
{
	gentity_t  *flag;

	if(g_gametype.integer != GT_UT_CAH)
	{
		G_FreeEntity (ent);
		return;
	}

	flag = utFlagSpawn (ent->s.origin, TEAM_FREE);

	if (!flag)
	{
		G_FreeEntity (ent);
		return;
	}
	flag->touch   = CapturePointTouch;
	flag->s.time2 = level.totalCapturePoints;

	trap_SetConfigstring (CS_FLAGS + level.totalCapturePoints,
				  va ("%i %i %f %f %f",
				  flag->s.number,
				  TEAM_FREE,
				  flag->s.origin[0],
				  flag->s.origin[1],
				  flag->s.origin[2]));

	level.totalCapturePoints++;

	G_FreeEntity (ent);
}

///////////////////////////////////////////////////////////////////////////////////////////
// Name 		: utBombRespawn
// Description	: Spawns the bomb in the Search and destroy game type
// By			: Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
void utBombRespawn (void)
{
	gentity_t  *spawnents[20];

	gentity_t  *ent   = NULL;
	gentity_t  *bomb  = NULL;
	int    spawns = 0;
	vec3_t	   up	  = { 0, 0, 16};

	// Find all the bomb spawns
	while ((ent = G_FindClassHash (ent, HSH_team_BTB_bombspawn)) != NULL)
	{
		spawnents[spawns++] = ent;
	}

	// If no bomb spawns were found then freak out
	if(!spawns)
	{
		G_Error ("Map has no bomb spawn points for the Bomb the base gametype");
		return;
	}

	// Pick a random spawn.
	ent = spawnents[rand () % spawns];

	// Now spawn the bomb
	bomb = G_Spawn ();

	// Set its origin and move it up a bit so it doenst spawn in the floor
	VectorCopy (ent->r.currentOrigin, bomb->s.origin);
	VectorAdd (up, bomb->s.origin, bomb->s.origin);

	// Set the class name from the item.
	bomb->classname = bg_itemlist[UT_ITEM_C4].classname;
	HASHSTR(bomb->classname,bomb->classhash);

	// Spawn it and link it
	G_SpawnItem (bomb, &bg_itemlist[UT_ITEM_C4]);
	FinishSpawningItem (bomb);
	trap_LinkEntity (bomb);
}

///////////////////////////////////////////////////////////////////////////////////////////
// Name 		: utBombThink
// Description	: Called once a second by an active bomb in GT_UT_BTB
// By			: Dokta8
///////////////////////////////////////////////////////////////////////////////////////////
void utBombThink (gentity_t *ent)
{
	ent->health--;

	// If the health reaches zero then blow shit up
	if (ent->health <= 0)
	{
		if (ent->enemy)
		{
			gentity_t  *te;

			teamgame.last_flag_capture = level.time;
			teamgame.last_capture_team = ent->breakaxis;
			//level.survivorCaptureTeam = ent->breakaxis;

			ent->enemy->client->pers.teamState.captures++;
			AddTeamScore (ent->breakaxis, 1);

			te		   = G_TempEntity (ent->s.pos.trBase, EV_UT_BOMB_BASE);
			te->r.svFlags |= SVF_BROADCAST;
			te->s.time2    = ent->enemy->client->ps.clientNum;
		}

		ent->nextthink = 0;
		ent->think	   = 0;

		G_FreeEntity (ent);
		return;
	}

	// Send the tiem to the client.
	ent->s.time2   = 100 * ent->health / g_bombExplodeTime.integer;
	ent->nextthink = level.time + 1000;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Name 		: utFlagSpawn
// Description	: Spawns a flag at the given origin
// Author		: Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
gentity_t *utFlagSpawn (vec3_t origin, team_t team)
{
	trace_t    tr;
	vec3_t	   dest;
	gentity_t  *flag = G_Spawn ();

	VectorCopy (origin, flag->s.origin);

	VectorSet (flag->r.mins, -12, -12, -12);
	VectorSet (flag->r.maxs, 12, 12, 12);

	VectorSet (dest, flag->s.origin[0], flag->s.origin[1], flag->s.origin[2] - 4096);
	trap_Trace (&tr, flag->s.origin, flag->r.mins, flag->r.maxs, dest, flag->s.number, MASK_SOLID);

	if (tr.startsolid)
	{
		G_FreeEntity (flag);
		return NULL;
	}

	flag->s.eType	   = ET_TEAM;
	flag->s.modelindex = team;
	flag->r.contents   = CONTENTS_TRIGGER;
	flag->r.svFlags   |= SVF_BROADCAST;

	switch (team)
	{
		default:
			flag->classname = bg_itemlist[UT_ITEM_NEUTRALFLAG].classname;
			flag->classhash = HSH_team_CTF_neutralflag;
			break;

		case TEAM_BLUE:
			flag->classname = bg_itemlist[UT_ITEM_BLUEFLAG].classname;
			flag->classhash = HSH_team_CTF_blueflag;
			break;

		case TEAM_RED:
			flag->classname = bg_itemlist[UT_ITEM_REDFLAG].classname;
			flag->classhash = HSH_team_CTF_redflag;
			break;
	}
//	  HASHSTR(flag->classname,flag->classhash);
	flag->s.groundEntityNum = tr.entityNum;
	G_SetOrigin (flag, tr.endpos);

	trap_LinkEntity (flag);

	return flag;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Name 		: GetDistanceToFlag
// Description	: Returns the distance to the flag if it is in the base
// Author		: Iain && Dok8
///////////////////////////////////////////////////////////////////////////////////////////
float GetDistanceToFlag (vec3_t origin, team_t team)
{
//	  char		 *classname = NULL;
	int 		 classhash = 0;
	gentity_t	 *ent		= NULL;
	vec3_t		 nOrigin;

	switch (team)
	{
		default:
			{
//				  classname = bg_itemlist[UT_ITEM_NEUTRALFLAG].classname;
				classhash = HSH_team_CTF_neutralflag;
				if(teamgame.redStatus != FLAG_ATBASE)
				{
					return -1.0f;
				}
				break;
			}

		case TEAM_RED:
			{
//				  classname = bg_itemlist[UT_ITEM_REDFLAG].classname;
				classhash = HSH_team_CTF_redflag;
				if(teamgame.redStatus != FLAG_ATBASE)
				{
					return -1.0f;
				}
				break;
			}

		case TEAM_BLUE:
			{
//				  classname = bg_itemlist[UT_ITEM_BLUEFLAG].classname;
				classhash = HSH_team_CTF_blueflag;
				if(teamgame.redStatus != FLAG_ATBASE)
				{
					return -1.0f;
				}
				break;
			}
	}

	ent = G_FindClassHash (ent, classhash);

	// D8: Iain - don't assume the G_Find will work... if it returns
	// a NULL, this crashes
	if (NULL != ent)
	{
		VectorSubtract (origin, ent->s.origin, nOrigin);
		return VectorLength (nOrigin);		  // also, this returns a float, not an int!!
	}
	else
	{
		// G_Find: no flags on this map
		return -1.0f;
	}
}

/**
 * GetDistanceToBombCarrier
 */
float GetDistanceToBombCarrier (vec3_t origin)
{
	int    i;
	gclient_t  *client;
	vec3_t	   nOrigin;

	for(i = 0; i < level.maxclients; i++)
	{
		// Get client pointer.
		client = &level.clients[i];

		// If not connected.
		if (client->pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		// If this is the bomb holder.
		if (client->sess.bombHolder == qtrue)
		{
			VectorSubtract (origin, client->ps.origin, nOrigin);
			return VectorLength (nOrigin);
		}
	}
	return -1;
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
// Bomb Related Team Code
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

/*=---- Bombsite Spawn function ----=*/

void SP_info_ut_bombsite (gentity_t *ent)
{
	if(g_gametype.integer != GT_UT_BOMB)
	{
		G_FreeEntity (ent);
		return;
	}
	ent->r.svFlags |= SVF_BROADCAST;
	ent->s.eType	= ET_BOMBSITE;		// Bomb Entity type
	ent->think	= 0;

	// Get set range, if nothing set, default to 128 units
	G_SpawnInt ("range", "128", &ent->s.frame);
//	ent->s.frame = ent->BombRange; // hack for clients
	trap_LinkEntity (ent);
	level.NumBombSites++;			// increase site counter
}

/*=---- Returns ClientNum for redteam bomb holder ----=*/

int PickBombHolderClientNum (int Seed)
{
	int    i, k;
	gclient_t  *cl;

	for(i = 0, k = 0; i < level.maxclients; i++)
	{
		cl = &level.clients[i];

		if(cl->pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		if(cl->sess.sessionTeam == level.AttackingTeam)
		{
			if(k == Seed)
			{
				return i;
			}
			k++;
		}
	}
	return -1;
}

/*=---- Is the Team all fulled with bots ---=*/

qboolean IsTeamAllBots (team_t team)
{
	int    i;
	gclient_t  *cl;
	qboolean   ReturnValue = qtrue;

	for(i = 0; i < level.maxclients; i++)
	{
		cl = &level.clients[i];

		if((cl->sess.sessionTeam == team) && (cl->pers.connected != CON_DISCONNECTED) && !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT))
		{
			ReturnValue = qfalse;
			break;
		}
	}
	return ReturnValue;
}

/**
 * utBombInitRound
 */
void utBombInitRound (void)
{
	gclient_t  *client;
	int    attackClients[MAX_CLIENTS];
	int    numAttackClients;
	int    i, j;

	// If not enough bomb sites.
	if (level.NumBombSites != 2)
	{
		// Turn off bomb rules.
		level.BombMapAble = qfalse;

		return;
	}
	else
	{
		// Using bomb rules.
		level.BombMapAble = qtrue;
	}

	// Reset stuff.
	level.BombGoneOff = qfalse;
	level.BombDefused = qfalse;
	level.BombPlanted = qfalse;

	// Loop through clients.
	for (i = 0, numAttackClients = 0; i < level.maxclients; i++)
	{
		// Get client pointer.
		client = &level.clients[i];

		// Clear bomb holder flag for everyone.
		client->sess.bombHolder = qfalse;

		// If not connected.
		if (client->pers.connected != CON_CONNECTED)
		{
			continue;
		}

		// If not on the attacking team.
		if (client->sess.sessionTeam != level.AttackingTeam)
		{
			continue;
		}

		// If a substitute.
		if (client->pers.substitute)
		{
			continue;
		}

		// Add client to list.
		attackClients[numAttackClients++] = i;
	}

	// If there are no attack clients.
	if (numAttackClients < 1)
	{
		return;
	}

	// Select bomb holder client.
	j = attackClients[rand () % numAttackClients];

	// Log who got the bomb.
	G_LogPrintf ("Bombholder is %i\n", j);

	// Loop through clients.
	for (i = 0; i < level.maxclients; i++)
	{
		// Get client pointer.
		client = &level.clients[i];

		// If not connected.
		if (client->pers.connected == CON_DISCONNECTED)
		{
			continue;
		}

		// If this is the bomb holder.
		if (i == j)
		{
			// Set bomb holder flag.
			client->sess.bombHolder = qtrue;

			// Notify the client.
			trap_SendServerCommand (i, va ("cp \"You have the %sBOMB\"", level.AttackingTeam == TEAM_RED ? S_COLOR_RED : S_COLOR_BLUE));
		}
		// Or this client is on the bomb holder's team.
		else if (client->sess.sessionTeam == level.AttackingTeam)
		{
			// Notify this client.
			trap_SendServerCommand (i, va ("cp \"%s has the %sBOMB\"", level.clients[j].pers.netname, level.AttackingTeam == TEAM_RED ? S_COLOR_RED : S_COLOR_BLUE));
		}
	}
}

//--------------------------------------------------------------------------------
// Bomb Mode think functions
//--------------------------------------------------------------------------------

/*
	All Thinking code todo with the bomb entity, planting, thinking before explostion
	and explostion
*/

/*=---- Bomb Planting code ----=*/

void utBombThink_Plant (gentity_t *ent) 	 // called when bomb is planted
{
	gentity_t  *Bomb, *player;
	vec3_t	   forward;
	vec3_t	   right;
	vec3_t	   up;
	vec3_t	   muzzle;
	vec3_t	   End, Start;
	trace_t    tr;
	int    contents;

	player = &g_entities[ent->parent->s.clientNum];

	// If an invalid entity.
	if(!player->client)
	{
		G_Error ("utBombThink_Plant: No Client found!");
	}

	// If player is dead.
	if(player->client->ps.stats[STAT_HEALTH] <= 0)
	{
		// we are dead, plant is canceled
		level.BombPlanted				  = qfalse;
		player->client->ps.stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_PLANTING);
		player->client->ps.eFlags			 &= ~EF_UT_PLANTING;
		// Destory this entity
		ent->nextthink					  = level.time + 50;
		ent->think					  = G_FreeEntity;
		return;
	}

	// If bomb shouldn't be planted.
	if (!level.BombPlanted)
	{
		level.BombPlanted				  = qfalse;
		player->client->ps.stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_PLANTING);
		player->client->ps.eFlags			 &= ~EF_UT_PLANTING;
		// Destory this entity
		ent->nextthink					  = level.time + 50;
		ent->think					  = G_FreeEntity;
		return;
	}

	// Plant our bomb
	forward[2] = 0.2f;
	AngleVectors (player->client->ps.viewangles, forward, right, up);
	CalcEyePoint (player, muzzle);
	Bomb			  = G_Spawn ();
	Bomb->timestamp 	  = level.time + 50;	 // Hack: use as starttime
	Bomb->classname 	  = "ut_planted_bomb";	 // Planted bomb entity name
	Bomb->classhash 	  = HSH_ut_planted_bomb;
	Bomb->nextthink 	  = level.time + 50;	 // 50MS delay
	Bomb->think 	  = utBombThink_Think;
	Bomb->s.eType		  = ET_MISSILE;
	//Bomb->s.eType = ET_CORPSE;
	Bomb->s.weapon		  = UT_WP_BOMB;
	Bomb->r.ownerNum	  = player->s.number;
	Bomb->s.time2		  = -1;
	Bomb->parent		  = player;
	Bomb->damage		  = (int)((float)UT_MAX_HEALTH / 2 *
		bg_weaponlist[player->s.weapon].damage[HL_UNKNOWN].value);
	Bomb->splashDamage	  = (int)((float)UT_MAX_HEALTH * 2 *
		bg_weaponlist[player->s.weapon].damage[HL_UNKNOWN].value);
	Bomb->splashRadius	  = 750;
	Bomb->methodOfDeath   = bg_weaponlist[player->s.weapon].mod;
	Bomb->splashMethodOfDeath = bg_weaponlist[player->s.weapon].mod;
	Bomb->clipmask		  = MASK_SHOT;	 // hence why you can't pick it up
	Bomb->target_ent	  = NULL;
	Bomb->s.pos.trType	  = TR_STATIONARY;
	Bomb->s.pos.trTime	  = level.time;
	Bomb->s.eFlags		  = (EF_NOPICKUP | EF_BOUNCE_HALF);
	Bomb->r.mins[0] 	  = -5;
	Bomb->r.mins[1] 	  = -5;
	Bomb->r.mins[2] 	  = -5;
	Bomb->r.maxs[0] 	  = 5;
	Bomb->r.maxs[1] 	  = 5;
	Bomb->r.maxs[2] 	  = 5;

	// New Planting Code
	// We just fire a trace from the player origin directly down and slap the bomb there
	// really simple actually - DensitY :)
	VectorCopy (level.BombPlantOrigin, Start);
	Start[2] += 20;
	End[0]	  = Start[0];
	End[1]	  = Start[1];
	End[2]	  = Start[2] - 100;
	trap_Trace (&tr, Start, NULL, NULL, End, player->client->ps.clientNum, CONTENTS_SOLID);    // player->r.currentOrigin
	// Check for Water drop
	contents = trap_PointContents (tr.endpos, Bomb->s.number);

	if(contents & CONTENTS_WATER)
	{
		G_FreeEntity (Bomb);
		level.BombPlanted				  = qfalse;
		trap_SendServerCommand (player->s.number, va ("cp \"The ^%dBOMB^7 can't be planted in water!\"", level.AttackingTeam == TEAM_RED ? 1 : 4));
		player->client->ps.stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_PLANTING);
		player->client->ps.eFlags			 &= ~EF_UT_PLANTING;
		return;
	}
	tr.endpos[2] += 2;
	VectorCopy (tr.endpos, Bomb->s.pos.trBase);
	VectorCopy (tr.endpos, Bomb->r.currentOrigin);

	Bomb->r.svFlags |= SVF_BROADCAST;	// Send to all clients
	// link bomb for collision
	trap_LinkEntity (Bomb);
	// ok tell everyone that the bomb has been planted!
	// TODO: move to a clientside event
	trap_SendServerCommand (-1, va ("cp \"The ^%dBOMB^7 has been planted by ^%d%s" S_COLOR_WHITE "^7!\"", level.AttackingTeam == TEAM_RED ? 1 : 4, level.AttackingTeam == TEAM_RED ? 1 : 4, player->client->pers.netname));
	level.BombHolderLastRound = player->s.clientNum;
	utPSSetWeaponBullets (&player->client->ps, 0);

	if(bg_weaponlist[player->s.weapon].flags & UT_WPFLAG_REMOVEONEMPTY)
	{
		if(utPSGetWeaponBullets (&player->client->ps) + utPSGetWeaponClips (&player->client->ps) == 0)
		{
			//NextWeapon(&player->client->ps);
			utPSSetWeaponID (&player->client->ps, 0);
		}
	}
	// Destory this entity
	ent->nextthink					  = level.time + 50;
	ent->think					  = G_FreeEntity;
	player->client->ps.stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_PLANTING); // player has stopped planting!
	player->client->ps.eFlags			 &= ~EF_UT_PLANTING;
	G_LogPrintf ("Bomb was planted by %i\n", player->client->ps.clientNum);
}

/*=---- Standard Think PRocess ----=*/

void utBombThink_Think (gentity_t *ent)
{
	int    TimeRemaining;
	int    SoundIndex = 0;
	gentity_t  *SoundEnt;		  // Beep sound :)

	if(level.BombDefused == qtrue)
	{
		return;
	}

	// if x seconds has pasted then blow up
	if(level.time - ent->timestamp >= g_bombExplodeTime.integer * 1000)
	{
		ent->think	   = utBombThink_Explode;
		ent->nextthink = level.time + 50;
		return;
	}

	// Work out remaining time
	// Work out Which sound we should play
	if(((level.time - ent->timestamp) / 1000) < g_bombExplodeTime.integer / 2)
	{
		SoundIndex = 0;
	}
	else if(((level.time - ent->timestamp) / 1000) >= g_bombExplodeTime.integer - 5)
	{
		SoundIndex = 2;
	}
	else if(((level.time - ent->timestamp) / 1000) >= g_bombExplodeTime.integer / 2)
	{
		SoundIndex = 1;
	}
	TimeRemaining		  = g_bombExplodeTime.integer + - ((level.time - ent->timestamp) / 1000);
	// Beep sound :D
	SoundEnt		  = G_TempEntity (ent->r.currentOrigin, EV_UT_BOMBBEEP);
	VectorCopy (ent->r.currentOrigin, SoundEnt->s.pos.trBase);
	SoundEnt->s.eventParm = SoundIndex;
	SoundEnt->r.svFlags  |= SVF_BROADCAST;

	if(TimeRemaining <= 3)		   // print out on last 3 seconds
	{
		if (TimeRemaining == 1)
		{
			trap_SendServerCommand (-1, va ("cp \"^%dBOMB^7 will blow up in %i second\"", level.AttackingTeam == TEAM_RED ? 1 : 4, TimeRemaining));
		}
		else
		{
			trap_SendServerCommand (-1, va ("cp \"^%dBOMB^7 will blow up in %i seconds\"", level.AttackingTeam == TEAM_RED ? 1 : 4, TimeRemaining));
		}
	}
	ent->nextthink = level.time + 1000; 	// think here next second
}

/*=---- bomb thinking code for exploding ----=*/

void utBombThink_Explode (gentity_t *ent)	   // called when bomb is about to explode
{
	vec3_t	dir;
	vec3_t	origin;

	BG_EvaluateTrajectory (&ent->s.pos, level.time, origin);
	SnapVector (origin);
	G_SetOrigin (ent, origin);
	// we don't have a valid direction, so just point straight up
	dir[0]				 = dir[1] = 0;
	dir[2]				 = 1;
	ent->s.eType			 = ET_GENERAL;
	ent->r.svFlags			|= SVF_BROADCAST;
	G_AddEvent (ent, EV_MISSILE_MISS, DirToByte (dir));
	ent->freeAfterEvent 	 = qtrue;
	trap_LinkEntity (ent);
	level.BombPlanted		 = qfalse;	 // No longer planted but it has gone off :)
	level.BombGoneOff		 = qtrue;
	VectorCopy (ent->r.currentOrigin, level.BombExplosionOrigin);
	level.BombExplosionTime 	 = level.time;
	level.BombExplosionCheckTime = 0;
	level.Bomber			 = ent->r.ownerNum;
	G_LogPrintf ("Pop!\n");

	//Give score to the bomer
	AddScore (&g_entities[ent->r.ownerNum], origin, 1);
	Stats_AddScore (&g_entities[ent->r.ownerNum], ST_BOMB_BOOM);
}

// DensitY: Checking functions

// check if we have the correct number of flags, if we have extras, remove them
void CheckFlags (void)
{
	gentity_t  *flag = NULL;
	int    blueCount;
	int    redCount;

	//@Fenix - we will use CTF flags also in JUMP mode but we are not going to check
	//the correct number of flags since third party jump maps can have tons of flags.
	if(g_gametype.integer != GT_CTF)
	{
		return;
	}
	blueCount = 0;

	while((flag = G_FindClass (flag, bg_itemlist[UT_ITEM_BLUEFLAG].classname)) != NULL)
	{
		if(blueCount > 1)
		{
			G_FreeEntity (flag);
		}
		blueCount++;
	}
	flag	 = NULL;
	redCount = 0;

	while((flag = G_FindClass (flag, bg_itemlist[UT_ITEM_REDFLAG].classname)) != NULL)
	{
		if(redCount > 1)
		{
			G_FreeEntity (flag);
		}
		redCount++;
	}
}

/**
 * CheckBomb
 * Check that we have the correct number of bombs, if we have extras remove them.
 */
void CheckBomb (void)
{
	gentity_t  *bomb = NULL;
	int    bombCount;

	if(g_gametype.integer != GT_UT_BOMB)
	{
		return;
	}
	bombCount = 0;

	if(level.BombPlanted == qfalse)
	{
		// Check for nonplanted bomb
		// should be an item on a person
	}
	else
	{
		// Check for planted bomb
		while((bomb = G_FindClassHash (bomb, HSH_ut_planted_bomb)) != NULL)
		{
			if(bombCount > 1)
			{
				G_FreeEntity (bomb);
			}
			bombCount++;
		}
	}
}
