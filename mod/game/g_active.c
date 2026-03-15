/**
 * Filename: g_active.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "g_local.h"
#include "animdef.h"

int last_hax_report_time = 0; //@r00t: Hax reporting

static qboolean G_IsStuckToClient(gentity_t *ent, gentity_t *ent2);

void		G_AnimatePlayer(gentity_t *ent);
void		ClientThink_BombDefuse(gentity_t *ent);

/**
 * G_SetGhostMode
 *
 * Sets the client to Ghost mode
 */
void G_SetGhostMode(gentity_t *ent)
{
	int  i;

	if (!ent->client) {
		return;
	}
	ent->client->ghost						= qtrue;
	ent->client->ps.stats[STAT_HEALTH]		= 0;
	ent->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_GHOST;
	ent->s.eFlags				  |= EF_DEAD;
	ent->client->sess.teamLeader	   = qfalse;
	ent->client->savedclientNum 	   = ent->client->ps.clientNum;

	if (ent->client->ps.clientNum == ent - g_entities) {
		ent->client->savedping = ent->client->ps.ping;
	}

	/* Do not use the client until he respawns */
	for (i = 0; i < MAX_PERSISTANT; i++) {
		ent->client->savedpersistant[i] = ent->client->ps.persistant[i];
	}

	/* Unlink player from collision engine. */
	trap_UnlinkEntity(ent);
}

/**
 * G_DamageFeedback
 *
 * Called just before a snapshot is sent to the given player.
 * Totals up all damage and generates both the player_state_t
 * damage values to that client for pain blends and kicks, and
 * global pain sound events for all clients.
 */
void P_DamageFeedback(gentity_t *player)
{
	gclient_t  *client;
	float	   count;
	vec3_t	   angles;

	client = player->client;

	if (client->ps.pm_type == PM_DEAD) {
		return;
	}

	/* Total points of damage shot at the player this frame */
	count = client->damage_blood + client->damage_armor;

	/* Return if the client took no damage */
	if (count == 0) {
		return;
	}

	if (count > 255) {
		count = 255;
	}

	/* Send the information to the client */

	/* World damage (falling, slime, etc) uses a special code */
	/* to make the blend blob centered instead of positional */
	if (client->damage_fromWorld) {
		client->ps.damagePitch	 = 255;
		client->ps.damageYaw	 = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles(client->damage_from, angles);
		client->ps.damagePitch = angles[PITCH] / 360.0 * 256;
		client->ps.damageYaw   = angles[YAW] / 360.0 * 256;
	}

	/* Play an apropriate pain sound */
	if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE)) {
		int  h = player->health;

		/* Only make bleed noise every 3 seconds */
		if (count == client->damage_armor) {
			if (level.time - player->pain_bleed_time > 3000) {
				player->pain_bleed_time = level.time;
				h |= 0x80;
			} else {
				client->damage_blood	 = 0;
				client->damage_armor	 = 0;
				client->damage_knockback = 0;
				return;
			}
		}

		player->pain_debounce_time = level.time + 700;

		G_AddEvent(player, EV_PAIN, h);

		client->ps.damageEvent++;
	}

	client->ps.damageCount = count;

	/* clear totals */
	client->damage_blood	 = 0;
	client->damage_armor	 = 0;
	client->damage_knockback = 0;
}

/**
 * P_WorldEffects
 *
 * Check for lava / slime contents and drowning
 */
void P_WorldEffects(gentity_t *ent)
{
	int   waterlevel;

	/* No clipping players do not drown */
	if (ent->client->noclip) {
		ent->client->airOutTime = level.time + DROWNTIME;
		return;
	}

	waterlevel = ent->waterlevel;

	/* Check for drowning */
	if (waterlevel == 3) {
		/* If out of air, start drowning */
		if (ent->client->airOutTime < level.time) {
			/* Drown! */
			ent->client->airOutTime += 1000;

			if (ent->health > 0) {
				/* Take more damage the longer underwater */
				ent->damage += 2;

				if (ent->damage > 15) {
					ent->damage = 15;
				}

				/*
				 * Play a gurp sound instead of a
				 * normal pain sound
				 */
				if (ent->health <= ent->damage) {
					G_Sound(ent, CHAN_VOICE,
						G_SoundIndex("*drown.wav"));
				} else if (rand() & 1) {
					G_Sound(ent, CHAN_VOICE,
						G_SoundIndex("sound/player/gurp1.wav"));
				} else {
					G_Sound(ent, CHAN_VOICE,
						G_SoundIndex("sound/player/gurp2.wav"));
				}

				/* Don't play a normal pain sound */
				ent->pain_debounce_time = level.time + 200;

				G_Damage(ent, NULL, NULL, NULL, NULL,
					 ent->damage, DAMAGE_NO_ARMOR,
					 MOD_WATER, HL_UNKNOWN);
			}
		}
	} else {
		ent->client->airOutTime = level.time + DROWNTIME;
		ent->damage 	= 2;
	}

	/*
	 * Check for sizzle damage
	 */
	if (waterlevel &&
			(ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		if ((ent->health > 0)
				&& (ent->pain_debounce_time <= level.time)) {
			if (ent->watertype & CONTENTS_LAVA) {
				G_Damage(ent, NULL, NULL, NULL, NULL,
					 30 * waterlevel, 0, MOD_LAVA,
					 HL_UNKNOWN);
			}

			if (ent->watertype & CONTENTS_SLIME) {
				G_Damage(ent, NULL, NULL, NULL, NULL,
					 10 * waterlevel, 0, MOD_SLIME,
					 HL_UNKNOWN);
			}
		}
	}
}

/**
 * G_Use
 *
 * Uses something
 */
void G_Use(gentity_t *ent) {

	static vec3_t  range = { 40, 40, 52 };

	int 	   i;
	int 	   num;
	int 	   touch[MAX_GENTITIES];
	gentity_t	   *hit;
	gentity_t	   *interfaceEnt;
	vec3_t		   mins;
	vec3_t		   maxs;
	qboolean	   anim  = qfalse;
	qboolean	   doingDoor = qfalse;
	trace_t 	   tr;
	vec3_t		   reach;
	gentity_t	   *other;
	vec3_t		   forward;
	vec3_t		   right;
	vec3_t		   up;
	vec3_t		   muzzle;

	// dokta8 - work out where player is and where they are facing
	AngleVectors (ent->client->ps.viewangles, forward, right, up);
	CalcMuzzlePoint ( ent, forward, right, up, muzzle );

	// has to be a meter away from the dude
	VectorMA(muzzle, 64, forward, reach);

	// dokta8 - Do a trace from player's muzzle to the object they are in front of
	trap_Trace(&tr, muzzle, NULL, NULL, reach, ent->s.number, MASK_SHOT | CONTENTS_TRIGGER);
	other = &g_entities[ tr.entityNum ];	//	dokta8 - get the first object in front of the player

	if (other) {

		if (other->item && (other->item->giType == IT_WEAPON || other->item->giType == IT_TEAM || other->item->giType == IT_HOLDABLE)) {
			//@Fenix - Fix cg_autoPickup 0 bug #392
			Pickup_Item(other, ent, &tr, -1);
			return;
		}

		if (other->classname) {
//			  if (!Q_stricmp(other->classname, "func_door")) {
			if (other->classhash==HSH_func_door) {
				doingDoor = qtrue;
			}
		}
	}

	VectorSubtract(ent->client->ps.origin, range, mins);
	VectorAdd(ent->client->ps.origin, range, maxs);

	num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	for (i = 0; i < num; i++) {

		hit = &g_entities[touch[i]];

		if (!hit->classname) {
			continue;
		}

//		  if (!Q_stricmp(hit->classname, "func_button")) {
		if (hit->classhash==HSH_func_button) {
			//Press a button
			hit->use(hit, hit, ent);
		}
//		  else if (!Q_stricmp(hit->classname, "func_door")) {
		else if (hit->classhash==HSH_func_door) {
			//Trigger_only is a flag set by the mapper
			//it prevents doors being opened directly by a player
			//ie: they have to use a button or another trigger
			if ((hit->mover->moverState != MOVER_1TO2) && (!hit->mover->trigger_only)) {
				hit->use(hit, hit, ent);
				anim = qtrue;
			}
		}
//		  else if (!Q_stricmp(hit->classname, "func_rotating_door")) {
		else if (hit->classhash==HSH_func_rotating_door) {
			if (!hit->mover->trigger_only) {
				hit->use(hit, hit, ent);
				anim = qtrue;
			}
		}
//		  else if (!Q_stricmp(hit->classname, "func_keyboard_interface") && !doingDoor) {
		else if (hit->classhash==HSH_func_keyboard_interface && !doingDoor) {
			hit->use(hit, hit, ent);
		}
//		  else if (!Q_stricmp(hit->classname, "func_ut_train") && !doingDoor && hit->interfaceEnt) {
		else if (hit->classhash==HSH_func_ut_train && !doingDoor && hit->interfaceEnt) {
			if(hit->interfaceEnt){
				if ((interfaceEnt = G_Find (NULL, FOFS(targetname), hit->interfaceEnt)) != NULL) {
					interfaceEnt->use(interfaceEnt, interfaceEnt, ent);
				}
			}
	   }
	}

	//Run "use" animation but only if a high priority anim is not already running
	if (anim && (ent->client->ps.torsoTimer <= 0)) {
		ent->client->ps.torsoAnim  = TORSO_USE;
		ent->client->ps.torsoTimer = 1500;
	}
}

/**
 * G_HealOther
 */
static qboolean G_HealOther(gentity_t *ent) {

	gentity_t  *other;
	vec3_t	   forward;
	vec3_t	   right;
	vec3_t	   up;
	vec3_t	   muzzle;
	vec3_t	   reach;
	trace_t    tr;
	int        maxHealth;
	int        addHealth;

	// dokta8 - work out where player is and where they are facing
		AngleVectors(ent->client->ps.viewangles, forward, right, up);
		CalcMuzzlePoint(ent, forward, right, up, muzzle );

		// has to be a meter away from the dude
		VectorMA(muzzle, 64, forward, reach);

		// dokta8 - Do a trace from player's muzzle to the object they are in front of
		trap_Trace(&tr, muzzle, NULL, NULL, reach, ent->s.number, MASK_SHOT);
		other = &g_entities[ tr.entityNum ];  // dokta8 - get the first object in front of the player

	// make sure it's a valid client
	if (!other->client) {
		return qfalse;
	}

	// Uhm, no healing dead people, we arent god
    if (other->health < 1) {
        return qfalse;
    }

		// If they are using the medkit then they heal faster and higher.
	if (utPSHasItem(&ent->client->ps, UT_ITEM_MEDKIT) || utPSHasItem(&other->client->ps, UT_ITEM_MEDKIT)) {
        maxHealth = UT_MAX_HEALTH * UT_MAX_HEAL_MEDKIT / 100;
        addHealth = UT_MAX_HEALTH * 15 / 100;
	} else {
        maxHealth = UT_MAX_HEALTH * UT_MAX_HEAL / 100;
        addHealth = UT_MAX_HEALTH * 15 / 100;
	}

	// Clear their limp and bleed flags
	other->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BLEEDING;
	other->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_LIMPING;
	other->client->ps.stats[STAT_AIRMOVE_FLAGS] &= ~UT_PMF_CRASHED;
	other->client->bleedRate			         = 0;
	other->client->bleedWeapon			         = UT_WP_NONE;
	other->client->bleedCount			         = 0;
	other->client->bleedCounter			         = 0;

	// clear the "need medic" icon
	other->client->ps.eFlags &= ~EF_UT_MEDIC;

	// See if we can heal him - @Barbatos: moved here for 4.2.002 - clear the limp and bleed flags anyway
	if (other->health >= maxHealth) {
		return qfalse;
	}

	// Add health
	other->health += addHealth;

	// Clamp health
	if (other->health > maxHealth) {
		other->health = maxHealth;
	}

	return qtrue;
}

/**
 * G_SetClientSound
 */
void G_SetClientSound(gentity_t *ent) {

	if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
		ent->client->ps.loopSound = level.snd_fry;
	} else {
		ent->client->ps.loopSound = 0;
	}

}

/**
 * ClientImpacts
 */
void ClientImpacts(gentity_t *ent, pmove_t *pm)
{
	int    i, j;
	trace_t    trace;
	gentity_t  *other;

	memset(&trace, 0, sizeof(trace));

	for (i = 0 ; i < pm->numtouch ; i++) {
		for (j = 0 ; j < i ; j++) {
			if (pm->touchents[j] == pm->touchents[i]) {
				break;
			}
		}

		if (j != i) {
			continue;	/* duplicated */
		}
		other = &g_entities[pm->touchents[i]];

		if ((ent->r.svFlags & SVF_BOT) && (ent->touch)) {
			ent->touch(ent, other, &trace);
		}

		if (!other->touch) {
			continue;
		}

		other->touch(other, ent, &trace);
	}
}

/**
 * G_TouchTriggers
 *
 * Find all trigger entities that ent's current position touches.
 * Spectators will only interact with teleporters.
 */
void	G_TouchTriggers(gentity_t *ent)
{
	int 	   i, num;
	int 	   touch[MAX_GENTITIES];
	gentity_t	   *hit;
	trace_t 	   trace;
	vec3_t		   mins, maxs;
	static vec3_t  range	   = { 40, 40, 52 };
	static vec3_t  biggerrange = { 40, 40, 100 };
	trace_t 	   tr;

	if (!ent->client) {
		return;
	}

	/* Dead clients don't activate triggers! */
	if (ent->client->ps.stats[STAT_HEALTH] <= 0) {
		return;
	}

	VectorSubtract(ent->client->ps.origin, range, mins);
	VectorAdd(ent->client->ps.origin, range, maxs);

	num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	/* Can't use ent->absmin, because that has a one unit pad */
	VectorAdd(ent->client->ps.origin, ent->r.mins, mins);
	VectorAdd(ent->client->ps.origin, ent->r.maxs, maxs);

	ent->client->ps.stats[STAT_UTPMOVE_FLAGS] &= (~UT_PMF_TEAMSITE);

	for (i = 0 ; i < num ; i++) {
		hit = &g_entities[touch[i]];

		/*
		 * There is a client nearby so we should see if we are
		 * stuck to it
		 */
		if ((hit != ent) && hit->client && !hit->client->stuckTo) {
			/* See if its stuck, if not then just continue */
			if (!G_IsStuckToClient(ent, hit)) {
				continue;
			}

			/*
			 * Ok, we are stuck, we need to flag this so we can
			 * walk through people until we are unstuck
			 */
			ent->client->stuckTo = hit;
			hit->client->stuckTo = ent;
		}

		if (!hit->touch && !ent->touch) {
			continue;
		}

		if (!(hit->r.contents & CONTENTS_TRIGGER)) {
			continue;
		}

		/*
		 * Ignore most entities if a spectator
		 */
		if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) ||
				ent->client->ghost) {
			if ((hit->s.eType != ET_TELEPORT_TRIGGER) &&
					/* this is ugly but adding a new ET_? type will */
					/* most likely cause network incompatibilities */
					(hit->touch != Touch_DoorTrigger)) {
				continue;
			}
		}

		/*
		 * Use seperate code for determining if an item is picked up
		 * so you don't have to actually contact its bounding box
		 */
		if (hit->s.eType == ET_ITEM) {
			if (!BG_PlayerTouchesItem(&ent->client->ps, &hit->s, level.time)) {
				continue;
			}
		} else {
			if (!trap_EntityContact(mins, maxs, hit)) {
				continue;
			}
		}

		memset(&trace, 0, sizeof(trace));

		if (hit->touch) {
			hit->touch(hit, ent, &trace);
		}

		if ((ent->r.svFlags & SVF_BOT) && (ent->touch)) {
			ent->touch(ent, hit, &trace);
		}
	}

	/* if we didn't touch a jump pad this pmove frame */
	if (ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent   = 0;
	}

	/* Check the CTF flags here */
	VectorSubtract(ent->client->ps.origin, biggerrange, mins);
	VectorAdd(ent->client->ps.origin, biggerrange, maxs);
	num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	for (i = 0 ; i < num ; i++) {
		hit = &g_entities[touch[i]];

//		  if ((strcmp(hit->classname, "team_CTF_redflag") == 0) ||
//				  (strcmp(hit->classname, "team_CTF_blueflag") == 0) ||
//								  (strcmp(hit->classname, "team_CTF_neutralflag") == 0)) {
		  if (hit->classhash==HSH_team_CTF_redflag ||
			  hit->classhash==HSH_team_CTF_blueflag ||
			  hit->classhash==HSH_team_CTF_neutralflag) {
			if (!hit->touch && !ent->touch) {
				continue;
			}

			if (!(hit->r.contents & CONTENTS_TRIGGER)) {
				continue;
			}

			/* ignore most entities if a spectator */
			if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) ||
					ent->client->ghost) {
				/* TODO: Add new entity type for door trigger */
				if ((hit->s.eType != ET_TELEPORT_TRIGGER) &&
						(hit->touch != Touch_DoorTrigger)) {
					continue;
				}
			}

			if (hit->touch) {
				trap_Trace(&tr, ent->client->ps.origin, NULL,
					   NULL, hit->s.pos.trBase, 0,
					   CONTENTS_SOLID);

				if (tr.fraction == 1) {
					hit->touch(hit, ent, &trace);
				}
			}
		}
	}
}

/**
 * SpectatorThink
 */
void SpectatorThink(gentity_t *ent, usercmd_t *ucmd)
{
	pmove_t    pm;
	gclient_t  *client;
	qboolean   deadTeam = qfalse;

	client = ent->client;

	/* Don't let bot's think when they spectate */
	if (ent->r.svFlags & SVF_BOT) {
		return;
	}

	/* See if every player on our team is dead */
	if ((client->sess.sessionTeam == TEAM_RED) ||
			(client->sess.sessionTeam == TEAM_BLUE)) {
		if (TeamLiveCount(-1, client->sess.sessionTeam) == 0) {
			deadTeam = qtrue;
		}
	}

	/* If we're a substitute */
	if (client->pers.substitute && g_followStrict.integer && !deadTeam) {
		int   spectatedClient;
		qboolean  validSpectatedClient = qtrue;

		/* Disable movment */
		ucmd->forwardmove = 0;
		ucmd->rightmove   = 0;
		ucmd->upmove	  = 0;

		/* Get spectator client */
		spectatedClient = client->sess.spectatorClient;

		/* If we're not in following mode */
		if (client->sess.spectatorState != SPECTATOR_FOLLOW) {
			validSpectatedClient = qfalse;
		} else {
			/* If the client we are spectating is invalid */
			if ((spectatedClient < 0) ||
					(spectatedClient >= MAX_CLIENTS)) {
				validSpectatedClient = qfalse;
			} else {
				/* If spectated client is not connected */
				if (level.clients[spectatedClient].pers.connected != CON_CONNECTED) {
					validSpectatedClient = qfalse;
				} else {
					/*
					 * If spectated client is a
					 * spectator, ghost, dead or crazy
					 */
					if ((level.clients[spectatedClient].sess.sessionTeam == TEAM_SPECTATOR) || level.clients[spectatedClient].ghost ||
							(level.clients[spectatedClient].ps.stats[STAT_HEALTH] <= 0) || (level.clients[spectatedClient].ps.pm_type != PM_NORMAL)) {
						validSpectatedClient = qfalse;
					} else {
						/* If spectated client is not on our team. */
						if (level.clients[spectatedClient].sess.sessionTeam != client->sess.sessionTeam) {
							validSpectatedClient = qfalse;
						}
					}
				}
			}
		}

		/* If we're spectating an invalid client */
		if (!validSpectatedClient) {
			/* Cycle clients */
			Cmd_FollowCycle_f(ent, ent->client->spectatorDirection);
		}
	}

	if (client->sess.spectatorState != SPECTATOR_FOLLOW) {
		if (!level.intermissiontime &&
				!level.survivorRoundRestartTime) {
			if (g_followStrict.integer &&
					(level.numConnectedClients > 1) &&
					(ent->client->sess.sessionTeam !=
					 TEAM_SPECTATOR) &&
					!deadTeam) {
				if (-1 != Cmd_FollowCycle_f(ent, 1)) {
					return;
				}
			}
		}

		client->ps.pm_type = PM_SPECTATOR;
		client->ps.speed   = 400; /* faster than normal */

		/* Set up for pmove - spectators fly through bodies */
		memset(&pm, 0, sizeof(pm));
		pm.ps		 = &client->ps;
		pm.cmd		 = *ucmd;
		pm.tracemask	 = MASK_DEADSOLID;
		pm.trace	 = trap_Trace;
		pm.pointcontents = trap_PointContents;
		pm.isBot	 = (ent->r.svFlags & SVF_BOT) ? qtrue : qfalse;
		pm.ariesModel	 = client->pers.ariesModel;

		if (!pm.isBot && client->ghost && client->pers.substitute &&
				!g_survivor.integer && g_followStrict.integer &&
				((client->sess.sessionTeam == TEAM_RED) ||
				 (client->sess.sessionTeam == TEAM_BLUE))) {
			pm.freeze = qtrue;
		} else {
			pm.freeze = qfalse;
		}

		/* Perform a pmove */
		Pmove(&pm);
		/* save results of pmove */
		VectorCopy(client->ps.origin, ent->s.origin);

		G_TouchTriggers(ent);
		trap_UnlinkEntity(ent);
	}

	client->oldbuttons = client->buttons;
	client->buttons    = ucmd->buttons;

	if (client->buttons & BUTTON_TALK) {
		client->ps.eFlags |= EF_TALK;
	} else {
		client->ps.eFlags &= ~EF_TALK;
	}

	if ((client->buttons & BUTTON_ATTACK) &&
			!(client->oldbuttons & BUTTON_ATTACK)) {
		/* attack button cycles through spectators */
		client->spectatorDirection = 1;
		Cmd_FollowCycle_f(ent, client->spectatorDirection);
	} else if ((client->buttons & BUTTON_RELOAD) &&
			!(client->oldbuttons & BUTTON_RELOAD)) {
		/*
		 * and reload button cycles through spectators in the
		 * opposite direction
		 */
		client->spectatorDirection = -1;
		Cmd_FollowCycle_f(ent, client->spectatorDirection);
	} else if (client->sess.spectatorState == SPECTATOR_FOLLOW &&
			level.clients[client->sess.spectatorClient].ghost) {
		/* switch if the client dies */
		Cmd_FollowCycle_f(ent, client->spectatorDirection);
	} else if ((client->buttons & BUTTON_USE) && !(client->oldbuttons & BUTTON_USE)) {
		/* If they press use then stop following */
		if ((client->sess.spectatorState == SPECTATOR_FOLLOW) &&
				(!client->pers.substitute ||
				 !g_followStrict.integer ||
				 deadTeam)) {

			if (client->ps.eFlags & EF_CHASE) {
				StopFollowing(ent, qfalse);
			} else {
				client->ps.eFlags |= EF_CHASE;
			}

		} else {
			Cmd_FollowCycle_f(ent, client->spectatorDirection);
		}
	}

	/* Check for inactivity timer, but never drop the local client of a non-dedicated server. */
	/*ClientInactivityTimer(client); */
	/* Specs don't get kicked after joining because they are invincible as ghosts */
	client->sess.inactivityTime = level.time + g_inactivity.integer * 1000;
}

/**
 * ClientInactivityTimer
 *
 * Returns qfalse if the client is dropped
 */
qboolean ClientInactivityTimer(gclient_t *client, int msec)
{
	//@Barbatos - don't check bots inactivity. They are VIP members
	// that can stay on the server until they make it crash :)
	if(g_entities[client->ps.clientNum].r.svFlags & SVF_BOT)
		return qtrue;

	if (!g_inactivity.integer) {
		/* give everyone some time, so if the operator sets g_inactivity during */
		/* gameplay, everyone isn't kicked */
		client->sess.inactivityTime = level.time + 60 * 1000;
		client->inactivityWarning	= qfalse;
	} else if (level.pauseState) {
		if (client->sess.inactivityTime <
				level.pauseTime + g_inactivity.integer * 1000) {
			client->sess.inactivityTime = level.pauseTime +
							  g_inactivity.integer * 1000;
		}
	} else if (client->pers.cmd.forwardmove ||
			client->pers.cmd.rightmove ||
			client->pers.cmd.upmove ||
			(client->pers.cmd.buttons & BUTTON_ATTACK)) {
		if (client->sess.inactivityTime <
				level.time + g_inactivity.integer * 1000) {
			client->sess.inactivityTime = level.time +
							  g_inactivity.integer * 1000;
		}
		client->inactivityWarning = qfalse;
	} else if (client->ps.stats[STAT_HEALTH] <= 0) {
		client->sess.inactivityTime += msec;
	} else if (!client->pers.localClient) {
		if (level.time > client->sess.inactivityTime) {
			if (g_inactivityAction.integer == 1) {
				SetTeam(&g_entities[ client->ps.clientNum ], "s", qfalse);
				trap_SendServerCommand(client - level.clients, "cp \"You were moved to spectator due to inactivity!\n\"");
			} else {
				trap_DropClient(client - level.clients,
						"Dropped due to inactivity");
			}

			return qfalse;
		}

		if ((level.time > client->sess.inactivityTime - 10000) &&
				!client->inactivityWarning) {
			client->inactivityWarning = qtrue;
			trap_SendServerCommand(client - level.clients,
						   "cp \"Ten seconds until inactivity drop!\n\"");
		}
	}
	return qtrue;
}

/**
 * ClientTimerActions
 *
 * Actions that happen once a second
 */
void ClientTimerActions( gentity_t *ent, int msec ) {

	int bleedRate;
	gclient_t  *client;

	client = ent->client;

	/*
	 * If they moved out of their spawn point then they can't change
	 * gear but give them a few seconds to account for movement due to
	 * spawn points on slopes or in the air
	 */
	if (!client->noGearChange && ((level.time - client->respawnTime) > 3000)) {
		vec3_t	vel;
		VectorCopy(client->ps.velocity, vel);
		vel[2] = 0;

		if (VectorLengthSquared(vel) > 100) {
			client->noGearChange = qtrue;
		}
	}

	/* If the laser is on, make sure of it */
	ent->client->laserSight = ((ent->s.powerups & POWERUP(PW_LASER)) != 0);

	client->bleedResidual += msec;

	/* Check the ban list and unban anyone whose time has expired */
	expireBans();

#ifdef USE_AUTH
// @r00t: Hax reporting
	if ((level.time-last_hax_report_time)>HAX_REPORT_INTERVAL) {
		SendHaxReport();
		last_hax_report_time = level.time;
	}
#endif

	/*
	 * Forget team kill transgressions after
	 * g_teamkillsForgetTime minutes
	 */
	if ((ent->breakaxis + (g_teamkillsForgetTime.integer * 1000)) < level.time) {
		ent->breakshards = 0;
	}

#if 0
	/* Handle the bleeding: bleed faster if there are multiple wounds */
	// @r00t: Bleed variables aren't exactly well named:
	//  >bleedRate = how long will medic take, decreased while meding, in seconds
	//  >bleedCount = number of hits (max 4), controls how often health is reduced by bleeding
	//  >bleedCounter = used to count 100ms ticks to bleed every N ticks
	//  >bleedResidual = counter to get 100ms ticks

	bleedRate = 100; //@r00t: fixed this to 100ms, bleed timing is controlled using code below
	while ( client->bleedResidual >= bleedRate ) {

		static const int bleednum[5] = { 9, 4, 3, 2, 1 }; // Urt 4.1.1 magic constants (bleed every Nth 100ms tick)
		int bcnt = client->bleedCount-1;
		int bc = (client->bleedCounter + 1) % bleednum[bcnt];
		client->bleedResidual -= bleedRate;
		client->bleedCounter = bc;

		/* Cause bleeding damage */
		if ((client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) && (client->ps.stats[STAT_HEALTH] > 0) && !bc) {
#if 0
			/*
			// NOTE: in the current code/settings, I am seeing a damage of 0 passed all the time for UT_MOD_BLED:
			// this happens because of the (int) casting etc.
			// this is true for the legs, the only thing that uses bleedRate, and all the other hit locations
			// this is true when you bleed from multiple locations as well (such as if hit with a SPA)
			// the code inside G_Damage sets a damage of 1 when 0 is passed though so you still bleed (e.g. forces a round up?)
			// @r00t: This code works well now, fixed the float rounding. But it's too fast.
			//        Can be used after changing the 0.25/1.0 constants below to something better.
			*/
			float damage = ent->client->bleedRate * (float)(UT_MAX_HEALTH) * (ent->client->bleedWeapon == UT_WP_HK69 ? 0.25f : 1.0f);
			G_Damage(ent, ent->bleedEnemy, ent->bleedEnemy, NULL, NULL, damage, 0, UT_MOD_BLED, ent->bleedHitLocation);
#else
			// more simple, same result
			G_Damage( ent, ent->bleedEnemy, ent->bleedEnemy, NULL, NULL, 1, 0, UT_MOD_BLED, ent->bleedHitLocation );
#endif
		}

		/* If this client is bandaging then bandage them already */
		if (client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BANDAGING) {

			/* Safety check */
			if ((client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) && (ent->client->bleedRate <= 0.0f)) {
				client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BLEEDING; //@r00t: never seen this executed. needed?
				client->bleedCount = 0;
				client->bleedRate = 0; //@r00t: these are important!
			}


			/*
			 * If bleeding then lower the bleed rate and if there is
			 * no more bleedrate just clear the bleeding flag
			 */
			if (/*(client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING) &&*/ (ent->client->bleedRate > 0.0f)) { //@r00t:crash landing doesn't bleed
				ent->client->bleedRate -= 0.1f;
				if (ent->client->bleedRate <= 0.0f) {
					client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BLEEDING;
					client->bleedCount = 0;
					client->bleedRate = 0; //@r00t: these are important!
				}
			}
			/* Clear the limping flag last */
			else if (client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_LIMPING) {
				client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_LIMPING;
				client->ps.stats[STAT_AIRMOVE_FLAGS] &= ~UT_PMF_CRASHED;
			}
		}
	}
#endif

	/* clear medic icon if its time has expired (10 seconds) */
	if ((client->ps.eFlags & EF_UT_MEDIC) &&
			(level.time > client->rewardTime)) {
		client->ps.eFlags &= ~EF_UT_MEDIC;
	}

	client->timeResidual += msec;
}

/**
 * ClientIntermissionThink
 */
void ClientIntermissionThink(gclient_t *client)
{
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	/* the level will exit when everyone wants to or after timeouts */

	/* swap and latch button actions */
	client->oldbuttons = client->buttons;
	client->buttons    = client->pers.cmd.buttons;

	if (client->buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE) &
			(client->oldbuttons ^ client->buttons)) {
		/*
		 * this used to be an ^1 but once a player says ready,
		 * it should stick
		 */
		client->readyToExit = 1;
	}
}

/**
 * OriginalQ3Map
 *
 * Checks if the map is an original Quake3 map
 */
int OriginalQ3Map(void)
{
	char  mapname[256];

	trap_Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));

	if ((mapname[0] == 'q') || (mapname[0] == 'Q')) {
		return 1;
	}
	return 0;
}

/**
 * ClientEvents
 *
 * Events will be passed on to the clients for presentation,
 * but any server game effects are handled here
 */
void ClientEvents(gentity_t *ent, int oldEventSequence) {

	int         i;
	int         event;
	int         damage;
	gclient_t   *client;
	gentity_t   *target;
	float       medtime;

	client = ent->client;

	if (oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}

	for (i = oldEventSequence ; i < client->ps.eventSequence ; i++) {

		event = CSE_DEC(client->ps.events[i & (MAX_PS_EVENTS - 1)]);

		switch (event) {

		    case EV_FALL_MEDIUM:
		    case EV_FALL_FAR:

                if (event == EV_FALL_FAR) {
                    if (!ent->client->ps.stats[STAT_EVENT_PARAM]) { /* if no damage given. */
                        damage = UT_StdRand(UT_MAX_HEALTH * 60 / 100, 0);
                    } else {
                        damage = ent->client->ps.stats[STAT_EVENT_PARAM];
                    }
                    medtime = 0.6;
                } else {
                    if (!ent->client->ps.stats[STAT_EVENT_PARAM]) { /* if no damage given */
                        damage = UT_StdRand(UT_MAX_HEALTH * 30 / 100, 0);
                    } else {
                        damage = ent->client->ps.stats[STAT_EVENT_PARAM];
                    }
                    medtime = 0.3;
                }

                /*
                 * Reset it, not strictly necessary but
                 * maybe useful in future
                 */
                ent->client->ps.stats[STAT_EVENT_PARAM] = 0;
                ent->pain_debounce_time = level.time + 200; /* no normal pain sound */

                /*
                 * HACK: Disallow damage on Quake3 maps.
                 * Jumping damage disallowed
                 */
                if (!OriginalQ3Map() || (!((g_gametype.integer == GT_JUMP) && (g_noDamage.integer == 1)))) { //@Barbatos - no damage in jump mode with g_noDamage 1
                    G_Damage(ent, NULL, NULL, NULL, NULL, damage, 0, MOD_FALLING, HL_UNKNOWN);
                    ent->client->bleedRate += medtime;
                    ent->client->bleedResidual = 0; //@r00t:No instant medic,restart timer
                    ent->client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_LIMPING;
                    ent->client->ps.stats[STAT_AIRMOVE_FLAGS] |= UT_PMF_CRASHED;
                }

                if ((g_gametype.integer == GT_JUMP) && (g_noDamage.integer == 1)) {
                    ent->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_LIMPING;
                    ent->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_BLEEDING;
                    ent->client->ps.stats[STAT_AIRMOVE_FLAGS] &= ~UT_PMF_CRASHED;
                }

                break;

            case EV_FIRE_WEAPON:
                ent->client->protectionTime = 0;
                ent->client->noGearChange	= qtrue;
                FireWeapon(ent);
                break;

            case EV_UT_BANDAGE:
                ent->client->bleedResidual = 0;
                break;

                /* player heal thy teammate */
            case EV_UT_TEAM_BANDAGE:
                G_HealOther(ent);
                break;

            case EV_UT_KICK:
                target = &g_entities[client->ps.eventParms[i & (MAX_PS_EVENTS - 1)]];
                G_Kick(ent, target);
                break;

            case EV_UT_GOOMBA:
                target = &g_entities[client->ps.eventParms[i & (MAX_PS_EVENTS - 1)]];
                G_Goomba(ent, target, 200);
                break;

			/* Use anything in the map */
            case EV_UT_USE:
                G_Use(ent);
                break;

            case EV_UT_WEAPON_MODE:

                /* if there's a mode change to "armed" while the player is ready to throw a gren */
                /* then we need to know to change the mode back to "safe" after it's thrown */
                if (((utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_HE) ||
                     (utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_FLASH) ||
                     (utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_SMOKE)) &&
                     (ent->client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE) &&
                     (utPSGetWeaponMode(&ent->client->ps) == 1) && !(ent->r.svFlags & SVF_BOT)) {
                    ent->client->botShouldJumpCount = 1; /* change back to safe mode after throwing */
                }

                /* Save the mode so we can use it later when we give the weapon to the client. */
                ent->client->pers.weaponModes[utPSGetWeaponID(&ent->client->ps)] = utPSGetWeaponMode(&ent->client->ps) + '0';
                break;

            default:
                break;
		}
	}
}

/**
 * G_IsStuckToClient
 */
static qboolean G_IsStuckToClient(gentity_t *ent, gentity_t *ent2)
{
	if ((ent2->client->ps.pm_type != PM_NORMAL) || (ent2->health <= 0)) {
		return qfalse;
	}

	if ((ent->client->ps.pm_type != PM_NORMAL) || (ent->health <= 0)) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[0] + ent2->r.mins[0] >=
			ent->r.currentOrigin[0] + ent->r.maxs[0]) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[1] + ent2->r.mins[1] >
			ent->r.currentOrigin[1] + ent->r.maxs[1]) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[2] + ent2->r.mins[2] >
			ent->r.currentOrigin[2] + ent->r.maxs[2]) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[0] + ent2->r.maxs[0] <
			ent->r.currentOrigin[0] + ent->r.mins[0]) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[1] + ent2->r.maxs[1] <
			ent->r.currentOrigin[1] + ent->r.mins[1]) {
		return qfalse;
	}

	if (ent2->r.currentOrigin[2] + ent2->r.maxs[2] <
			ent->r.currentOrigin[2] + ent->r.mins[2]) {
		return qfalse;
	}

	return qtrue;
}


/**
 * SendPendingPredictableEvents
 */
void SendPendingPredictableEvents(playerState_t *ps)
{
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	/* if there are still events pending */
	if (ps->entityEventSequence < ps->eventSequence) {
		/*
		 * create a temporary entity for this event which is
		 * sent to everyone except the client who generated
		 * the event
		 */
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ((ps->entityEventSequence & 3) << 8);
		/* set external event to zero before calling BG_PlayerStateToEntityState */
		extEvent = ps->externalEvent;
		ps->externalEvent = CSE_ENC(0);
		/* create temporary entity for event */
		t = G_TempEntity(ps->origin, CSE_DEC(event));
		number = t->s.number;
		BG_PlayerStateToEntityState(ps, &t->s, qtrue);
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		/* send to everyone except the client who generated the event */
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		/* set back external event */
		ps->externalEvent = extEvent;
	}
}

/**
 * ClientThink_BombDefuse
 *
 * Temp Bomb Defusing Function
 */
void ClientThink_BombDefuse(gentity_t *ent)
{
	gclient_t  *client;
	gclient_t  *other;
	gentity_t  *bomb;
	gentity_t  *event;
	int    i;
	int    weaponID;

	/* If not a valid client entity. */
	if (!ent || !ent->client) {
		return;
	}

	/* If bomb is not planted. */
	if (!level.BombPlanted || !level.BombMapAble || ((level.time - level.BombPlantTime) <= g_bombPlantTime.integer * 1000)) {
		return;
	}

	/* If bomb already defused. */
	if (level.BombDefused) {
		return;
	}

	/* Set client pointer. */
	client = ent->client;

	/* If we're not yet connected. */
	if (client->pers.connected != CON_CONNECTED) {
		return;
	}

	/* If we're dead. */
	if ((client->ps.stats[STAT_HEALTH] <= 0) || client->ghost) {
		return;
	}

	/* If we're not on the defusing team. */
	if (client->sess.sessionTeam != (level.AttackingTeam == TEAM_RED ? TEAM_BLUE : TEAM_RED)) {
		return;
	}

	/* Loop through clients */
	for (i = 0; i < level.maxclients; i++) {
		/* Set pointer */
		other = &level.clients[i];

		/* If it's us */
		if (other == client) {
			continue;
		}

		/* If the dude is not yet connected */
		if (other->pers.connected != CON_CONNECTED) {
			continue;
		}

		/* If the dude is dead */
		if ((other->ps.stats[STAT_HEALTH] <= 0) || other->ghost) {
			continue;
		}

		/* If the dude is not on our team */
		if (other->sess.sessionTeam != client->sess.sessionTeam) {
			continue;
		}

		/* If this dude is already defusing */
		if (other->IsDefusing) {
			return;
		}
	}

	/* Get bomb entity. */
	bomb = G_FindClassHash(NULL, HSH_ut_planted_bomb);

	/* If we couldn't find the bomb. */
	if (!bomb) {
		G_Error("ClientThink_BombDefuse: No bomb entity found while bomb is planted.");
	}

	/* If we're within defusing range from the bomb. */
	if (Distance(ent->r.currentOrigin, bomb->s.pos.trBase) < 75.0f) {
		/* If use key is pressed. */
		if (client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_USE_HELD) {
			/* If we're defusing. */
			if (client->IsDefusing) {
				/* If bomb is defused. */
				if ((level.time - level.StartDefuseTime) >= (g_bombDefuseTime.integer * 1000)) {
					G_LogPrintf("Bomb was defused by %i!\n", client->ps.clientNum);
					/* Set bomb defused flag. */
					level.BombDefused = qtrue;

					/* Clear defuse start time. */
					level.StartDefuseTime = 0;

					/* Clear defusing flags. */
					client->IsDefusing			  = qfalse;
					client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_DEFUSING;

					/* Clear defusing flag. */
					client->ps.eFlags &= ~EF_UT_DEFUSING;

					/* Send EV_UT_BOMBDEFUSE_STOP event. */
					event = G_TempEntity(bomb->s.pos.trBase, EV_UT_BOMBDEFUSE_STOP);
					event->s.eventParm = 1;
					event->r.svFlags  |= SVF_BROADCAST;

					AddScore(ent, ent->r.currentOrigin, 1);
					Stats_AddScore(ent, ST_BOMB_DEFUSE);
				}
			} else {
				/* @B1N If we have an armed grenade, put the pin back in */
				weaponID = utPSGetWeaponID ( &client->ps );
				if ((client->ps.stats[STAT_UTPMOVE_FLAGS]  & UT_PMF_FIRE_HELD) && ((weaponID == UT_WP_GRENADE_HE) || (weaponID == UT_WP_GRENADE_SMOKE) || (weaponID == UT_WP_GRENADE_FLASH))) {
					client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~(UT_PMF_FIRE_HELD | UT_PMF_READY_FIRE);
					client->ps.weaponstate = WEAPON_DROPPING;
				}

				/* Set defuse start time. */
				level.StartDefuseTime = level.time;

				/* Set defusing flags. */
				client->IsDefusing			          = qtrue;
				client->ps.stats[STAT_UTPMOVE_FLAGS] |= UT_PMF_DEFUSING;

				/* Set defusing flag. */
				client->ps.eFlags |= EF_UT_DEFUSING;

				/* Send EV_UT_BOMBDEFUSE_START event. */
				event		        = G_TempEntity(bomb->s.pos.trBase, EV_UT_BOMBDEFUSE_START);
				event->s.eventParm  = g_bombDefuseTime.integer;
				event->r.svFlags   |= SVF_BROADCAST;

				// Fenix: added autoradio message
				G_AutoRadio(7, ent);

			}

		} else {
			/* If we're defusing. */
			if (client->IsDefusing) {
				/* Clear defuse start time. */
				level.StartDefuseTime = 0;

				/* Clear defusing flags. */
				client->IsDefusing			  = qfalse;
				client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_DEFUSING;

				/* Clear defusing flag. */
				client->ps.eFlags &= ~EF_UT_DEFUSING;

				/* Send EV_UT_BOMBDEFUSE_STOP event. */
				event		   = G_TempEntity(bomb->s.pos.trBase, EV_UT_BOMBDEFUSE_STOP);
				event->s.eventParm = 0;
				event->r.svFlags  |= SVF_BROADCAST;
			}
		}
	} else {
		/* If we're defusing. */
		if (client->IsDefusing) {
			/* Clear defuse start time. */
			level.StartDefuseTime = 0;

			/* Clear defusing flags. */
			client->IsDefusing			  = qfalse;
			client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_DEFUSING;

			/* Clear defusing flag. */
			client->ps.eFlags &= ~EF_UT_DEFUSING;

			/* Send EV_UT_BOMBDEFUSE_STOP event. */
			event		   = G_TempEntity(bomb->s.pos.trBase, EV_UT_BOMBDEFUSE_STOP);
			event->s.eventParm = 0;
			event->r.svFlags  |= SVF_BROADCAST;
		}
	}
}

/**
 * ClientThink_real
 *
 * This will be called once for each client frame, which will
 * usually be a couple times for eachserver frame on fast clients.
 *
 * If "g_synchronousClients 1" is set, this will be called exactly
 * once fore each server frame, which makes for smooth demo recording.
 */
void ClientThink_real(gentity_t *ent)
{
	gclient_t  *client;
	pmove_t     pm;
	int         oldEventSequence;
	int         msec;
	usercmd_t  *ucmd;
	vec3_t	    oldViewAngles;

	client = ent->client;

	/*
	 * Don't think if the client is not yet connected
	 * (and thus not yet spawned in)
	 */
	if (client->pers.connected != CON_CONNECTED) {
		return;
	}
	client->LastClientUpdate = level.time;
	ucmd			 = &ent->client->pers.cmd;

	/* Stash the time of the attack so it doesnt get messed with */
	client->AttackTime = ucmd->serverTime;

	/* Sanity check the command time to prevent speedup cheating */
	if (ucmd->serverTime > level.time + 200) {
		ucmd->serverTime = level.time + 200;
	}

	if (ucmd->serverTime < level.time - 1000) {
		ucmd->serverTime = level.time - 1000;
	}

	msec = ucmd->serverTime - client->ps.commandTime;

	/*
	 * following others may result in bad times, but we still want
	 * to check for follow toggles
	 */
	if ((msec < 1) && (client->sess.spectatorState != SPECTATOR_FOLLOW)) {
		return;
	}

	if (msec > 200) {
		msec = 200;
	}

	if (client->pers.physics) {
		ucmd->serverTime = ((ucmd->serverTime + PMOVE_MSEC - 1) /
				PMOVE_MSEC) * PMOVE_MSEC;
	}

	/* Check for exiting intermission */
	if (level.intermissiontime) {
		ClientIntermissionThink(client);
		return;
	}

	/* spectators don't do much */
	if ((client->sess.sessionTeam == TEAM_SPECTATOR) || client->ghost) {
		if (client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
			return;
		}
		SpectatorThink(ent, ucmd);
		return;
	}

	/* If we're paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		/* Restore angles. */
		client->pers.cmd.angles[0] = client->oldAngles[0];
		client->pers.cmd.angles[1] = client->oldAngles[1];
		client->pers.cmd.angles[2] = client->oldAngles[2];

		/* Clear motion. */
		client->pers.cmd.forwardmove = 0;
		client->pers.cmd.rightmove	 = 0;
		client->pers.cmd.upmove 	 = 0;

		/* Clear the button. */
		client->pers.cmd.buttons = 0;
	} else {
		/* Backup angles. */
		client->oldAngles[0] = client->pers.cmd.angles[0];
		client->oldAngles[1] = client->pers.cmd.angles[1];
		client->oldAngles[2] = client->pers.cmd.angles[2];
	}

	/* Check for inactivity timer, but never drop the local client of a non-dedicated server. */
	if (!ClientInactivityTimer(client, msec)) {
		return;
	}

	if (client->noclip) {
		client->ps.pm_type = PM_NOCLIP;
	} else if (client->ps.stats[STAT_HEALTH] <= 0) {
		client->ps.pm_type = PM_DEAD;
	} else {
		client->ps.pm_type = PM_NORMAL;
	}

	/* If we're paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		client->ps.gravity = 0;
		client->ps.speed   = 0;
	} else {
		if (ent->triggerGravity) {
			client->ps.gravity = g_gravity.value * ent->triggerGravity->speed;
			ent->triggerGravity = NULL;
		} else {
			client->ps.gravity = g_gravity.value;
		}
		client->ps.speed   = UT_MAX_RUNSPEED;
	}

	/* Save the scores, this will be transmitted with the player state. */
    ent->client->ps.persistant[PERS_SCORERB]  = level.teamScores[TEAM_RED] | (level.teamScores[TEAM_BLUE]<<8); //r00t
    ent->client->ps.persistant[PERS_FLAGSRB]  = level.redCapturePointsHeld | (level.blueCapturePointsHeld<<8); //r00t

	/* set up for pmove */
	oldEventSequence = client->ps.eventSequence;

	memset(&pm, 0, sizeof(pm));

	if (g_gametype.integer == GT_UT_BOMB) {
		ClientThink_BombDefuse(ent);
	}

	/* If we're alive. */
	if ((ent->client->ps.stats[STAT_HEALTH] > 0) && !ent->client->ghost) {
		/* If we're planting. */
		if (ent->client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_PLANTING) {
			/* Lock player in place. */
			ent->client->pers.cmd.forwardmove = 0;
			ent->client->pers.cmd.rightmove   = 0;

			if (ent->client->pers.cmd.upmove > 0) {
				ent->client->pers.cmd.upmove = 0;
			}
		}
	}

	pm.ps  = &ent->client->ps;
	pm.cmd = *ucmd;

	if (pm.ps->pm_type == PM_DEAD) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	} else if (ent->client->stuckTo) {
		/* Make sure we are still stuck, if so, clip through players. */
		if (G_IsStuckToClient(ent, ent->client->stuckTo)) {
			pm.tracemask = MASK_PLAYERSOLID & ~(CONTENTS_BODY);
		} else {
			/* Ok, we arent stuck anymore so we can clear the stuck flag. */
			ent->client->stuckTo->client->stuckTo = NULL;
			ent->client->stuckTo = NULL;
			pm.tracemask = MASK_PLAYERSOLID;
		}
	} else if (ent->r.svFlags & SVF_BOT) {
		pm.tracemask = MASK_PLAYERSOLID | CONTENTS_BOTCLIP;
	} else if ((g_gametype.integer == GT_JUMP) && (client->sess.ghost)) {
	    pm.tracemask = (MASK_PLAYERSOLID & ~CONTENTS_BODY) | CONTENTS_CORPSE;
	} else {
		pm.tracemask = MASK_PLAYERSOLID;
	}

	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.warmup = level.warmupTime == 0 ? 0 : 1;
	pm.isBot = (ent->r.svFlags & SVF_BOT) ? qtrue : qfalse;
	pm.physics = client->pers.physics;
	pm.ariesModel = client->pers.ariesModel;

    if (level.pauseState & UT_PAUSE_ON) {
        pm.paused = qtrue;
    } else {
        pm.paused = qfalse;
    }

	VectorCopy(client->ps.origin, client->oldOrigin);
	VectorCopy(client->ps.viewangles, oldViewAngles);

	Pmove(&pm);

	/* If we're paused. */
	if (level.pauseState & UT_PAUSE_ON) {
		/* Restore origin and view angles. */
		VectorCopy(client->oldOrigin, client->ps.origin);
		VectorCopy(oldViewAngles, client->ps.viewangles);
	}

	/* save results of pmove */
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qtrue);

	SendPendingPredictableEvents(&ent->client->ps);

	/*
	 * set last zoom time for player: zeroed on spawn
	 *
	 * use (level.time - lastZoomTime) to determine how long they have
	 * been zoomed for must come after pmove because that's where
	 * client->buttons is set
	 */

	if (!(client->buttons & BUTTON_ZOOM1) && !(client->buttons & BUTTON_ZOOM2)) {
		client->lastZoomTime = level.time;
	}

	/* REMOVE: For grapple? */
	if (!(ent->client->ps.eFlags & EF_FIRING)) {
		client->fireHeld = qfalse;
	}

	/*
	 * Use the snapped origin for linking so it matches client
	 * predicted versions
	 */
	VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);

	VectorCopy(pm.mins, ent->r.mins);
	VectorCopy(pm.maxs, ent->r.maxs);

	ent->waterlevel = pm.waterlevel;
	ent->watertype	= pm.watertype;

	/* Execute client events */
	ClientEvents(ent, oldEventSequence);

	/* If weapon modes don't match. */
	if (client->pers.weaponModes[utPSGetWeaponID(&client->ps)] - '0' !=
			utPSGetWeaponMode(&client->ps)) {
		/* This shouldnt fckin happen!!! */
		client->pers.weaponModes[utPSGetWeaponID(&client->ps)] =
			utPSGetWeaponMode(&client->ps) + '0';
	}

	/*
	 * link entity now, after any personal teleporters have been used,
	 * except we got 'teleported' off the server
	 */
	if (client->pers.connected != CON_DISCONNECTED) {
		trap_LinkEntity(ent);
	}

	if (!ent->client->noclip) {
		G_TouchTriggers(ent);
	}

	/* NOTE: now copy the exact origin over otherwise clients can be snapped into solid */
	VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);

	{
		/*
		 * Need to calculate are leg, torso, pitch and roll values once
		 * each server frame
		 */
		angleData_t  adata;
		int 	 pryFlags;
		BG_PlayerAngles(ent->s.apos.trBase,
				ent->s.pos.trDelta,
				ent->s.legsAnim,
				ent->s.torsoAnim,
				level.time - level.previousTime,
				&adata,
				&ent->client->legsPitchAngle,
				&ent->client->legsYawAngle,
				&ent->client->torsoPitchAngle,
				&ent->client->torsoYawAngle,
				&pryFlags);
	}

	/* Test for solid areas in the AAS file */
	if (trap_Cvar_VariableIntegerValue("bot_enable"))
		BotTestAAS(ent->r.currentOrigin);

	/* Touch other objects */
	ClientImpacts(ent, &pm);

	/* Save results of triggers and client events */
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	/* Swap and latch button actions */
	client->oldbuttons	 = client->buttons;
	client->buttons 	 = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	/* Check for respawning */
	if (client->ps.stats[STAT_HEALTH] <= 0) {
		qboolean  doRespawn = qfalse;

		/* Wait for the attack button to be pressed */
		if (level.time > client->respawnTime) {
			/* forcerespawn is to prevent users from waiting out powerups */
			if (g_survivor.integer) {
				doRespawn = qtrue;
			} else if (g_forcerespawn.integer > 0 &&
					(level.time - client->respawnTime) > g_forcerespawn.integer * 1000) {
				doRespawn = qtrue;
			}
			/* Pressing attack or use is the normal respawn method */
			else if (ucmd->buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE)) {
				doRespawn = qtrue;
			}
			/* Before we only did this if it was > then 2 now we just check if its set */
			else if (!g_survivor.integer && (g_respawnDelay.integer || (g_waveRespawns.integer && g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_JUMP && (client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE)))) {
				doRespawn = qtrue;
			}
		}

		if (doRespawn) {
			if (g_survivor.integer) {
				/* Set ghost mode. */
				G_SetGhostMode(ent);

				/* Free minimap client arrow. */

				/* Unlink client from collision engine. */
				trap_UnlinkEntity(ent);
			} else {
				respawn(ent);
			}
		}

		return;
	}

	/*
	 * if they are ready to throw a grenade and the mode is armed and
	 * they are not a bot
	 */
	if (((utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_HE) /*||
			(utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_FLASH)*/ ||
			/*	 utPSGetWeaponID(&ent->client->ps) == UT_WP_GRENADE_SMOKE || */
			(utPSGetWeaponID(&ent->client->ps) == UT_WP_BOMB)) &&
			(ent->client->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_READY_FIRE) &&
			!(ent->r.svFlags & SVF_BOT)) {
		/* if the time has not started, start it now */
		if (ent->client->botNotMovedCount == 0) {
			ent->client->botNotMovedCount = level.time;
		} else if ((level.time - ent->client->botNotMovedCount) > 3500) { /* was 2500 */
			/* otherwise, if the gren has been held too long, it goes boom in their hand */
			ent->client->ps.stats[STAT_UTPMOVE_FLAGS] &= ~UT_PMF_FIRE_HELD;
		}
	} else if (!(ent->r.svFlags & SVF_BOT)) {
		/* they are either not holding a grenade, or it's not ready to throw or it's not armed yet */
		ent->client->botNotMovedCount = 0;

		/* should we change back to "safe" mode? */
		if (ent->client->botShouldJumpCount == 1) {
			if (utPSGetWeaponMode(&ent->client->ps) == 1) {
				utPSSetWeaponMode(&ent->client->ps, 0);
				ent->client->botShouldJumpCount = 0;
			}
		}
	}

	/* perform once-a-second actions */
	ClientTimerActions(ent, msec);
}

/**
 * ClientThink
 *
 * A new command has arrived from the client
 */
void ClientThink(int clientNum)
{
	gentity_t  *ent;

	ent = g_entities + clientNum;
	trap_GetUsercmd(clientNum, &ent->client->pers.cmd);

	/*
	 * Mark the time we got info, so we can display the
	 * phone jack if they don't ge any for a while
	 */
	ent->client->lastCmdTime = level.time;

	if (!(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer) {
		ClientThink_real(ent);
	}
}

/**
 * G_RunClient
 */
void G_RunClient(gentity_t *ent)
{
	int  timeSinceLastCmd;
	int  tol = g_antiwarptol.integer;

	if ((g_antiwarp.integer > 0) && !(ent->r.svFlags & SVF_BOT)) {
		timeSinceLastCmd = (level.time - ent->client->lastCmdTime) % 500;

		/*if (tol < 12) */
		/*	tol = 12; */

		if (timeSinceLastCmd > tol) {
			/* Indicate to the client that they are warping */
			ent->client->ps.eFlags |= EF_KAMIKAZE;

			/* Create a blank command */
			ent->client->pers.cmd.serverTime += 50;
			ClientThink_real(ent);
		} else {
			ent->client->ps.eFlags &= ~EF_KAMIKAZE;
		}
	}

	if (!(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real(ent);
}

/**
 * SpectatorClientEndFrame
 */
void SpectatorClientEndFrame(gentity_t *ent)
{
	gclient_t  *cl;
	int    i;
	int    persistant[MAX_PERSISTANT];

	/* if we are doing a chase cam or a remote view, grab the latest info */
	if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
		int  clientNum, flags;

		clientNum = ent->client->sess.spectatorClient;

		/* team follow1 and team follow2 go to whatever clients are playing */
		if (clientNum == -1) {
			clientNum = level.follow1;
		} else if (clientNum == -2) {
			clientNum = level.follow2;
		}

		if (clientNum >= 0) {
			cl = &level.clients[clientNum];

			if ((cl->pers.connected == CON_CONNECTED) &&
					((cl->sess.sessionTeam != TEAM_SPECTATOR) &&
					 !cl->ghost)) {
				flags = (cl->ps.eFlags &
					 ~(EF_VOTED |
					   EF_TEAMVOTED |
					   EF_TALK |
					   EF_CHASE)) |
					(ent->client->ps.eFlags &
					 (EF_VOTED |
					  EF_TEAMVOTED |
					  EF_TALK |
					  EF_CHASE));

				for (i = 0 ; i < MAX_PERSISTANT ; i++) {
					persistant[i] =
						ent->client->ps.persistant[i];
				}

				ent->client->ps = cl->ps;

				/* Disabled Reenable later! - DensitY */
				ent->client->ps.pm_flags |= PMF_FOLLOW;

				if (ent->client->ghost == qtrue) {
					ent->client->ps.pm_flags |=
						PMF_HAUNTING;
				}
				ent->client->ps.eFlags = flags;
				ent->client->ps.persistant[PERS_TEAM] =
					cl->ps.persistant[PERS_TEAM];

				for (i = 0 ; i < MAX_PERSISTANT ; i++) {
					ent->client->ps.persistant[i] =
						persistant[i];
				}

				return;
			} else {
				/*
				 * Drop them to free spectators unless they
				 * are dedicated camera followers
				 */
				if (ent->client->sess.spectatorClient >= 0) {
					Cmd_FollowCycle_f(ent, ent->client->spectatorDirection);
				}
			}
		}
	}
}

/**
 * ClientEndFrame
 *
 * Called at the end of each server frame for each connected client
 * A fast client will have multiple ClientThink for each ClientEdFrame,
 * while a slow client may have multiple ClientEndFrame between ClientThink.
 */
void ClientEndFrame(gentity_t *ent)
{
	clientPersistant_t	*pers;

	pers = &ent->client->pers;

	/* Pending team change so q3 doesnt choke */
	if ((level.time > ent->client->sess.queSwitch) &&
			(ent->client->sess.queSwitch != 0)) {
		ent->client->sess.queSwitch = 0;
		SetTeam(ent, ent->client->sess.pendingTeam == TEAM_RED ?
			"red" : "blue", qtrue);
	}

	if ((ent->client->sess.sessionTeam == TEAM_SPECTATOR) ||
			ent->client->ghost) {
		SpectatorClientEndFrame(ent);
		return;
	}

	/*
	 * If the end of unit layout is displayed, don't give
	 * the player any normal movement attributes
	 */
	if (level.intermissiontime) {
		return;
	}

	/* Burn from lava, etc */
	P_WorldEffects(ent);

	/* Apply all the damage taken this frame */
	P_DamageFeedback(ent);

	/* Add the EF_CONNECTION flag if we haven't gotten commands recently */
	if (level.time - ent->client->lastCmdTime > 1000) {
		ent->s.eFlags |= EF_CONNECTION;
	} else {
		ent->s.eFlags &= ~EF_CONNECTION;
	}

	/* FIXME: get rid of ent->health */
	ent->client->ps.stats[STAT_HEALTH] = ent->health;

	G_SetClientSound(ent);

	BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qtrue);

	SendPendingPredictableEvents(&ent->client->ps);

	G_AnimatePlayer(ent);

	/* Cache the position */
	G_StoreFIFOTrail(ent);

	/* Check for dual primaries etc. */
	Contraband(ent);
}

/**
 * G_AnimatePlayer
 */
void G_AnimatePlayer(gentity_t *ent)
{
	int    legsOld;
	int    legs;
	float  legsBackLerp;
	int    torsoOld;
	int    torso;
	float  torsoBackLerp;

	ent->client->animationdata.legsAnim   = ent->s.legsAnim & ~ANIM_TOGGLEBIT;
	ent->client->animationdata.torsoAnim  = ent->s.torsoAnim & ~ANIM_TOGGLEBIT;
	ent->client->animationdata.clientnum  = ent->s.number;

	/*
	Com_Printf("G_AnimatePlayer: %d: %d %d\n",
		level.time, ent->s.legsAnim, ent->s.torsoAnim);
	*/

	C_PlayerAnimation(&ent->s, g_playerAnims[ent->client->pers.ariesModel],
			  &ent->client->animationdata.legs,
			  &ent->client->animationdata.torso,
			  &legsOld, &legs, &legsBackLerp,
			  &torsoOld, &torso, &torsoBackLerp,
			  level.time);
}
