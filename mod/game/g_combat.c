// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_combat.c

#include "g_local.h"
#include "../common/c_weapons.h"
extern vmCvar_t  g_healthReport;
/*
============
ScorePlum
============
*/
void ScorePlum( gentity_t *ent, vec3_t origin, int score )
{
	gentity_t  *plum;

	plum			   = G_TempEntity( origin, EV_SCOREPLUM );
	// only send this temp entity to a single client
	plum->r.svFlags 	  |= SVF_SINGLECLIENT;
	plum->r.singleClient   = ent->s.number;
	//
	plum->s.otherEntityNum = ent->s.number;
	plum->s.time		   = score;
}

/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore( gentity_t *ent, vec3_t origin, int score )
{
	if (!ent->client)
	{
		return;
	}

	// no scoring during pre-match warmup
	if (level.warmupTime)
	{
		return;
	}
	// show score plum

	ent->client->ps.persistant[PERS_SCORE] += score;

	if ((g_gametype.integer == GT_TEAM) && !g_survivor.integer)
	{
		level.teamScores[ent->client->ps.persistant[PERS_TEAM]] += score;
	}
	CalculateRanks();
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
void TossClientItems( gentity_t *self )
{
	int    weaponSlot;
	int    weaponID;
	gentity_t  *weapon;

	// Drop ALL the weapons for now.
	for (weaponSlot = UT_MAX_WEAPONS - 1; weaponSlot >= 0 ; weaponSlot--) //MAX_WEAPONS
	{
		weaponID = UT_WEAPON_GETID(self->client->ps.weaponData[weaponSlot]);

		// Skip it if there is no weapon in this slot.
		if (!weaponID)
		{
			continue;
		}

		// Skip the knife
		if (weaponID == UT_WP_KNIFE)
		{
			continue;
		}

		// If its empty dont bother.
		if (UT_WEAPON_GETCLIPS(self->client->ps.weaponData[weaponSlot]) +
			UT_WEAPON_GETBULLETS(self->client->ps.weaponData[weaponSlot]) == 0)
		{
			continue;
		}

		weapon = Drop_Weapon ( self, weaponID, rand() % 360 );
	}

	weapon = Drop_Item(self, &bg_itemlist[UT_ITEM_REDFLAG], 0, qtrue);

	if (weapon)
	{
		weapon->nextthink = level.time + 50;
	}
	weapon = Drop_Item(self, &bg_itemlist[UT_ITEM_BLUEFLAG], 0, qtrue);

	if (weapon)
	{
		weapon->nextthink = level.time + 50;
	}
}

/*
==================
LookAtKiller //never used - 27
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker )
{
	vec3_t	dir;
	vec3_t	angles;

	if (attacker && (attacker != self))
	{
		VectorSubtract (attacker->s.pos.trBase, self->s.pos.trBase, dir);
	}
	else
	if (inflictor && inflictor != self)
	{
		VectorSubtract (inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	}
	else
	{
		//self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	//self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw ( dir );

	angles[YAW]   = vectoyaw ( dir ) + (rand() % 360);
	angles[PITCH] = 0;
	angles[ROLL]  = 0;
}

/*
==================
GibEntity
==================
*/
void GibEntity( gentity_t *self, int killer )
{
//	gentity_t *ent;
//	int i;

	//if this entity still has kamikaze
/*	if (self->s.eFlags & EF_KAMIKAZE) {
		// check if there is a kamikaze timer around for this owner
		for (i = 0; i < MAX_GENTITIES; i++) {
			ent = &g_entities[i];
			if (!ent->inuse)
				continue;
			if (ent->activator != self)
				continue;
//			  if (strcmp(ent->classname, "kamikaze timer"))
			if (ent->classname!=HSH_kamikaze_timer)
				continue;
			G_FreeEntity(ent);
			break;
		}
	}*/
	G_AddEvent( self, EV_GIB_PLAYER, killer );
	self->takedamage = qfalse;
	self->s.eType	 = ET_INVISIBLE;
	self->r.contents = 0;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	self->health = GIB_HEALTH + 1;
	return;

/* URBAN TERROR - No gibbing
	if ( self->health > GIB_HEALTH ) {
		return;
	}
	if ( !g_blood.integer ) {
		self->health = GIB_HEALTH+1;
		return;
	}

	GibEntity( self, 0 );
*/
}

// these are just for logging, the client prints its own messages
static char  *modNames[] = {
	"MOD_UNKNOWN",
	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT",
	"MOD_CHANGE_TEAM",
	"UT_MOD_WEAPON",
	"UT_MOD_KNIFE",
	"UT_MOD_KNIFE_THROWN",
	"UT_MOD_BERETTA",
	"UT_MOD_DEAGLE",
	"UT_MOD_SPAS",
	"UT_MOD_UMP45",
	"UT_MOD_MP5K",
	"UT_MOD_LR300", 	  // UT_MOD_M4
	"UT_MOD_G36",
	"UT_MOD_PSG1",
	"UT_MOD_HK69",
	"UT_MOD_BLED",
	"UT_MOD_KICKED",
	"UT_MOD_HEGRENADE",
	"UT_MOD_FLASHGRENADE",
	"UT_MOD_SMOKEGRENADE",
	"UT_MOD_SR8",
	"UT_MOD_SACRIFICE",
	"UT_MOD_AK103",
	"UT_MOD_SPLODED",
	"UT_MOD_SLAPPED",
	"UT_MOD_SMITED", //@Barbatos
	"UT_MOD_BOMBED",
	"UT_MOD_NUKED",
	"UT_MOD_NEGEV",
	"UT_MOD_HK69_HIT",
	"UT_MOD_M4",
	"UT_MOD_GLOCK",
	"UT_MOD_COLT1911",
	"UT_MOD_MAC11",
	"UT_MOD_FLAG",
	"UT_MOD_GOOMBA",
	"UT_MOD_MELT",
	"UT_MOD_FREEZE",
	"UT_MOD_P90",
	"UT_MOD_BENELLI",
	"UT_MOD_MAGNUM",
	"UT_MOD_FRF1",
};

/*
==================
CheckAlmostCapture
==================
*/
void CheckAlmostCapture( gentity_t *self, gentity_t *attacker )
{
	gentity_t  *ent;
	vec3_t	   dir;
//	  char		 *classname;
	int classhash;

	// if this player was carrying a flag
	if (utPSHasItem ( &self->client->ps, UT_ITEM_REDFLAG ) ||
		utPSHasItem ( &self->client->ps, UT_ITEM_BLUEFLAG) ||
		utPSHasItem ( &self->client->ps, UT_ITEM_NEUTRALFLAG))
	{
		// get the goal flag this player should have been going for
		if (g_gametype.integer == GT_CTF)
		{
			if (self->client->sess.sessionTeam == TEAM_BLUE)
			{
//				  classname = "team_CTF_blueflag";
				  classhash = HSH_team_CTF_blueflag;
			}
			else
			{
//				  classname = "team_CTF_redflag";
				  classhash = HSH_team_CTF_redflag;
			}
		}
		else
		{
			if (self->client->sess.sessionTeam == TEAM_BLUE)
			{
//				  classname = "team_CTF_redflag";
				  classhash = HSH_team_CTF_redflag;
			}
			else
			{
//				  classname = "team_CTF_blueflag";
				  classhash = HSH_team_CTF_blueflag;
			}
		}
		ent = NULL;

		do
		{
			ent = G_FindClassHash(ent, classhash);
		}
		while (ent && (ent->flags & FL_DROPPED_ITEM));

		// if we found the destination flag and it's not picked up
		if (ent && !(ent->r.svFlags & SVF_NOCLIENT))
		{
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );

			if (VectorLength(dir) < 200)
			{
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;

				if (attacker->client)
				{
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

/*
==================
CheckAlmostScored
==================
*/
void CheckAlmostScored( gentity_t *self, gentity_t *attacker )
{
	gentity_t  *ent;
	vec3_t	   dir;
//	  char		 *classname;
	int classhash;

	// if the player was carrying cubes
	if (self->client->ps.generic1)
	{
		if (self->client->sess.sessionTeam == TEAM_BLUE)
		{
//			  classname = "team_redobelisk";
			classhash = HSH_team_redobelisk;
		}
		else
		{
//			  classname = "team_blueobelisk";
			classhash = HSH_team_blueobelisk;
		}
		ent = G_FindClassHash(NULL, classhash);

		// if we found the destination obelisk
		if (ent)
		{
			// if the player was *very* close
			VectorSubtract( self->client->ps.origin, ent->s.origin, dir );

			if (VectorLength(dir) < 200)
			{
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;

				if (attacker->client)
				{
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

/*
==================
player_die
==================
*/
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	gentity_t  *ent;
	int 	   contents;
	int 	   killer;
	int 	   i, delay, delta;
	char	   *killerName, *obit;

	if (self->client->ps.pm_type == PM_DEAD)
	{
		return;
	}

	if (level.intermissiontime)
	{
		return;
	}
	//#27 - Clear the MEDIC! icon if it's up
	self->client->ps.eFlags &= ~EF_UT_MEDIC;

	// Clear planting, defusing flags.
	self->client->ps.eFlags &= EF_UT_PLANTING;
	self->client->ps.eFlags &= EF_UT_DEFUSING;

	// Clear planting/defusing flags.
	self->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_PLANTING;
	self->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_DEFUSING;

	// if we are in bomb mode and the person was the bomb holder then we pick a new bomb holder next round
	if((g_gametype.integer == GT_UT_BOMB) && (self->client->sess.bombHolder == qtrue)) {
		level.UseLastRoundBomber = qfalse;
	}

	// Report the leader death if we are playing assasins
	if ((g_gametype.integer == GT_UT_ASSASIN) && (self->client->sess.teamLeader == qtrue)) {

		for (i = 0 ; i < level.maxclients ; i++) {

			if (self->s.number == i) {
				continue;
			}

			if(level.clients[i].pers.connected != CON_CONNECTED) {
				continue;
			}

			if (level.clients[i].sess.sessionTeam != self->client->sess.sessionTeam) {
				continue;
			}

			if (inflictor && inflictor->client) {
				trap_SendServerCommand( i, va("cp \"%s%s" S_COLOR_WHITE " killed your team leader %s%s" S_COLOR_WHITE ".\"", TeamColorString ( inflictor->client->sess.sessionTeam ), inflictor->client->pers.netname, TeamColorString ( self->client->sess.sessionTeam ), self->client->pers.netname ));
			} else {
				trap_SendServerCommand( i, va("cp \"Your team leader %s%s" S_COLOR_WHITE " has been killed.\"", self->client->sess.sessionTeam == TEAM_RED ? S_COLOR_RED : S_COLOR_BLUE, self->client->pers.netname ));
			}

		}
	}

	self->client->laserSight = qfalse;

	// check for an almost capture
	CheckAlmostCapture( self, attacker );
	// check for a player that almost brought in cubes
//	CheckAlmostScored( self, attacker );

	self->client->ps.pm_type = PM_DEAD;

	// IF the weapon was readied, then fire it, but with no forward velocity.
	if ((self->client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE)) {
		FireWeapon ( self );
	}

	if (attacker) {
		killer = attacker->s.number;

		if (attacker->client) {
			killerName = attacker->client->pers.netname;
		} else {
			killerName = "<non-client>";
		}
	}
	else {
		killer	   = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ((killer < 0) || (killer >= MAX_CLIENTS)) {
		killer	   = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if ((meansOfDeath < 0) || (meansOfDeath >= sizeof(modNames) / sizeof(modNames[0]))) {
		obit = "<bad obituary>";
	} else {
		obit = modNames[meansOfDeath];
	}

	G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n", killer, self->s.number, meansOfDeath, killerName, self->client->pers.netname, obit ); //27 SOUNDLOOP BUG

	// broadcast the death event to everyone
	ent 		           = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
	ent->s.eventParm	   = meansOfDeath;
	ent->s.otherEntityNum  = self->s.number;
	ent->s.otherEntityNum2 = killer;

	if ((rand() & 100) > 98) {
		ent->s.torsoAnim = 1;
	} else {
		ent->s.torsoAnim = 0;
	}

	ent->s.generic1 = 255;

	if (attacker->client && (g_healthReport.integer == 1)) {
		ent->s.generic1 = attacker->health;
	}

	ent->r.svFlags |= SVF_BROADCAST;	// send to everyone

	self->enemy = attacker;

	if (meansOfDeath != MOD_CHANGE_TEAM) {

		self->client->ps.persistant[PERS_KILLED]++;

		//tally the actual deaths
		if (g_survivor.integer) {
			if (self->client->sess.sessionTeam == TEAM_RED) {
				level.survivorRedDeaths++;
			}

			if (self->client->sess.sessionTeam == TEAM_BLUE) {
				level.survivorBlueDeaths++;
			}
		}
	}

	if (attacker && attacker->client) {

		attacker->client->lastkilled_client = self->s.number;

		if ((attacker == self) || OnSameTeam (self, attacker )) {

			if ((meansOfDeath != MOD_CHANGE_TEAM) && (meansOfDeath != UT_MOD_BOMBED)) {

				if (g_gametype.integer != GT_JUMP) {
					AddScore( attacker, self->r.currentOrigin, -1 );
				}
			}

			if ((attacker != self) && (g_friendlyFire.integer == 1)) {

				// killed a team mate in a game where there are penalties for doing so
				// use breakable ints to record the client's transgressions

				// multiple kills inside a single second only counts as one TK
				// so that a badly thrown gren doesn't get you kicked
				if (meansOfDeath != UT_MOD_BOMBED)
				{
					if (level.time - attacker->breakaxis > 2000)
					{
						if (attacker->breakshards == 0) {
							attacker->breakaxis = level.time;			// time at which initial TK occurred for this player
						}
						attacker->breakshards++;						// number of TKs

						if (attacker->breakshards - g_maxteamkills.integer >= 0) {
							trap_DropClient( attacker->s.clientNum, "Teamkilling is bad m'kay?");
							trap_SendServerCommand( -1, va("print \"Server: %s was booted for being a TKing assclown.\n\"", attacker->client->pers.netname));
						}
						else if (attacker->breakshards - g_maxteamkills.integer == -1) {
							trap_SendServerCommand( attacker->s.clientNum, "print \"^5You killed your last teammate: next time you will be kicked.\n\"" );
						}
						else {
							trap_SendServerCommand( attacker->s.clientNum, "print \"^5You killed a teammate: you will be kicked if you continue to do so.\n\"" );
						}
					}
				}
			}
		}
		else
		{
			if (meansOfDeath != UT_MOD_BOMBED) {
				AddScore( attacker, self->r.currentOrigin, 1 );
			}

			//Bomb carrier
			if (g_gametype.integer == GT_UT_BOMB) {

				float  flagDistance;

				if(utPSGetWeaponByID(&self->client->ps, UT_WP_BOMB) > 0)
				{
					AddScore(attacker, self->r.currentOrigin, 2);
					Stats_AddScore(attacker, ST_KILL_BC);
					trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You killed the %s " S_COLOR_WHITE "bomb carrier.\"", ColoredTeamName(OtherTeam(attacker->client->sess.sessionTeam))));
				}

				flagDistance = GetDistanceToBombCarrier(attacker->r.currentOrigin);

//				trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"Distance To Flag For Flag Protect DEF is %d\n\"", flagDistance));
				if ((flagDistance < CTF_PROTECT_RADIUS) && (flagDistance > 32) && (attacker->client->sess.sessionTeam == level.AttackingTeam)) {
					trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You protected the %s " S_COLOR_WHITE "bomb carrier.\"", ColoredTeamName(OtherTeam(self->client->sess.sessionTeam))));
					AddScore(attacker, self->r.currentOrigin, 1);
					Stats_AddScore(attacker, ST_PROTECT_BC);
				}

				if (self->client->IsDefusing) {
					trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You killed someone defusing.\"", ColoredTeamName(OtherTeam(self->client->sess.sessionTeam))));
					AddScore(attacker, self->r.currentOrigin, 1);
					Stats_AddScore(attacker, ST_KILL_DEFUSE);
				}
			}

			//CTF Assists.
			if(g_gametype.integer == GT_CTF) {

				float  flagDistance;

				flagDistance = GetDistanceToFlag(self->r.currentOrigin, self->client->sess.sessionTeam);

//				trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"Distance To Flag For Flag Protect CAP is %d\n\"", flagDistance));
				if((utPSGetItemByID(&self->client->ps, UT_ITEM_REDFLAG) != -1) || (utPSGetItemByID (&self->client->ps, UT_ITEM_BLUEFLAG) != -1)) {
					AddScore(attacker, self->r.currentOrigin, CTF_FRAG_CARRIER_BONUS);
					Stats_AddScore(attacker, ST_KILL_FC);
					trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You killed the %s " S_COLOR_WHITE "flag carrier.\"", ColoredTeamName(OtherTeam(attacker->client->sess.sessionTeam))));

					if((flagDistance < CTF_PROTECT_RADIUS) && (flagDistance != -1.0f)) {
						trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You prevented the %s team from capturing.\"", ColoredTeamName(self->client->sess.sessionTeam)));
						AddScore(attacker, self->r.currentOrigin, CTF_PREVENT_CAPTURE_BONUS);
						Stats_AddScore(attacker, ST_STOP_CAP);
					}
				}

				flagDistance = GetDistanceToFlag(self->r.currentOrigin, OtherTeam(self->client->sess.sessionTeam));

//				trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"Distance To Flag For Flag Protect DEF is %d\n\"", flagDistance));
				if((flagDistance < CTF_PROTECT_RADIUS) && (flagDistance != -1.0f)) {
					trap_SendServerCommand(attacker->client->ps.clientNum, va("print \"You protected the %s " S_COLOR_WHITE "flag.\"", ColoredTeamName(OtherTeam(self->client->sess.sessionTeam))));
					AddScore(attacker, self->r.currentOrigin, CTF_FLAG_DEFENSE_BONUS);
					Stats_AddScore(attacker, ST_PROTECT_FLAG);
				}
			}
		}
	}
	else if (meansOfDeath != UT_MOD_BLED && meansOfDeath != UT_MOD_BOMBED) {
		if (g_gametype.integer != GT_JUMP) {
			AddScore( self, self->r.currentOrigin, -1 );
		}
	}

	// if I committed suicide, the flag does not fall, it returns.
/*	if ( meansOfDeath == MOD_CHANGE_TEAM )
	{
		int itemSlot;
		if ( -1 != (itemSlot = utPSGetItemByID ( &self->client->ps, UT_ITEM_NEUTRALFLAG ) ) ) { 	// only happens in One Flag CTF
			Team_ReturnFlag( TEAM_FREE );
//			self->client->ps.powerups[PW_NEUTRALFLAG] = 0;
			UT_ITEM_SETID ( self->client->ps.itemData[itemSlot], 0 );
		}
		else if ( -1 != (itemSlot = utPSGetItemByID ( &self->client->ps, UT_ITEM_REDFLAG) ) ) { 	// only happens in standard CTF
			Team_ReturnFlag( TEAM_RED );
//			self->client->ps.powerups[PW_REDFLAG] = 0;
			UT_ITEM_SETID ( self->client->ps.itemData[itemSlot], 0 );
		}
		else if ( -1 != (itemSlot = utPSGetItemByID ( &self->client->ps, UT_ITEM_BLUEFLAG) ) ) {	// only happens in standard CTF
			Team_ReturnFlag( TEAM_BLUE );
//			self->client->ps.powerups[PW_BLUEFLAG] = 0;
			UT_ITEM_SETID ( self->client->ps.itemData[itemSlot], 0 );
		}
		else if( utPSGetWeaponByID( &self->client->ps, UT_WP_BOMB ) != -1 ) {
			// Give Bomb to next person
			Drop_Weapon( self, UT_WP_BOMB, rand()%360 );
		}
	} */

	// meansOfDeath == MOD_SUICIDE

	// if client is in a nodrop area, don't drop anything (but return CTF flags!)
	// D8: this is a mess, needs a recode
	contents = trap_PointContents( self->r.currentOrigin, -1 );

	// if the player dies in a nodrop then don't return the flags, just drop them
	// because the flag may land in an okay area: the item toss code
	// handles drops in a nodrop incase it also lands in a nodrop
//	if ( utPSGetItemByID(&self->client->ps, UT_ITEM_NEUTRALFLAG) != -1 ) {		// only happens in One Flag CTF
	//Team_ReturnFlag( TEAM_FREE );
//		Drop_Item(self, &bg_itemlist[UT_ITEM_NEUTRALFLAG], 0);
//	}
	//ps is not valid in these cases
	if ((self->client->sess.sessionTeam != TEAM_SPECTATOR) || (self->client->sess.spectatorState == SPECTATOR_NOT))
	{
		if (!self->client->pers.substitute && !self->client->ghost)
		{
			if (utPSGetItemByID(&self->client->ps, UT_ITEM_REDFLAG) != -1) // only happens in standard CTF
			{  //Team_ReturnFlag( TEAM_RED );
				Drop_Item(self, &bg_itemlist[UT_ITEM_REDFLAG], 0, qtrue);
			}
			else if (utPSGetItemByID(&self->client->ps, UT_ITEM_BLUEFLAG) != -1) // only happens in standard CTF
			{  //Team_ReturnFlag( TEAM_BLUE );
				Drop_Item(self, &bg_itemlist[UT_ITEM_BLUEFLAG], 0, qtrue);
			}
			else if(utPSGetWeaponByID( &self->client->ps, UT_WP_BOMB ) != -1)
			{
				// Drop the bomb
				Drop_Weapon( self, UT_WP_BOMB, rand() % 360 );
			}
		}
	}

	if (!(contents & CONTENTS_NODROP))
	{
		if (meansOfDeath != MOD_CHANGE_TEAM)
		{
			TossClientItems( self );
		}
	}

	//When someone gets greased, its better just to update everyone.
	//This is because most everyone runs with the team hud up now.
	//SendScoreboardMessageToAllClients();

	// If killed by another client.
	if (attacker && attacker->client && (attacker != self))
	{
		SendScoreboardDoubleMessageToAllClients(self, attacker);
	}
	else
	{
		SendScoreboardSingleMessageToAllClients(self);
	}

	if (g_gametype.integer == 3)
	{
		SendScoreboardGlobalMessageToAllClients();
	}
	//Cmd_Score_f( self );		// show scores

	/*// send updated scores to any clients that are following this one,
		 // or they would get stale scoreboards
		 for ( i = 0 ; i < level.maxclients ; i++ )
	{
			 gclient_t	*client;

			 client = &level.clients[i];
			 if ( client->pers.connected != CON_CONNECTED )
	   {
				 continue;
			 }
			 if ( client->sess.sessionTeam != TEAM_SPECTATOR )
	   {
				 continue;
			 }
			 if ( client->sess.spectatorClient == self->s.number ) {
				 Cmd_Score_f( g_entities + i );
			 }
		 }*/

	if (meansOfDeath == MOD_CHANGE_TEAM) {
		return; // we dont want a death if they change team
	}

	self->flags 			   &= ~FL_GODMODE;
	self->client->ps.stats[STAT_HEALTH] = self->health = 0;
	self->s.eFlags			   |= EF_DEAD;
	self->client->ps.pm_type	= PM_DEAD; //this renders the player invisble..

	self->takedamage			= qfalse;

	self->s.weapon				= UT_WP_NONE;
	self->r.contents			= 0; //CONTENTS_CORPSE;  //we're not really a corpse, we just want to not be walk intoable

	self->s.angles[0]			= 0;
	self->s.angles[2]			= 0;
	self->s.angles[1]			= self->client->ps.viewangles[1] + ((rand() % 180) - 90); //SPIN A BIT ON DEATH
	//LookAtKiller (self, inflictor, attacker);

	VectorCopy( self->s.angles, self->client->ps.viewangles );
	self->s.loopSound = 0;

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
	if (g_survivor.integer)
	{
		self->client->respawnTime = level.time + 4000;
	}
	else
	{
		// If we're doing wave respawns.
		if (g_waveRespawns.integer && (g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP) && ((self->client->sess.sessionTeam == TEAM_RED) || (self->client->sess.sessionTeam == TEAM_BLUE)))
		{
			if (self->client->sess.sessionTeam == TEAM_RED)
			{
				delay = g_redWave.integer;
			}
			else
			{
				delay = g_blueWave.integer;
			}

			if (delay <= 2)
			{
				delay = 2;
			}

			//Figure out next time this should occur   ( time + time mod rd)
			delta			  = (level.time) + ((delay * 1000) - ((level.time - level.pausedTime) % (delay * 1000)));

			self->client->respawnTime = delta;

			// If death was the result of a suicide.
			if (meansOfDeath == MOD_SUICIDE)
			{
				// Increase respawn delay.
				self->client->respawnTime += delay * 1000;
			}
		}
		else if (g_respawnDelay.integer > 2)
		{
			self->client->respawnTime = level.time + g_respawnDelay.integer * 1000;
		}
		else
		{
			self->client->respawnTime = level.time + 2000;
		}
	}

	self->client->protectionTime = self->client->respawnTime;
	//self->client->sess.inactivityTime += self->client->respawnTime - level.time;

	G_AddEvent( self, EV_DEATH1, meansOfDeath );	//killer );

	G_UnTimeShiftClient(self); // sets us back to where we were for the corpse
	G_CreateCorpse(self, damage, meansOfDeath);
	self->s.powerups		  = 0;

	self->client->ps.legsAnim	  = 0;
	self->client->ps.torsoAnim	  = 0;
	self->client->legsPitchAngle  = 0;
	self->client->torsoPitchAngle = 0;
	self->client->legsYawAngle	  = 0;
	self->client->torsoYawAngle   = 0;

	// wtf!
	//trap_LinkEntity(self);

	// Should be this!
	trap_UnlinkEntity( self );

	StatsEngineDeath(attacker->s.clientNum, meansOfDeath, self->s.clientNum);
}

//Pops a body in to "die" for the player
//Figures out which anim to use as well, which can be quite fun ^_^
void G_CreateCorpse(gentity_t *cent, int damage, int mod)
{
	gentity_t  *corpse;
	int    anim;

	corpse = G_Spawn();

	//memset(corpse,0,sizeof(gentity_t));
	if (!corpse)
	{
		return;
	}

	corpse->classname = "player_corpse";
	corpse->classhash = HSH_player_corpse;

	if (g_survivor.integer && !level.warmupTime) //@r00t: Remove corpses during warmup to prevent exploiting
	{
		corpse->nextthink = level.time + 1000000;
	}
	else
	{
		corpse->nextthink = level.time + 10000;
	}
	corpse->timestamp	 = level.time;
	corpse->think		 = G_FreeEntity;
	corpse->clipmask	 = MASK_SHOT;
	corpse->r.contents	 = 0; //CONTENTS_CORPSE;
	corpse->s.eType 	 = ET_CORPSE;
	corpse->r.ownerNum	 = cent->s.number;
	corpse->parent		 = cent;
	corpse->r.mins[0]	 = cent->r.mins[0];
	corpse->r.mins[1]	 = cent->r.mins[1];
	corpse->r.mins[2]	 = cent->r.mins[2];
	corpse->r.maxs[0]	 = cent->r.maxs[0];
	corpse->r.maxs[1]	 = cent->r.maxs[1];
	corpse->r.maxs[2]	 = cent->r.maxs[2];
	corpse->s.clientNum  = cent->s.number;
	corpse->s.generic1	 = 1 + rand() % 60000; // This makes a little Unique Key so we can tell the very first frame we've been rendered
	corpse->s.origin2[0] = level.time;

	corpse->s.pos.trTime = level.time;	// move a bit on the very first frame
	G_SetOrigin(corpse, cent->s.pos.trBase );
	cent->s.apos.trType  = TR_INTERPOLATE; // no rots
	corpse->s.pos.trType = TR_INTERPOLATE; // but bounce ;)
	corpse->s.pos.trTime = level.time - 50;

	corpse->s.eFlags	 = EF_BOUNCE_HALF;
	VectorCopy(cent->s.pos.trBase, corpse->s.pos.trBase );
	VectorCopy(cent->s.pos.trBase, corpse->r.currentOrigin);
	VectorCopy(cent->client->ps.velocity, corpse->s.pos.trDelta );
	VectorCopy(cent->s.apos.trBase, corpse->s.apos.trBase);

	//VectorScale( dir, speed, knife->s.pos.trDelta );
	//SnapVector( knife->s.pos.trDelta );			// save net bandwidth

	corpse->target_ent = NULL;

	//set the team/race
	corpse->s.modelindex	 = cent->client->sess.sessionTeam;
	corpse->s.modelindex2	 = cent->client->pers.race;
	///
	corpse->s.apos.trBase[0] = 0;
	corpse->s.apos.trBase[2] = 0;

	//copy over the relative stuff so the client corpse can draw it in
	corpse->s.powerups	  = cent->s.powerups;
	corpse->s.time		  = cent->s.time; //copy the damage skin stuff too.

	corpse->s.pos.trDelta[0] *= 0.1f;
	corpse->s.pos.trDelta[1] *= 0.1f;
	corpse->s.pos.trDelta[2] *= 0.1f;

	//SET UP THE ANIMATIONS

	anim = BOTH_DEATH_CHEST;

	if( cent->client->ps.pm_flags & PMF_DUCKED )
	{
		anim = BOTH_DEATH_CROUCHED;
	}
	else if( mod == MOD_FALLING )
	{
		anim = BOTH_DEATH_LEMMING;
	}
	else
	{
		switch (damage)
		{
			case HL_HEAD:
				//put a hole in the head of the corpse
//	   corpse->s.powerups|=UT_HEAD_BULLET_HOLE_BIT; // Only if we draw the head, though

				//head explodes
				if ((mod == UT_MOD_DEAGLE) || //these weps make head go boom
					(mod == UT_MOD_PSG1) ||
					(mod == UT_MOD_SR8) ||
					(mod == UT_MOD_SPAS)
					/*(mod == UT_MOD_BENELLI)*/)
				{
					corpse->s.powerups |= POWERUP(PW_HASNOHEAD);
				}
				anim = BOTH_DEATH_HEADFRONT;
				break;

			/*	case HL_NECK:
				  {
				 anim = BOTH_DEATH_HEADBACK;
				 break;
				  } */
			case HL_UNKNOWN:
				break;
			default:
				switch (rand() % 3)
				{
					case 0:
						anim = BOTH_DEATH_CHEST;
						break;

					case 1:
						anim = BOTH_DEATH_BACK;
						break;

					case 2:
						anim = BOTH_DEATH_BLOWBACK;
						break;
				}
				break;
		}
	}

	corpse->s.legsAnim	= anim; //cent->s.legsAnim;
	corpse->s.torsoAnim = anim; //cent->s.torsoAnim;

	corpse->switchType	= 0;
	///////////////////////
	trap_LinkEntity(corpse);
}

/*
================
RaySphereIntersections
================
*/
int RaySphereIntersections( vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2] )
{
	float  b, c, d, t;

	//	| origin - (point + t * dir) | = radius
	//	a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	//	c = (point[0] - origin[0])^2 + (point[1] - origin[1])^2 + (point[2] - origin[2])^2 - radius^2;

	// normalize dir so a = 1
	VectorNormalizeNR(dir);
	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	c = (point[0] - origin[0]) * (point[0] - origin[0]) +
		(point[1] - origin[1]) * (point[1] - origin[1]) +
		(point[2] - origin[2]) * (point[2] - origin[2]) -
		radius * radius;

	d = b * b - 4 * c;

	if (d > 0)
	{
		t = (-b + sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[0]);
		t = (-b - sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[1]);
		return 2;
	}
	else if (d == 0)
	{
		t = (-b) / 2;
		VectorMA(point, t, dir, intersections[0]);
		return 1;
	}
	return 0;
}

/**
 * $(function)
 */
void G_ReduceStamina ( gentity_t *ent, int amount )
{
	ent->client->ps.stats[STAT_STAMINA] -= amount * UT_STAM_MUL;

	if(ent->client->ps.stats[STAT_STAMINA] < 0)
	{
		ent->client->ps.stats[STAT_STAMINA] = 0;
	}
}

/**
 * $(function)
 */
void G_Damage (gentity_t *targ,
               gentity_t *inflictor,
		       gentity_t *attacker,
		       vec3_t dir,
		       vec3_t point,
		       int damage,
		       int dflags,
		       int mod,
		       HitLocation_t hitloc) {

	gclient_t  *client;
	int 	   take;
	int 	   save;
	float	   knockback;
	int 	   w;

	if (!targ->takedamage) {
		return;
	}

	// @Fenix - If we are playing jump mode do not allow
	// a player to hit another player with any weapon.
	if ((g_gametype.integer == GT_JUMP) &&
		(mod > UT_MOD_WEAPON) &&
		(mod != UT_MOD_SMITED) &&
		(targ) &&
		(targ->client)) {
		return;
	}

	// @Fenix - If we are playing jump mode and g_nodamage is enabled on the server
	// do not allow players to die because of MOD_TELEFRAG and MOD_FALLING.
	if ((g_gametype.integer == GT_JUMP) &&
		(g_noDamage.integer > 0) &&
		((mod == MOD_TELEFRAG) || (mod == MOD_FALLING))) {
		return;
	}

	// Instagib: player damage to players becomes a one-shot kill (new in 4.3)
	if (g_instagib.integer &&
		attacker && attacker->client &&
		targ && targ->client &&
		attacker != targ &&
		mod > UT_MOD_WEAPON) {
		damage = 9999;
	}

	// See if they are invulnerable
	if (mod > UT_MOD_WEAPON) {
		// Frozen players are immune to all player-inflicted damage (Freeze Tag - 4.3)
		if (targ->client && targ->client->freezeTime > 0 && mod != UT_MOD_MELT) {
			return;
		}

		if (targ->client && (level.time - targ->client->protectionTime < g_respawnProtection.integer * 1000)) {
			return;
		}
	}

	// the intermission has already been qualified for, so don't
	// allow any extra scoring
	if (level.intermissionQueued) {
		return;
	}

	// If we're paused.
	if (level.pauseState & UT_PAUSE_ON) {
		return;
	}

	// Set the no join if this is survivor and someone got shot
	if (g_survivor.integer && !level.survivorNoJoin && (mod > UT_MOD_WEAPON)) {
		if (targ->client && attacker->client) {
			if (targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam) {
				level.survivorNoJoin = qtrue;
			}
		}
	}

	if (targ->client) {
		targ->client->noGearChange = qtrue;
	}

	if (!inflictor) {
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}

	if (!attacker) {
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// shootable doors / buttons don't actually have any health
	if ((targ->s.eType == ET_MOVER) && /*strcmp(targ->classname, "func_breakable")*/targ->classhash != HSH_func_breakable) {
		if (targ->use && (targ->mover->moverState == MOVER_POS1)) {
			targ->use( targ, inflictor, attacker );
		}
		return;
	}

	client = targ->client;

	// If a noclip client is being hurt then ignore it
	if (client && client->noclip) {
		return;
	}

	if (!dir) {
		dflags |= DAMAGE_NO_KNOCKBACK;
	} else {
		VectorNormalizeNR(dir);
	}

	//@Barbatos bugfix: a spectator can't hit a player
	if(attacker->client && (attacker != targ) && (attacker->client->sess.sessionTeam == TEAM_SPECTATOR)) {
		return;
	}

	knockback = 0;

	// find weapon using mod - can't use attacker weapon
	// because then may have changed weapons, and damage
	// may be caused by bleeding.
	for (w = 0; w < bg_numWeapons; w++) {
		if (bg_weaponlist[w].mod == mod) {
			knockback = bg_weaponlist[w].knockback;
			break;
		}
	}

	if ((mod == UT_MOD_HK69) || (mod == UT_MOD_HEGRENADE) || (mod == UT_MOD_SACRIFICE)) {
		knockback = damage / 2;
	} else if (mod == UT_MOD_HK69_HIT) {
		knockback = damage;
	}

	if (knockback > 200) {
		knockback = 200;
	}

	if (targ->flags & FL_NO_KNOCKBACK) {
		knockback = 0;
	}

	if (dflags & DAMAGE_NO_KNOCKBACK) {
		knockback = 0;
	}

	// check for completely getting out of the damage
	if (!(dflags & DAMAGE_NO_PROTECTION)) {
		// if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
		// if the attacker was on the same team
		if ((targ != attacker) && OnSameTeam (targ, attacker)) {
			if (!g_friendlyFire.integer) {
				return;
			}
		}

		// check for godmode
		if (targ->flags & FL_GODMODE) {
			return;
		}
	}

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if (attacker->client && (targ != attacker) && (targ->health > 0)
		&& (targ->s.eType != ET_MISSILE)
		&& (targ->s.eType != ET_GENERAL)
		&& (targ->s.eType != ET_MOVER)) {
		if (OnSameTeam( targ, attacker )) {
			attacker->client->ps.persistant[PERS_HITS]--;
		} else {
			attacker->client->ps.persistant[PERS_HITS]++;
		}
	}

	if (damage < 1) {
		damage = 1;
	}

	take = damage;
	save = 0;

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client) {
		if (attacker) {
			client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
		} else {
			client->ps.persistant[PERS_ATTACKER] = ENTITYNUM_WORLD;
		}

		if(mod != UT_MOD_BLED) {
			client->damage_blood += take;
//			client->damage_knockback += knockback;
			if (dir) {
				VectorCopy ( dir, client->damage_from );
				client->damage_fromWorld = qfalse;
			} else {
				VectorCopy ( targ->r.currentOrigin, client->damage_from );
				client->damage_fromWorld = qtrue;
			}
		}
		else {
			client->damage_armor += take;
			VectorCopy ( targ->r.currentOrigin, client->damage_from );
			client->damage_fromWorld = qtrue;
		}
	}

	// Dokta8 - check for fall and if enough damage, hurt legs
	if ((mod == MOD_FALLING) && (take >= 30)) {
		targ->client->ps.pm_flags |= UT_PMF_LIMPING;
	}

	if (targ->client) {
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod	  = mod;
	}

	// do the damage
	if (!take)
	{
		return;
	}

	// Dokta8 - check to see if object shatters
    // if (!strcmp(targ->classname, "func_breakable"))
	if (targ->classhash==HSH_func_breakable) {
		//int rn;

		// If this entity is bombable.
		if (targ->bombable) {
			// If damage was caused by a bomb.
			if (mod == UT_MOD_BOMBED) {
				targ->health = -999;
				targ->enemy  = attacker;
				targ->use(targ, NULL, NULL);
			}
			return;
		}
		else {
			// If damage was caused by a bomb.
			if (mod == UT_MOD_BOMBED) {
				targ->health = -999;
				targ->enemy  = attacker;
				targ->use(targ, NULL, NULL);
				return;
			}
		}

		/*
		// only shatter sometimes
		rn = rand() % 100;

		if (( mod != UT_MOD_KNIFE_THROWN ) &&
			( mod != UT_MOD_HEGRENADE ) &&
			( mod != UT_MOD_KNIFE ) &&
			( mod != UT_MOD_KICKED )) {

			if (rn >= 85) {
				take = 3;
			} else if ( rn >= 50) {
				take = 2;
			} else {
				take = 1;
			}
		}
		*/

		// Select breakable type.
		switch (targ->breaktype) {
			case 0: // Glass.
			case 1: // Wood - normal.
			case 2: // Ceramic.
			case 3: // Plastic.
			case 4: // Metal - normal.

				if ((mod != UT_MOD_KNIFE_THROWN) &&
					(mod != UT_MOD_HEGRENADE) &&
					(mod != UT_MOD_HK69) &&
					(mod != UT_MOD_KNIFE) &&
					(mod != UT_MOD_KICKED)) {

					int  rn;

					rn = rand() % 100;

					if (rn >= 85)
					{
						take = 3;
					}
					else if (rn >= 50)
					{
						take = 2;
					}
					else
					{
						take = 1;
					}
				}

				break;

			case 5: // Metal - explosives.

				if ((mod != UT_MOD_HEGRENADE) && (mod != UT_MOD_HK69))
				{
					return;
				}
				else if (mod == UT_MOD_HEGRENADE)
				{
					take /= 2;
				}

				break;

			case 6: // Metal - hk69.

				if (mod != UT_MOD_HK69)
				{
					return;
				}

				break;

			case 7: // Stone - explosives.

				if ((mod != UT_MOD_HEGRENADE) && (mod != UT_MOD_HK69))
				{
					return;
				}
				else if (mod == UT_MOD_HEGRENADE)
				{
					take /= 2;
				}

				break;

			case 8: // Stone - hk69.

				if (mod != UT_MOD_HK69)
				{
					return;
				}

				break;

			case 9: // Wood - explosives.

				if ((mod != UT_MOD_HEGRENADE) && (mod != UT_MOD_HK69))
				{
					return;
				}
				else if (mod == UT_MOD_HEGRENADE)
				{
					take /= 2;
				}

				break;

			case 10: // Wood - hk69.

				if (mod != UT_MOD_HK69)
				{
					return;
				}

				break;
		}

		targ->health = targ->health - take;

		if (targ->health < -999) {
			targ->health = -999;
		}

		targ->enemy = attacker;

		// if a breakable object has been damaged, then 'use' it
		targ->use (targ, NULL, NULL);
		return;
	}

	targ->health = targ->health - take;
#ifdef DEBUG
	G_Printf( "Health: %d (Damage -%d)\n", targ->health, take );
#endif
	targ->bleedEnemy	   = attacker;
	targ->bleedHitLocation = hitloc;

	// IF this is a client being damamged then copy the new health
	// to its playerstate
	if (client) {
		client->ps.stats[STAT_HEALTH] = targ->health;

		// Reduce the stamina based on the damage taken
		G_ReduceStamina ( targ, take );
	}

	//clear flashbangs if you take damage
	if (client && mod !=UT_MOD_FLASHGRENADE)  //flashgrens wont clear flashgrens
	{
		int blind =client->ps.stats[STAT_BLIND] & 0x0000FFFF;
		
		if (blind>500) //fade time is 2000, so 3/4 clear it
		{
			int sound=0;  //clear the sound		
			blind=500;
			client->ps.stats[STAT_BLIND] = blind | (sound << 16);
		}

	}
	
	// Check for death.
	if (targ->health <= 0) {

		// do knockback on death
		G_DoKnockBack( targ, dir, knockback );

		if (targ->health < -999) {
			targ->health = -999;
		}

		targ->enemy = attacker;

		// If this ent has a death function then do it
		if (targ->die) {
			targ->die (targ, inflictor, attacker, targ->client ? hitloc : take, mod);
		}

		return;

	}
	else if (targ->pain) {
		// No death, but if they have a pain func then do that now
		targ->pain (targ, attacker, take);
	}

	// apply knockback for grens, and kicking
	if ((mod == UT_MOD_SPAS) ||
		//(mod == UT_MOD_BENELLI) ||
		(mod == UT_MOD_HK69) ||
		(mod == UT_MOD_HEGRENADE) ||
		(mod == UT_MOD_SACRIFICE) ||
		(mod == UT_MOD_KICKED) ||
		(mod == UT_MOD_HK69_HIT)) {
		dir[2] = 0;  // Iain: Cancel out vert component of knockback to kill spas jumping etc
		G_DoKnockBack ( targ, dir, knockback );
	}
}

//
// G_DoKnockBack
// Applies weapon knockback to a target client entity
//

void G_DoKnockBack( gentity_t *targ, vec3_t dir, float knockback )
{
	vec3_t	kvel;
	float	mass;

	if (!targ->client)
	{
		return;
	}

	if (!knockback)
	{
		return;
	}

	mass = 200.0f;

	VectorScale (dir, g_knockback.value * (float)knockback / mass, kvel);
	VectorAdd (targ->client->ps.velocity, kvel, targ->client->ps.velocity);

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if (!targ->client->ps.pm_time)
	{
		int  t;

		t = knockback * 2;

		if (t < 50)
		{
			t = 50;
		}

		if (t > 200)
		{
			t = 200;
		}

		targ->client->ps.pm_time   = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.
Used for explosions and melee attacks.
============
*/
qboolean CanDamage(gentity_t *targ, vec3_t origin) {
	vec3_t dest;
	trace_t tr;
	vec3_t midpoint;
	vec3_t offsetmins = { -15.f, -15.f, -15.f };
	vec3_t offsetmaxs = {  15.f,  15.f,  15.f };

	// use the midpoint of the bounds instead of the origin, because bmodels may have their origin is 0, 0, 0
	VectorAdd(targ->r.absmin, targ->r.absmax, midpoint);
	VectorScale(midpoint, 0.5, midpoint);

	VectorCopy(midpoint, dest);
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0 || tr.entityNum == targ->s.number) return qtrue;
	// this should probably check in the plane of projection, rather than in world coordinate, and also include Z
	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[2];
	dest[2] += offsetmins[2];
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);

	if (tr.fraction == 1.0) return qtrue;

 	return qfalse;
 }

////////////////////////////////////////////////////////

int G_EntitiesInRadius(vec3_t center, float radius, gentity_t *ignore, int *entities)
{
	gentity_t  *entity;
	vec3_t	   mins, maxs;
	int 	   boxEntities[MAX_GENTITIES];
	int 	   numBoxEntities;
	vec3_t	   origin, width, temp;
	trace_t    trace;
	qboolean   visible;
	int 	   numRadiusEntities;
	vec3_t	   sign;
	int 	   i, j;

	// If an invalid array pointer.
	if (!entities)
	{
		return 0;
	}

	// Clear stuff.
	numRadiusEntities = 0;

	// Clamp radius.
	if (radius < 1)
	{
		radius = 1;
	}

	// Calculate radius box.
	for (i = 0; i < 3; i++)
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	// Get all entities whitin the radius box.
	numBoxEntities = trap_EntitiesInBox(mins, maxs, boxEntities, MAX_GENTITIES);

	// Loop through box entities.
	for (i = 0; i < numBoxEntities; i++)
	{
		// Get entity.
		entity = &g_entities[boxEntities[i]];

		// If not a valid entity.
		if ((ignore && (entity->s.number == ignore->s.number)) || !entity->takedamage)
		{
			continue;
		}

		// Find closest point on entity's hitbox from center.
		for (j = 0; j < 3; j++)
		{
			if (center[j] < entity->r.absmin[j])
			{
				temp[j] = entity->r.absmin[j] - center[j];
			}
			else if (center[j] > entity->r.absmax[j])
			{
				temp[j] = center[j] - entity->r.absmax[j];
			}
			else
			{
				temp[j] = 0;
			}
		}

		// If entity is out of range.
		if (VectorLength(temp) >= radius)
		{
			continue;
		}

		// Calculate entity's hitbox origin.
		for (j = 0; j < 3; j++)
		{
			origin[j] = (entity->r.absmax[j] + entity->r.absmin[j]) / 2;
		}

		// Calculate entity's hitbox width.
		for (j = 0; j < 3; j++)
		{
			width[j] = entity->r.maxs[j] - entity->r.mins[j];
		}

		// Clear visible flag.
		visible = qfalse;

		// Do a trace to entity's origin.
		trap_Trace(&trace, center, NULL, NULL, origin, entity->s.number, MASK_SHOT & ~CONTENTS_BODY);

		// If entity is visible.
		if (trace.fraction == 1)
		{
			visible = qtrue;
		}

		// If entity not visible.
		if (!visible)
		{
			// Do entity hitbox sides visibility test.
			for (j = 0; j < 6; j++)
			{
				// TODO: Find a less retarted way of doing this.
				if (j == 0)
				{
					sign[0] = 0;
					sign[1] = -1;
					sign[2] = 0;
				}
				else if (j == 1)
				{
					sign[0] = 1;
					sign[1] = 0;
					sign[2] = 0;
				}
				else if (j == 2)
				{
					sign[0] = 0;
					sign[1] = 1;
					sign[2] = 0;
				}
				else if (j == 3)
				{
					sign[0] = -1;
					sign[1] = 0;
					sign[2] = 0;
				}
				else if (j == 4)
				{
					sign[0] = 0;
					sign[1] = 0;
					sign[2] = 1;
				}
				else if (j == 5)
				{
					sign[0] = 0;
					sign[1] = 0;
					sign[2] = -1;
				}

				// Calculate point.
				temp[0] = origin[0] + ((width[0] / 2) * sign[0]);
				temp[1] = origin[1] + ((width[1] / 2) * sign[1]);
				temp[2] = origin[2] + ((width[2] / 2) * sign[2]);

				// If this is a client and we're checking the top side.
				if (entity->client && (j == 4))
				{
					// Nudge point down.
					temp[2] -= 8;
				}

				// Do a trace.
				trap_Trace(&trace, center, NULL, NULL, temp, entity->s.number, MASK_SHOT & ~CONTENTS_BODY);

				// If entity is visible.
				if (trace.fraction == 1)
				{
					// Set visible flag.
					visible = qtrue;

					break;
				}
			}
		}

		// If entity not visible.
		if (!visible)
		{
			// Do entity hitbox corners visibility test.
			for (j = 0; j < 8; j++)
			{
				// TODO: Find a less retarted way of doing this.
				if (!(j & 0x3))
				{
					sign[0] = -1;
					sign[1] = -1;
				}
				else if ((j & 0x1) && !(j & 0x2))
				{
					sign[0] = 1;
					sign[1] = -1;
				}
				else if (!(j & 0x1) && (j & 0x2))
				{
					sign[0] = 1;
					sign[1] = 1;
				}
				else if ((j & 0x1) && (j & 0x2))
				{
					sign[0] = -1;
					sign[1] = 1;
				}

				if (j & 0x4)
				{
					sign[2] = -1;
				}
				else
				{
					sign[2] = 1;
				}

				// Calculate point.
				temp[0] = origin[0] + ((width[0] / 2) * sign[0]);
				temp[1] = origin[1] + ((width[1] / 2) * sign[1]);
				temp[2] = origin[2] + ((width[2] / 2) * sign[2]);

				// If this is a client and we're checking the top points.
				if (entity->client && !(j & 0x4))
				{
					// Nudge point down.
					temp[2] -= 8;
				}

				// Do a trace.
				trap_Trace(&trace, center, NULL, NULL, temp, entity->s.number, MASK_SHOT & ~CONTENTS_BODY);

				// If entity is visible.
				if (trace.fraction == 1)
				{
					// Set visible flag.
					visible = qtrue;

					break;
				}
			}
		}

		// If entity is visible.
		if (visible)
		{
			// Add entity to list.
			entities[numRadiusEntities++] = entity - g_entities;
		}
	}

	return numRadiusEntities;
}

////////////////////////////////////////////////////////

void G_BombDamage(vec3_t origin, float damage, float radius, float maxRadius, int mod)
{
	gentity_t  *entity;
	vec3_t	   mins, maxs;
	vec3_t	   dir;
	int 	   boxEntities[MAX_GENTITIES];
	int 	   numBoxEntities;
	float	   dist, scale;
	int 	   i;

	// Calculate radius box.
	for (i = 0; i < 3; i++)
	{
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	// Get all entities whitin the radius box.
	numBoxEntities = trap_EntitiesInBox(mins, maxs, boxEntities, MAX_GENTITIES);

	// Loop through entities.
	for (i = 0; i < numBoxEntities; i++)
	{
		// Get entity.
		entity = &g_entities[boxEntities[i]];

		// If not a valid entity.
		if (!entity->takedamage)
		{
			continue;
		}

		// If entity already got bombed.
		if (entity->bombedTime > level.BombExplosionTime)
		{
			continue;
		}

		// Calculate distance.
		VectorSubtract(entity->r.currentOrigin, origin, dir);
		VectorNormalize(dir, dist);

		// Update time.
		entity->bombedTime = level.time;

		// If we're outside blast radius.
		if (dist >= maxRadius)
		{
			continue;
		}

		// Calculate damage scale.
		scale = damage * (float)(1.0f + sin(M_PI + ((M_PI / 2) * (dist / maxRadius))));

		// Damage entity.
		G_Damage(entity, entity, &g_entities[level.Bomber], dir, origin, (int)scale, DAMAGE_RADIUS, mod, HL_UNKNOWN);
	}
}

/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage(vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod)
{
	gentity_t  *entity;
	vec3_t	   temp;
	int 	   entities[MAX_GENTITIES];
	int 	   numEntities, i, j;
	float	   distance, points;

	// Clear stuff.
	numEntities = 0;

	for (i = 0; i < 5; i++)
	{
		// Initialize vector with origin.
		temp[0] = origin[0];
		temp[1] = origin[1];
		temp[2] = origin[2] + 8;

		// Nudge vector.
		if (i == 1)
		{
			temp[0] += 4;
		}
		else if (i == 2)
		{
			temp[0] -= 4;
		}
		else if (i == 3)
		{
			temp[1] += 4;
		}
		else if (i == 4)
		{
			temp[1] -= 4;
		}

		// Get entities in radius.
		numEntities = G_EntitiesInRadius(temp, radius, ignore, entities);

		// If some entities are visible.
		if (numEntities)
		{
			break;
		}
	}

	// If we didn't find any entities.
	if (!numEntities)
	{
		return qfalse;
	}

	// Damage entities.
	for (i = 0; i < numEntities; i++)
	{
		// Set entity pointer.
		entity = &g_entities[entities[i]];

		// Find distance from the edge of the bounding box.
		for (j = 0; j < 3; j++)
		{
			if (origin[j] < entity->r.absmin[j])
			{
				temp[j] = entity->r.absmin[j] - origin[j];
			}
			else if (origin[j] > entity->r.absmax[j])
			{
				temp[j] = origin[j] - entity->r.absmax[j];
			}
			else
			{
				temp[j] = 0;
			}
		}

		distance = VectorLength(temp);

		// Do a last within radius check.
		if (distance >= radius)
		{
			continue;
		}

		// Calculate damage points.
		points = damage * (float)(1.0f + sin(M_PI + ((M_PI / 2) * (distance / radius))));

		// If this is a client and he's crouching.
		if (entity->client && (entity->client->ps.pm_flags & PMF_DUCKED))
		{
			// Reduce damage points.
			points *= 0.8f;

			// Clamp.
			if (points < 1)
			{
				points = 1;
			}
		}

		// Calculate direction vector.
		VectorSubtract(entity->r.currentOrigin, origin, temp);

		// Damage entity.
		G_Damage(entity, NULL, attacker, temp, origin, (int)points, DAMAGE_RADIUS, mod, HL_UNKNOWN);
	}

	return qfalse;
}
