// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

/*

  Items are any object that a player can touch to gain some effect.

  Pickup will return the number of seconds until they should respawn.

  all items should pop when dropped in lava or slime

  Respawnable items don't actually go away when picked up, they are
  just made invisible and untouchable.	This allows them to ride
  movers and respawn apropriately.
*/

#define RESPAWN_ARMOR	   25
#define RESPAWN_HEALTH	   35
#define RESPAWN_AMMO	   40
#define RESPAWN_HOLDABLE   60
#define RESPAWN_MEGAHEALTH 35	  //120
#define RESPAWN_POWERUP    120

/**
 * $(function)
 */
int Pickup_Holdable( gentity_t *ent, gentity_t *other )
{
	int item = ent->item - bg_itemlist;
	utPSGiveItem ( &other->client->ps, item );
	if (item == UT_ITEM_LASER)
		other->client->laserSight = qtrue;
	return -1;
}

//======================================================================

void Add_Ammo (gentity_t *ent, int weapon, int clips, int bullets )
{
	int  mul;
	int  weaponSlot = utPSGetWeaponByID ( &ent->client->ps, weapon );

	if(-1 == weaponSlot)
	{
		return;
	}

	switch (weapon)
	{
		case UT_WP_GRENADE_HE:
		case UT_WP_GRENADE_FLASH:
		case UT_WP_GRENADE_SMOKE:

		case UT_WP_KNIFE:
		case UT_WP_BOMB:
			mul = 1;
			break;

		default:
			mul = 2;
			break;
	}

	if (clips)
	{
		if((int)UT_WEAPON_GETCLIPS( ent->client->ps.weaponData[weaponSlot] ) + clips > bg_weaponlist[weapon].clips * mul)
		{
			UT_WEAPON_SETCLIPS ( ent->client->ps.weaponData[weaponSlot], bg_weaponlist[weapon].clips * mul );
		}
		else
		{
			UT_WEAPON_SETCLIPS ( ent->client->ps.weaponData[weaponSlot], UT_WEAPON_GETCLIPS( ent->client->ps.weaponData[weaponSlot] ) + clips );
		}
	}

	if (bullets)
	{
		if((int)UT_WEAPON_GETBULLETS( ent->client->ps.weaponData[weaponSlot] ) + bullets > bg_weaponlist[weapon].bullets)
		{
			UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[weaponSlot], bg_weaponlist[weapon].bullets );
		}
		else
		{
			UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[weaponSlot], UT_WEAPON_GETBULLETS( ent->client->ps.weaponData[weaponSlot] ) + bullets );
		}
	}
}

/**
 * $(function)
 */
void Set_Ammo (gentity_t *ent, int weapon, int clips, int bullets )
{
	int  mul;
	int  weaponSlot = utPSGetWeaponByID ( &ent->client->ps, weapon );

	if(-1 == weaponSlot)
	{
		return;
	}

	switch (weapon)
	{
		case UT_WP_GRENADE_HE:
		case UT_WP_GRENADE_FLASH:
		case UT_WP_GRENADE_SMOKE:
		case UT_WP_KNIFE:
		case UT_WP_BOMB:
			mul = 1;
			break;

		default:
			mul = 2;
			break;
	}

	// Cant pick up more than max clips
	if (clips > bg_weaponlist[weapon].clips * mul)
	{
		clips = bg_weaponlist[weapon].clips * mul;
	}

	if (bullets > bg_weaponlist[weapon].bullets)
	{
		bullets = bg_weaponlist[weapon].bullets;
	}

	UT_WEAPON_SETCLIPS ( ent->client->ps.weaponData[weaponSlot], clips );
	UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[weaponSlot], bullets );
}

//======================================================================

int Pickup_Weapon (gentity_t *ent, gentity_t *other)
{
	qboolean  stripAmmo = qfalse;
	int 	  clips 	= 0;
	int 	  bullets	= 0;

	// do we have this item??
	if (-1 != (utPSGetWeaponByID ( &other->client->ps, ent->item->giTag )))
	{
		stripAmmo = qtrue;
	}

	// See how much ammo it has
	clips	= (ent->count >> 8);
	bullets = (ent->count & 0xFF);

	// If we already have the weapon then just strip the gun of its bullets
	if(ent->item->giTag == UT_WP_KNIFE)
	{
		clips	= 0;
		bullets = 1;
	}
	else if(ent->item->giTag == UT_WP_BOMB)
	{
		clips	= 0;
		bullets = 1;
	}
	else
	{
		//#27 fix for grens ammo
		if ((ent->item->giTag != UT_WP_GRENADE_HE) && (ent->item->giTag != UT_WP_GRENADE_FLASH) && (ent->item->giTag != UT_WP_GRENADE_SMOKE))
		{
			if (stripAmmo)
			{
				bullets = 0;
			}
		}

		if(ent->item->giTag == UT_WP_BOMB)
		{
			bullets   = 1;
			clips	  = 0;
			stripAmmo = qfalse;
		}
	}

	if (!stripAmmo)
	{
		// Give the player the new weapon
		utPSGiveWeapon ( &other->client->ps, ent->item->giTag, other->client->pers.weaponModes[ent->item->giTag] - '0' );
		Set_Ammo( other, ent->item->giTag, clips, bullets );
	}
	else
	{
		// now add the clips and bullets
		Add_Ammo( other, ent->item->giTag, clips, bullets );
		ent->count = bullets;
	}

	return -1;
}

/*
===============
RespawnItem
===============
*/
void RespawnItem( gentity_t *ent )
{
	// randomly select from teamed entities
	if (ent->team)
	{
		gentity_t  *master;
		int    count;
		int    choice;

		if (!ent->teammaster)
		{
			G_Error( "RespawnItem: bad teammaster");
		}
		master = ent->teammaster;

		for (count = 0, ent = master; ent; ent = ent->teamchain, count++)
		{
			;
		}

		choice = rand() % count;

		for (count = 0, ent = master; count < choice; ent = ent->teamchain, count++)
		{
			;
		}
	}

	ent->r.contents = CONTENTS_TRIGGER;
	ent->flags	   |= FL_KILLNEXTROUND;
	ent->s.eFlags  &= ~EF_NODRAW & ~EF_NOPICKUP;
	ent->r.svFlags &= ~SVF_NOCLIENT;
	trap_LinkEntity (ent);

	if (ent->item->giType == IT_POWERUP)
	{
		// play powerup spawn sound to all clients
		gentity_t  *te;

		// if the powerup respawn sound should Not be global
		if (ent->speed)
		{
			te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
		}
		else
		{
			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		}
		te->s.eventParm = G_SoundIndex( "sound/items/poweruprespawn.wav" );
		te->r.svFlags  |= SVF_BROADCAST;
	}

	if ((ent->item->giType == IT_HOLDABLE) && (ent->item->giTag == HI_KAMIKAZE))
	{
		// play powerup spawn sound to all clients
		gentity_t  *te;

		// if the powerup respawn sound should Not be global
		if (ent->speed)
		{
			te = G_TempEntity( ent->s.pos.trBase, EV_GENERAL_SOUND );
		}
		else
		{
			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		}
		te->s.eventParm = G_SoundIndex( "sound/items/kamikazerespawn.wav" );
		te->r.svFlags  |= SVF_BROADCAST;
	}

	// play the normal respawn sound only to nearby clients
	G_AddEvent( ent, EV_ITEM_RESPAWN, 0 );

	ent->nextthink = 0;
}

/**
 * Touch_Item
 *
 * Touch an item, but only pickup straight away if autoPickup is enabled
 */
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace)
{
	Pickup_Item (ent, other, trace, other->client->pers.autoPickup);
}

/**
 * Pickup_Item
 *
 * Pickup an item but do a few checks before to see whether this is indeed valid
 */
void Pickup_Item (gentity_t *ent, gentity_t *other, trace_t *trace, int autoPickup)
{
	int respawn;

	// need a client and dead people can't pickup
	if (!other->client || other->health < 1)
	{
		return;
	}

	if(ent->s.eFlags & EF_NOPICKUP)
	{
		return;
	}

	// owner can't pickup his own items straight away
	if((ent->lastOwner == other) && (ent->lastOwnerThink > level.time))
	{
		return;
	}

	// not allowed to pick up weapons while reloading
	if ((ent->item->giType == IT_WEAPON) && (other->client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_RELOADING))
	{
		return;
	}

	// the same pickup rules are used for client side and server side
	if (!BG_CanItemBeGrabbed( g_gametype.integer, &ent->s, &other->client->ps ))
	{
		return;
	}

	if(g_gametype.integer == GT_UT_BOMB &&
		(other->client->sess.sessionTeam != TEAM_RED) && (ent->item->giTag == UT_WP_BOMB))
	{
		return;
	}

	//Somebodys grabbed the bomb
	if (ent->item->giTag == UT_WP_BOMB)
	{
		level.BombHolderLastRound = other->client->ps.clientNum;
		G_LogPrintf("Bomb has been collected by %i\n", other->client->ps.clientNum);
		level.UseLastRoundBomber  = qtrue;

		//@Barbatos: be sure we are playing bomb mode
		if(g_gametype.integer == GT_UT_BOMB)
			G_AutoRadio(3, other);

		// we have to pickup bombs when we walk over
		autoPickup = -1;
	}

	// call the item-specific pickup function
	switch(ent->item->giType)
	{
		case IT_WEAPON:
			if (autoPickup & (1 << (ent->item - bg_itemlist)))
			{
				respawn = Pickup_Weapon(ent, other);
			}
			else
			{
				return;
			}
			break;

		case IT_TEAM:
			respawn = Pickup_Team(ent, other);
			break;

		case IT_HOLDABLE:
			// No picking up an item twice.
			if ((autoPickup & (1 << (ent->item - bg_itemlist))) && !utPSHasItem(&other->client->ps, bg_itemlist - ent->item))
			{
				respawn = Pickup_Holdable(ent, other);
			}
			else
			{
				return;
			}

			break;

		default:
			return;
	}

	if (!respawn)
	{
		return;
	}

	G_LogPrintf( "Item: %i %s\n", other->s.number, ent->item->classname );

	// play the normal pickup sound
	G_AddPredictableEvent( other, EV_ITEM_PICKUP, ent->s.modelindex );

	// powerup pickups are global broadcasts
	if ((ent->item->giType == IT_POWERUP) || (ent->item->giType == IT_TEAM))
	{
		char powerupcmd[32];

		// if we want the global sound to play
		if (!ent->speed)
		{
			gentity_t  *te;

			te		= G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm = ent->s.modelindex;
			te->r.svFlags  |= SVF_BROADCAST;
		}
		else
		{
			gentity_t  *te;

			te		   = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_ITEM_PICKUP );
			te->s.eventParm    = ent->s.modelindex;
			// only send this temp entity to a single client
			te->r.svFlags	  |= SVF_SINGLECLIENT;
			te->r.singleClient = other->s.number;
		}
	/***
		s.e.t.i. : Use global broadast to generate a cg_followpowerup command
			Gametypes:
				* CTF : Flag has been grabbed by other->client->ps.clientNum
				* Bomb : Bomb has been picked up by other->client->ps.clientNum
	***/
		Com_sprintf(powerupcmd, 32, "followpowerup %i", other->client->ps.clientNum);

		//trap_Argv(1, clientnum, sizeof(char)*2);
		// Send to client -1, which broadcasts to all clients
		trap_SendServerCommand(-1, powerupcmd);
	}

	// fire item targets
	G_UseTargets (ent, other);

	// wait of -1 will not respawn
	if (ent->wait == -1)
	{
		ent->r.svFlags		 |= SVF_NOCLIENT;
		ent->s.eFlags		 |= EF_NODRAW | EF_NOPICKUP;
		ent->r.contents 	  = 0;
		ent->unlinkAfterEvent = qtrue;
		return;
	}

	// non zero wait overrides respawn time
	if (ent->wait)
	{
		respawn = ent->wait;
	}

	// random can be used to vary the respawn time
	if (ent->random)
	{
		respawn += crandom() * ent->random;

		if (respawn < 1)
		{
			respawn = 1;
		}
	}

	// dropped items will not respawn
	if (ent->flags & FL_DROPPED_ITEM)
	{
		ent->freeAfterEvent = qtrue;
	}

	// picked up items still stay around, they just don't
	// draw anything.  This allows respawnable items
	// to be placed on movers.
	ent->r.svFlags |= SVF_NOCLIENT;
	ent->s.eFlags  |= EF_NODRAW | EF_NOPICKUP;
	ent->r.contents = 0;

	// ZOID
	// A negative respawn times means to never respawn this item (but don't
	// delete it).	This is used by items that are respawned by third party
	// events such as ctf flags
	if (respawn <= 0)
	{
		ent->nextthink = 0;
		ent->think	   = 0;
	}
	else
	{
		ent->nextthink = level.time + respawn * 1000;
		ent->think	   = RespawnItem;
	}
	trap_LinkEntity( ent );
}

//======================================================================

/*
================
LaunchItem

Spawns an item and tosses it forward
================
*/
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity, gentity_t *owner, int noPickupTime )
{
	gentity_t  *dropped;

	dropped 		   = G_Spawn();

	dropped->s.eType	   = ET_ITEM;
	dropped->s.modelindex  = item - bg_itemlist;	// store item number in modelindex
	dropped->s.modelindex2 = 1; 		// This is non-zero is it's a dropped item

	dropped->classname	   = item->classname;
	HASHSTR(item->classname,dropped->classhash);
	dropped->item		   = item;
	VectorSet (dropped->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS);
	VectorSet (dropped->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS);
	dropped->r.contents = CONTENTS_TRIGGER;

	dropped->touch		= Touch_Item;

	G_SetOrigin( dropped, origin );
	dropped->s.pos.trType = TR_GRAVITY;
	dropped->s.pos.trTime = level.time;
	VectorCopy( velocity, dropped->s.pos.trDelta );

	dropped->s.eFlags |= EF_BOUNCE_HALF;

	// Special case for CTF flags
	// @Fenix - check also for flags in JUMP mode
	if (((g_gametype.integer == GT_CTF) || (g_gametype.integer == GT_JUMP)) && (item->giType == IT_TEAM))
	{
		dropped->think	   = Team_DroppedFlagThink;
		dropped->nextthink = level.time + (g_flagReturnTime.integer * 1000);
		Team_CheckDroppedItem( dropped );
	}
	// Don't clear items in survivor modes.
	else if (g_survivor.integer)
	{
		if (level.warmupTime) { // @r00t: Fix for exploit - repeat(drop items,die) during warmup to crash server
			dropped->think	   = G_FreeEntity;
			dropped->nextthink = level.time + 30000;
		} else {
			dropped->think	   = 0;
			dropped->nextthink = 0;
		}
		dropped->flags	  |= FL_KILLNEXTROUND;
	}
	else	 // auto-remove after 30 seconds
	{
		dropped->think	   = G_FreeEntity;
		dropped->nextthink = level.time + 30000;
	}

	// Don't let the owner pick it up straight away
	dropped->lastOwner	= owner;
	dropped->lastOwnerThink = level.time + noPickupTime;
	dropped->flags = FL_DROPPED_ITEM;

	trap_LinkEntity (dropped);

	return dropped;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : Drop_Item
// Description: Drops the given item from the clients inventory
// Changlist  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle, qboolean makeent)
{
	vec3_t	   velocity;
	vec3_t	   angles;
	int 	   itemSlot;
	gentity_t  *dropped = NULL;

	// Find the item being dropped
	if (-1 == (itemSlot = utPSGetItemByID ( &ent->client->ps, item - &bg_itemlist[0] )))
	{
		return NULL;
	}

	// If they have dropped something then they can no longer change their gear
	ent->client->noGearChange = qtrue;

	// always forward
	VectorCopy( ent->s.apos.trBase, angles );
	angles[YAW]  += angle;
	angles[PITCH] = 0;

	AngleVectorsF( angles, velocity, NULL, NULL );
	VectorScale( velocity, 150, velocity );
	velocity[2] += 200 + crandom() * 50;

	// actually drop it
	if (makeent)
	{
		dropped = LaunchItem( item, ent->s.pos.trBase, velocity, ent, 1500 );
	}

	// Remove the item from our inventory
	UT_ITEM_SETID ( ent->client->ps.itemData[itemSlot], 0 );

	// IF it was a flag then send the message that the flag was dropped
	if ((item - &bg_itemlist[0] == UT_ITEM_REDFLAG) || (item - &bg_itemlist[0] == UT_ITEM_BLUEFLAG))
	{
		const char	*FlagName;

		dropped->r.svFlags |= SVF_BROADCAST;

		if(ent->client && ent->client->maparrow)
		{
			ent->client->maparrow->nextthink = level.time;
			//	ent->mapicon->nextthink = level.time + ( ( ent->s.number % 10 ) * 100 );
		}

		//@Fenix - workaround for correct flag name display since JUMP mode is not a team based gametype.
		//This prevent of showing "player dropped the FREE flag" while using blue and red flags in JUMP mode.
		//CTF behavior remains unchanged.
		//trap_SendServerCommand( -1, va("cp \"%s%s" S_COLOR_WHITE " dropped the %s" S_COLOR_WHITE " flag!\"", TeamColorString ( ent->client->sess.sessionTeam), ent->client->pers.netname, ColoredTeamName(OtherTeam(ent->client->sess.sessionTeam))));

		// Log print out - DensitY
		if(item - &bg_itemlist[0] == UT_ITEM_REDFLAG) {
			trap_SendServerCommand(-1, va("ccprint \"%d\" \"%d\" \"%s\" \"%d\"", CCPRINT_FLAG_DROPPED, ent->client->sess.sessionTeam, ent->client->pers.netname, 1));
			//trap_SendServerCommand(-1, va("cp \"%s%s" S_COLOR_WHITE " dropped the " S_COLOR_RED "Red" S_COLOR_WHITE " flag!\"", TeamColorString(ent->client->sess.sessionTeam), ent->client->pers.netname));
			FlagName = va( "team_CTF_redflag" );
		}
		else if(item - &bg_itemlist[0] == UT_ITEM_BLUEFLAG) {
			trap_SendServerCommand(-1, va("ccprint \"%d\" \"%d\" \"%s\" \"%d\"", CCPRINT_FLAG_DROPPED, ent->client->sess.sessionTeam, ent->client->pers.netname, 2));
			//trap_SendServerCommand(-1, va("cp \"%s%s" S_COLOR_WHITE " dropped the " S_COLOR_BLUE "Blue" S_COLOR_WHITE " flag!\"", TeamColorString(ent->client->sess.sessionTeam), ent->client->pers.netname));
			FlagName = va( "team_CTF_blueflag" );
		}
		else {
			trap_SendServerCommand(-1, va("ccprint \"%d\" \"%d\" \"%s\" \"%d\"", CCPRINT_FLAG_DROPPED, ent->client->sess.sessionTeam, ent->client->pers.netname, 0));
			//trap_SendServerCommand(-1, va("cp \"%s%s" S_COLOR_WHITE " dropped the " S_COLOR_WHITE "FREE" S_COLOR_WHITE " flag!\"", TeamColorString(ent->client->sess.sessionTeam), ent->client->pers.netname));
			FlagName = va( "team_CTF_neutralflag" );
		}

		G_LogPrintf( "Flag: %i 0: %s\n", ent->s.number, FlagName );
	}
	else
	{
		// Unlink item after being picked up
		dropped->wait		= -1;
	}

	return dropped;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name 	  : Drop_Weapon
// Description: Drops the given weapon from the clients inventory
// Changlist  : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
gentity_t *Drop_Weapon( gentity_t *ent, int weaponid, float angle )
{
	vec3_t	   velocity;
	vec3_t	   angles;
	int 	   weaponSlot;
	gentity_t  *dropped;

	// Cant drop the knife
	if (weaponid == UT_WP_KNIFE)
	{
		return NULL;
	}

	// Find the item being dropped
	if (-1 == (weaponSlot = utPSGetWeaponByID ( &ent->client->ps, weaponid )))
	{
		return NULL;
	}

	// If they have dropped something then they can no longer change their gear
	ent->client->noGearChange = qtrue;

	// always forward
	VectorCopy( ent->s.apos.trBase, angles );
	angles[YAW]  += angle;
	angles[PITCH] = 0;

	AngleVectorsF( angles, velocity, NULL, NULL );
	VectorScale( velocity, 150, velocity );
	velocity[2] += 200 + crandom() * 50;

	// actually drop it
	dropped = LaunchItem( BG_FindItemForWeapon ( weaponid ), ent->s.pos.trBase, velocity, ent, 2000 );

	if(weaponid == UT_WP_BOMB) // Broadcast so this will show up in the minimap
	{
		dropped->r.svFlags |= SVF_BROADCAST;
		G_LogPrintf("Bomb was tossed by %i\n", ent->client->ps.clientNum);
	}

	// Set the ammo thats on the weapon
	dropped->count = (UT_WEAPON_GETCLIPS(ent->client->ps.weaponData[weaponSlot]) << 8) +
			 UT_WEAPON_GETBULLETS(ent->client->ps.weaponData[weaponSlot]);

	// Unlink entity after being picked up
	dropped->wait = -1;

	// Remove the weapon from our inventory
	UT_WEAPON_SETID ( ent->client->ps.weaponData[weaponSlot], 0 );
	UT_WEAPON_SETBULLETS ( ent->client->ps.weaponData[weaponSlot], 0 );
	UT_WEAPON_SETCLIPS	( ent->client->ps.weaponData[weaponSlot], 0 );

	return dropped;
}

/*
================
Use_Item

Respawn the item
================
*/
void Use_Item( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	RespawnItem( ent );
}

//======================================================================

/*
================
FinishSpawningItem

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
void FinishSpawningItem( gentity_t *ent )
{
	trace_t  tr;
	vec3_t	 dest;

	VectorSet( ent->r.mins, -ITEM_RADIUS, -ITEM_RADIUS, -ITEM_RADIUS );
	VectorSet( ent->r.maxs, ITEM_RADIUS, ITEM_RADIUS, ITEM_RADIUS );

	ent->s.eType	   = ET_ITEM;
	ent->s.modelindex  = ent->item - bg_itemlist;		// store item number in modelindex
	ent->s.modelindex2 = 0; 				// zero indicates this isn't a dropped item

	ent->r.contents    = CONTENTS_TRIGGER;
	ent->touch	   = Touch_Item;
	// useing an item causes it to respawn
	ent->use	   = Use_Item;

	if (ent->spawnflags & 1)
	{
		// suspended
		G_SetOrigin( ent, ent->s.origin );
	}
	else
	{
		// drop to floor
		VectorSet( dest, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2] - 4096 );
		trap_Trace( &tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest, ent->s.number, MASK_SOLID );

		if (tr.startsolid)
		{
			G_Printf ("FinishSpawningItem: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
			G_FreeEntity( ent );
			return;
		}

		// allow to ride movers
		ent->s.groundEntityNum = tr.entityNum;

		G_SetOrigin( ent, tr.endpos );
	}

	// team slaves and targeted items aren't present at start
	if ((ent->flags & FL_TEAMSLAVE) || ent->targetname)
	{
		ent->s.eFlags  |= EF_NODRAW;
		ent->r.contents = 0;
		return;
	}

	// powerups don't spawn in for a while
	if (ent->item->giType == IT_POWERUP)
	{
		float  respawn;

		respawn 	= 45 + crandom() * 15;
		ent->s.eFlags  |= EF_NODRAW;
		ent->r.contents = 0;
		ent->nextthink	= level.time + respawn * 1000;
		ent->think	= RespawnItem;
		return;
	}

	trap_LinkEntity (ent);
}

qboolean  itemRegistered[MAX_ITEMS];

/*
==================
G_CheckTeamItems
==================
*/
void G_CheckTeamItems( void )
{
	Team_InitGame();

	// Set up team stuff
	// @Fenix - we are not checking flags for JUMP mode
	// several third party jump maps doesn't have flags.
	if(g_gametype.integer == GT_CTF)
	{
		gitem_t  *item;
		// check for the two flags
		item = BG_FindItem( "Red Flag" );

		if (!item || !itemRegistered[item - bg_itemlist])
		{
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map" );
		}
		item = BG_FindItem( "Blue Flag" );

		if (!item || !itemRegistered[item - bg_itemlist])
		{
			G_Printf( S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map" );
		}
	}

	//ok
}

/*
==============
ClearRegisteredItems
==============
*/
void ClearRegisteredItems( void )
{
	memset( itemRegistered, 0, sizeof(itemRegistered));
}

/*
===============
RegisterItem

The item will be added to the precache list
===============
*/
void RegisterItem( gitem_t *item )
{
	if (!item)
	{
		G_Error( "RegisterItem: NULL" );
	}
	itemRegistered[item - bg_itemlist] = qtrue;
}

/*
===============
SaveRegisteredItems

Write the needed items to a config string
so the client will know which ones to precache
===============
*/
void SaveRegisteredItems( void )
{
/*	char	string[MAX_ITEMS+1];
	int 	i;
	int 	count;

	count = 0;
	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( itemRegistered[i] ) {
			count++;
			string[i] = '1';
		} else {
			string[i] = '0';
		}
	}
	string[ bg_numItems ] = 0;

	G_Printf( "%i items registered\n", count );
	trap_SetConfigstring(CS_ITEMS, string);*/
}

/*
============
G_ItemDisabled
============
*/
int G_ItemDisabled( gitem_t *item )
{
	char  name[128];

	Com_sprintf(name, sizeof(name), "disable_%s", item->classname);
	return trap_Cvar_VariableIntegerValue( name );
}

/*
============
G_SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void G_SpawnItem (gentity_t *ent, gitem_t *item)
{
	G_SpawnFloat( "random", "0", &ent->random );
	G_SpawnFloat( "wait", "0", &ent->wait );

	RegisterItem( item );

	if (G_ItemDisabled(item))
	{
		return;
	}

	ent->item = item;
	// some movers spawn on the second frame, so delay item
	// spawns until the third frame so they can ride trains
	ent->nextthink	   = level.time + FRAMETIME * 2;
	ent->think		   = FinishSpawningItem;

	ent->physicsBounce = 0.50;		// items are bouncy

	if (item->giType == IT_POWERUP)
	{
		G_SoundIndex( "sound/items/poweruprespawn.wav" );
		G_SpawnFloat( "noglobalsound", "0", &ent->speed);
	}

#ifdef MISSIONPACK
	if (item->giType == IT_PERSISTANT_POWERUP)
	{
		ent->s.generic1 = ent->spawnflags;
	}
#endif
}

/*
================
G_BounceItem

================
*/
void G_BounceItem( gentity_t *ent, trace_t *trace )
{
	vec3_t	velocity;
	float	dot;
	int hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + (level.time - level.previousTime) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

	// cut the velocity to keep from bouncing forever
	VectorScale( ent->s.pos.trDelta, ent->physicsBounce, ent->s.pos.trDelta );

	// check for stop
	if ((trace->plane.normal[2] > 0) && (ent->s.pos.trDelta[2] < 40))
	{
		trace->endpos[2] += 1.0;	// make sure it is off ground
		SnapVector( trace->endpos );
		G_SetOrigin( ent, trace->endpos );
		ent->s.groundEntityNum = trace->entityNum;
		return;
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}

/*
================
G_RunItem

================
*/
void G_RunItem( gentity_t *ent )
{
	vec3_t	 origin;
	trace_t  tr;
	int 	 contents;
	int 	 mask;

	// if groundentity has been set to -1, it may have been pushed off an edge
	if (ent->s.groundEntityNum == -1)
	{
		if (ent->s.pos.trType != TR_GRAVITY)
		{
			ent->s.pos.trType = TR_GRAVITY;
			ent->s.pos.trTime = level.time;
		}
	}

	if (ent->s.pos.trType == TR_STATIONARY)
	{
		// check think function
		G_RunThink( ent );
		return;
	}

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// trace a line from the previous position to the current position
	if (ent->clipmask)
	{
		mask = ent->clipmask;
	}
	else
	{
		mask = MASK_PLAYERSOLID & ~CONTENTS_BODY; //MASK_SOLID;
	}
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin,
			ent->r.ownerNum, mask );

	VectorCopy( tr.endpos, ent->r.currentOrigin );

	if (tr.startsolid)
	{
		tr.fraction = 0;
	}

	trap_LinkEntity( ent ); // FIXME: avoid this for stationary?

	// check think function
	G_RunThink( ent );

	if (tr.fraction == 1)
	{
		return;
	}

	// if it is in a nodrop volume, remove it
	contents = trap_PointContents( ent->r.currentOrigin, -1 );

	if (contents & CONTENTS_NODROP)
	{
		if (ent->item && (ent->item->giType == IT_TEAM)) //If its the/a flag
		{
			Team_FreeEntity(ent);
		}
		else
		{
			G_FreeEntity( ent );
		}
		return;
	}

	G_BounceItem( ent, &tr );
}
