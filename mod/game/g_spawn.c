/**
 * Filename: g_spawn.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * This file contains routines to maintain session data between rounds.
 *
 * Entity spawning system
 */

#include "g_local.h"
#include "../common/c_bmalloc.h"

#define MAX_SPAWNENTITIES (MAX_GENTITIES - MAX_CLIENTS)

local_backup_spawn_t  levelspawn[MAX_SPAWNENTITIES];

typedef struct g_string_s {
	struct g_string_s *prev;
	struct g_string_s *next;
} g_string_t;

g_string_t *g_new_string_list = 0;

qboolean G_SpawnString( const char *key, const char *defaultString, char * *out )
{
	int  i;

	if(!level.spawning)
	{
		*out = (char *)defaultString;
		G_Error( "G_SpawnString() Called while not spawning!" );
	}

	if(level.RoundRespawn)	   // Round mode based respawning
	{
		for(i = 0 ; i < levelspawn[cur_levelspawn].numSpawnVars ; i++)
		{
			if (!Q_stricmp( key, levelspawn[cur_levelspawn].spawnVars[i][0] ))
			{
				*out = levelspawn[cur_levelspawn].spawnVars[i][1];
				return qtrue;
			}
		}
	}
	else		// Standard map spawnings
	{
		for(i = 0; i < level.numSpawnVars ; i++)
		{
			if (!Q_stricmp( key, level.spawnVars[i][0] ))
			{
				*out = level.spawnVars[i][1];
				return qtrue;
			}
		}
	}
	*out = (char *)defaultString;
	return qfalse;
}

/**
 * G_SpawnFloat
 */
qboolean G_SpawnFloat( const char *key, const char *defaultString, float *out )
{
	char	  *s;
	qboolean  present;

	present = G_SpawnString( key, defaultString, &s );
	*out	= atof( s );
	return present;
}

/**
 * G_SpawnInt
 */
qboolean G_SpawnInt( const char *key, const char *defaultString, int *out )
{
	char	  *s;
	qboolean  present;

	present = G_SpawnString( key, defaultString, &s );
	*out	= atoi( s );
	return present;
}

/**
 * G_SpawnVector
 */
qboolean G_SpawnVector( const char *key, const char *defaultString, float *out )
{
	char	  *s;
	qboolean  present;

	present = G_SpawnString( key, defaultString, &s );
	sscanf( s, "%f %f %f", &out[0], &out[1], &out[2] );
	return present;
}

/* fields are needed for spawning from the entity string */
typedef enum
{
	F_INT,
	F_FLOAT,
	F_LSTRING,	 // string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,	 // string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
	F_ENTITY,	 // index on disk, pointer in memory
	F_ITEM, 	 // index on disk, pointer in memory
	F_CLIENT,	 // index on disk, pointer in memory
	F_IGNORE
} fieldtype_t;

typedef struct
{
	char		 *name;
	int 		 ofs;
	fieldtype_t  type;
	int 		 flags;
} field_t;

field_t  fields[] = {
	{ "classname",			FOFS(classname),			F_LSTRING  },
	{ "origin", 			FOFS(s.origin), 			F_VECTOR   },
	{ "model",				FOFS(model),				F_LSTRING  },
	{ "model2", 			FOFS(model2),				F_LSTRING  },
	{ "spawnflags", 		FOFS(spawnflags),			F_INT	   },
	{ "speed",				FOFS(speed),				F_FLOAT    },
	{ "target", 			FOFS(target),				F_LSTRING  },
	{ "targetname", 		FOFS(targetname),			F_LSTRING  },
	{ "message",			FOFS(message),				F_LSTRING  },
	{ "team",				FOFS(team), 				F_LSTRING  },
	{ "wait",				FOFS(wait), 				F_FLOAT    },
	{ "random", 			FOFS(random),				F_FLOAT    },
	{ "count",				FOFS(count),				F_INT	   },
	{ "health", 			FOFS(health),				F_INT	   },
	{ "light",				0,							F_IGNORE   },
	{ "dmg",				FOFS(damage),				F_INT	   },
	{ "angles", 			FOFS(s.angles), 			F_VECTOR   },
	{ "angle",				FOFS(s.angles), 			F_ANGLEHACK},
	{ "targetShaderName",	FOFS(targetShaderName), 	F_LSTRING  },
	{ "targetShaderNewName",FOFS(targetShaderNewName),	F_LSTRING  },
	// For Bombs!
	{ "range",				FOFS( s.frame ),			F_INT	   },
	
	{ NULL}
};

typedef struct
{
	char  *name;
	void (*spawn)(gentity_t *ent);
} spawn_t;

void SP_info_player_start (gentity_t *ent);
void SP_info_player_deathmatch (gentity_t *ent);
void SP_info_player_intermission (gentity_t *ent);
void SP_info_firstplace(gentity_t *ent);
void SP_info_secondplace(gentity_t *ent);
void SP_info_thirdplace(gentity_t *ent);
void SP_info_podium(gentity_t *ent);

void SP_func_plat (gentity_t *ent);
void SP_func_static (gentity_t *ent);
void SP_func_rotating (gentity_t *ent);
void SP_func_bobbing (gentity_t *ent);
void SP_func_wall (gentity_t *ent);
void SP_mr_sentry (gentity_t *ent);
void SP_ut_jumpstart (gentity_t *ent);
void SP_ut_jumpstop  (gentity_t *ent);
void SP_ut_jumpcancel  (gentity_t *ent);
void SP_ut_jumpgoal (gentity_t *ent);
void SP_func_pendulum( gentity_t *ent );
void SP_func_button (gentity_t *ent);
void SP_func_door (gentity_t *ent);
void SP_func_train (gentity_t *ent);
void SP_func_timer (gentity_t *self);

void SP_trigger_always (gentity_t *ent);
void SP_trigger_multiple (gentity_t *ent);
void SP_trigger_push (gentity_t *ent);
void SP_trigger_teleport (gentity_t *ent);
void SP_trigger_hurt (gentity_t *ent);
void SP_trigger_location (gentity_t *ent);
void SP_trigger_gravity (gentity_t *ent);

void SP_target_remove_powerups( gentity_t *ent );
void SP_target_give (gentity_t *ent);
void SP_target_delay (gentity_t *ent);
void SP_target_speaker (gentity_t *ent);
void SP_target_print (gentity_t *ent);
void SP_target_laser (gentity_t *self);
void SP_target_character (gentity_t *ent);
void SP_target_score( gentity_t *ent );
void SP_target_teleporter( gentity_t *ent );
void SP_target_relay (gentity_t *ent);
void SP_target_kill (gentity_t *ent);
void SP_target_position (gentity_t *ent);
void SP_target_location (gentity_t *ent);
void SP_target_push (gentity_t *ent);

void SP_light (gentity_t *self);
void SP_info_null (gentity_t *self);
void SP_info_notnull (gentity_t *self);
void SP_info_camp (gentity_t *self);
void SP_path_corner (gentity_t *self);
void SP_path_ut_stop (gentity_t *self);

void SP_misc_teleporter_dest (gentity_t *self);
void SP_misc_model(gentity_t *ent);
void SP_misc_portal_camera(gentity_t *ent);
void SP_misc_portal_surface(gentity_t *ent);

void SP_team_CTF_redplayer( gentity_t *ent );
void SP_team_CTF_blueplayer( gentity_t *ent );

void SP_team_CTF_redspawn( gentity_t *ent );
void SP_team_CTF_bluespawn( gentity_t *ent );

/**
 * $(function)
 */
void SP_item_botroam( gentity_t *ent )
{
}

// Urban terror gametype additions
void SP_team_CAH_capturepoint ( gentity_t *ent );

/* NPC NPC NPC
void SP_func_sniperGameStart ( gentity_t* ent ); // GottaBeKD: begin sniper game - single player UT trainer
void SP_func_sniperGameEnd ( gentity_t* ent ); // GottaBeKD: end sniper game - single player UT trainer
void SP_func_RaceStart ( gentity_t* ent ); // GottaBeKD: begin a timed race
void SP_func_RaceEnd ( gentity_t* ent ); // GottaBeKD: end a timed race
*/

// Urban Terror entity spawns
void SP_func_rotating_door (gentity_t *ent);
void SP_func_breakable (gentity_t *ent);
void SP_func_ut_train (gentity_t *ent);  // the ut_train prototype, used for elevators

/* NPC NPC NPC
void  SP_func_NPC (gentity_t *ent); // the NPC prototype
void  SP_func_waypoint (gentity_t *ent); // GottaBeKD: the waypoint prototype for bot routing
void  SP_func_npc_radio (gentity_t *ent); // for NPC voice (radio) commands
void  SP_func_npc_leading (gentity_t *ent); // the NPC isLeading trigger
void  SP_func_npc_waitUntilVis (gentity_t *ent); // the NPC won't move on to the next waypoint until a player becomes visible
void  SP_func_npc_waitTime (gentity_t *ent); // the NPC won't move on to the next waypoint until this amount of time in milleseconds has passed
void  SP_func_npc_spawn (gentity_t *ent); // for NPC's who get forced/triggered spawns
void  SP_func_npc_kill (gentity_t *ent); // for NPC's who get forced/triggered death
void  SP_func_npc_takeDamage (gentity_t *ent); // the NPC takeDamage trigger
void  SP_func_npc_crouch (gentity_t *ent); // for NPC's who get forced/triggered crouch status
*/

void SP_func_ut_playerSound (gentity_t *ent); // play a sound that all players will hear, and that will not diminish
void SP_func_ut_originSound (gentity_t *ent); // play a sound at the origin of the map ent

void SP_func_keyboard_interface (gentity_t *ent);  // a keyboard menu interface for ents and npc's

void SP_info_ut_spawn (gentity_t *ent); // the Urban Terror player spawn ent
void SP_info_ut_bombsite( gentity_t *ent );

spawn_t  spawns[] = {
	// info entities don't do anything at all, but provide positional
	// information for things controlled by other processes
	{ "info_player_start",		  SP_info_player_start		 },
	{ "info_player_deathmatch",   SP_info_player_deathmatch  },
	{ "info_player_intermission", SP_info_player_intermission},
	{ "info_null",				  SP_info_null				 },
	{ "info_notnull",			  SP_info_notnull			 }, // use target_position instead
	{ "info_camp",				  SP_info_camp				 },

	{ "func_plat",				  SP_func_plat				 },
	{ "func_button",			  SP_func_button			 },
	{ "func_door",				  SP_func_door				 },
	{ "func_static",			  SP_func_static			 },
	{ "func_rotating",			  SP_func_rotating			 },
	{ "func_bobbing",			  SP_func_bobbing			 },
	{ "func_pendulum",			  SP_func_pendulum			 },
	{ "func_train", 			  SP_func_train 			 },
	{ "func_group", 			  SP_info_null				 },
	{ "func_timer", 			  SP_func_timer 			 }, // rename trigger_timer?

	// Urban Terror funcs
	{ "func_rotating_door", 	  SP_func_rotating_door 	 },
	{ "ut_mrsentry",			  SP_mr_sentry				 },
	{ "ut_jumpgoal",			  SP_ut_jumpgoal			 },
	{ "ut_jumpstart",			  SP_ut_jumpstart			 },
	{ "ut_jumpstop",			  SP_ut_jumpstop			 },
	{ "ut_jumpcancel",			  SP_ut_jumpcancel			 },
	{ "func_wall",				  SP_func_wall				 },
	{ "func_breakable", 		  SP_func_breakable 		 },
	{ "func_ut_train",			  SP_func_ut_train			 }, // GottaBeKD - used for elevators

	{ "func_keyboard_interface",  SP_func_keyboard_interface }, // GottaBeKD - used for interfacing ents and npcs, vehicles, elevators etc.

	{ "func_ut_playerSound",	  SP_func_ut_playerSound	 }, // GottaBeKD - play a sound that all players here, and that does not diminish
	{ "func_ut_globalSound",	  SP_func_ut_originSound	 }, // GottaBeKD - play a sound at the origin of the ent

/* NPC NPC NPC
	{"func_NPC",				  SP_func_NPC				 }, // GottaBeKD - NPC entity
	{"func_waypoint",			  SP_func_waypoint			 }, // GottaBeKD - NPC way point entity
	{"func_npc_radio",			  SP_func_npc_radio 		 }, // GottaBeKD - NPC voice (radio) entity
	{"func_npc_leading",		  SP_func_npc_leading		 }, // GottaBeKD - NPC isLeading trigger entity
	{"func_npc_waitUntilVis",	  SP_func_npc_waitUntilVis	 }, // GottaBeKD - NPC isWaitingUntilVis trigger entity
	{"func_npc_waitTime",		  SP_func_npc_waitTime		 }, // GottaBeKD - NPC waitTime entity
	{"func_npc_spawn",			  SP_func_npc_spawn 		 }, // GottaBeKD - NPC triggered spawn entity
	{"func_npc_kill",			  SP_func_npc_kill			 }, // GottaBeKD - NPC triggered kill entity
	{"func_npc_takeDamage", 	  SP_func_npc_takeDamage	 }, // GottaBeKD - NPC takeDamage trigger entity
	{"func_npc_crouch", 		  SP_func_npc_crouch		 }, // GottaBeKD - NPC triggered crouch entity
	{"func_sniperGameStart",	  SP_func_sniperGameStart	 },
	{"func_sniperGameEnd",		  SP_func_sniperGameEnd 	 },
	{"func_RaceStart",			  SP_func_RaceStart 		 },
	{"func_RaceEnd",			  SP_func_RaceEnd			 },
*/

	// URban terror gametype entities
	{ "team_CAH_capturepoint",	  SP_team_CAH_capturepoint	 },
	// DENSITY- Bomb Mode Spawns!
	{ "info_ut_bombsite",		  SP_info_ut_bombsite		 },

	{ "info_ut_spawn",			  SP_info_ut_spawn			 },

	// Triggers are brush objects that cause an effect when contacted
	// by a living player, usually involving firing targets.
	// While almost everything could be done with
	// a single trigger class and different targets, triggered effects
	// could not be client side predicted (push and teleport).
	{ "trigger_always", 		  SP_trigger_always 	 },
	{ "trigger_multiple",		  SP_trigger_multiple	 },
	{ "trigger_push",			  SP_trigger_push		 },
	{ "trigger_teleport",		  SP_trigger_teleport	 },
	{ "trigger_hurt",			  SP_trigger_hurt		 },
	{ "trigger_location",		  SP_trigger_location	 },
	{ "trigger_gravity",		  SP_trigger_gravity	 },

	// targets perform no action by themselves, but must be triggered
	// by another entity
	{ "target_give",			  SP_target_give		 },
	{ "target_remove_powerups",   SP_target_remove_powerups  },
	{ "target_delay",			  SP_target_delay		 },
	{ "target_speaker", 		  SP_target_speaker 	 },
	{ "target_print",			  SP_target_print		 },
	{ "target_laser",			  SP_target_laser		 },
	{ "target_score",			  SP_target_score		 },
	{ "target_teleporter",		  SP_target_teleporter	 },
	{ "target_relay",			  SP_target_relay		 },
	{ "target_kill",			  SP_target_kill		 },
	{ "target_position",		  SP_target_position	 },
	{ "target_location",		  SP_target_location	 },
	{ "target_push",			  SP_target_push		 },

	{ "light",					  SP_light				 },
	{ "path_corner",			  SP_path_corner		 },
	{ "path_ut_stop",			  SP_path_ut_stop		 },

	{ "misc_teleporter_dest",	  SP_misc_teleporter_dest	 },
	{ "misc_model", 			  SP_misc_model 			 },
	{ "misc_portal_surface",	  SP_misc_portal_surface	 },
	{ "misc_portal_camera", 	  SP_misc_portal_camera 	 },

	{ "team_CTF_redplayer", 	  0 			 },
	{ "team_CTF_blueplayer",	  0 			 },

	{ "team_CTF_redspawn",		  0 			 },
	{ "team_CTF_bluespawn", 	  0 			 },

	{ "item_botroam",			  SP_item_botroam},

	/* New Camera View node entity */
	{ "misc_CameraViewNode",	  0 			 },

	{ 0 		   ,			  0 			 }
};

/* Entities we don't want to spawn between rounds, add as required */
char	 *DontStoreEntities[] = {
	"light",
	"item_botroam",
	"shooter_plasma",
	"shooter_grenade",
	"shooter_rocket",
	NULL
};

/**
 * G_CallSpawn
 *
 * Finds the spawn function for the entity and calls it,
 * returning qfalse if not found
 */
qboolean G_CallSpawn( gentity_t *ent )
{
	spawn_t  *s;
	gitem_t  *item;

	if (!ent->classname)
	{
		G_Printf ("G_CallSpawn: NULL classname\n");
		return qfalse;
	}

	HASHSTR(ent->classname,ent->classhash);

	// check item spawn functions
	for (item = bg_itemlist + 1 ; item->classname ; item++)
	{
		if (!strcmp(item->classname, ent->classname))
		{
			G_SpawnItem( ent, item );
			return qtrue;
		}
	}

	// check normal spawn functions
	for (s = spawns ; s->name ; s++)
	{
		if (!strcmp(s->name, ent->classname))
		{
			// found it
			if (s->spawn)
				s->spawn(ent);
			return qtrue;
		}
	}
	// removed because ppl didn't like it
//	G_Printf ("%s doesn't have a spawn function\n", ent->classname);
	return qfalse;
}

/**
 * G_NewString
 *
 * Build a copy of the string, translating \n to real linefeeds
 * so message texts can be multi-line
 */
char *G_NewString( const char *string )
{
	char  *newb, *newp;
	int   l;
	const char	*s;
	g_string_t *hdr;

	l = strlen(string) + 1 + sizeof(g_string_t);

	newb = bmalloc(l);
	memset(newb, 0, l);

	hdr = (g_string_t *)newb;
	hdr->prev = NULL;
	hdr->next = g_new_string_list;
	if (g_new_string_list)
		g_new_string_list->prev = hdr;
	g_new_string_list = hdr;

	newp = newb + sizeof(g_string_t);

	// turn \n into a real linefeed
	for (s = string; *s; s++)
	{
		if (*s == '\\' && *(s+1))
		{
			s++;
			*newp++ = (*s == 'n') ?
				'\n' : '\\';
		}
		else
		{
			*newp++ = *s;
		}
	}
	*newp = 0;

	return (newb + sizeof(g_string_t));
}

void G_FreeString( const char *string )
{
	g_string_t *hdr = (g_string_t *)(string - sizeof(g_string_t));
	g_string_t *current = g_new_string_list;

	while (current) {
		if (current == hdr) {
			// G_Printf("Freed string \"%s\"\n", string);
			if (hdr->prev)
				hdr->prev->next = hdr->next;
			if (hdr->next)
				hdr->next->prev = hdr->prev;
			if (g_new_string_list == hdr)
				g_new_string_list = hdr->next;

			memset(hdr, 0, sizeof(g_string_t) + strlen(string));
			bfree(hdr);
			break;
		}
		current = current->next;
	}
	// G_Printf("Couldn't free string \"%s\"\n", string);
}

/**
 * G_ParseField
 *
 * Takes a key/value pair and sets the binary values
 * in a gentity
 */
void G_ParseField( const char *key, const char *value, gentity_t *ent )
{
	field_t  *f;
	byte	 *b;
	float	 v;
	vec3_t	 vec;

	for (f = fields ; f->name ; f++)
	{
		if (!Q_stricmp(f->name, key))
		{
			// found it
			b = (byte *)ent;

			switch(f->type)
			{
			case F_LSTRING:
				*(char * *)(b + f->ofs) = G_NewString (value);
				break;

			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b + f->ofs))[0] = vec[0];
				((float *)(b + f->ofs))[1] = vec[1];
				((float *)(b + f->ofs))[2] = vec[2];
				break;

			case F_INT:
				*(int *)(b + f->ofs) = atoi(value);
				break;

			case F_FLOAT:
				*(float *)(b + f->ofs) = atof(value);
				break;

			case F_ANGLEHACK:
				v			   = atof(value);
				((float *)(b + f->ofs))[0] = 0;
				((float *)(b + f->ofs))[1] = v;
				((float *)(b + f->ofs))[2] = 0;
				break;

			default:
			case F_IGNORE:
				break;
			}
			return;
		}
	}
}

/**
 * G_ParseSpawnFields
 */
void G_ParseSpawnFields( gentity_t *e, int SpawnNum )
{
	int  i;

	if(level.RoundRespawn)
	{
		for(i = 0; i < levelspawn[SpawnNum].numSpawnVars; i++)
		{
			G_ParseField( levelspawn[SpawnNum].spawnVars[i][0],
					  levelspawn[SpawnNum].spawnVars[i][1], e );
		}
	}
	else
	{
		for(i = 0; i < level.numSpawnVars; i++)
		{
			G_ParseField( level.spawnVars[i][0],
					  level.spawnVars[i][1], e );
		}
	}
	if (e->classname) HASHSTR(e->classname,e->classhash);
}

/**
 * G_SpawnGEntityFromSpawnVars
 *
 * Spawn an entity and fill in all the level fields from
 * level.spawnVars[], then call the class specific spawn function
 */
void G_SpawnGEntityFromSpawnVars( void )
{
	int    i;
	gentity_t  *ent;

	// get the next free entity
	ent = G_Spawn();

	G_ParseSpawnFields( ent, cur_levelspawn );

	// check for "notteam" flag (GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER)
	if ((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		G_SpawnInt( "notteam", "0", &i );

		if (i)
		{
			G_FreeEntity( ent );
			return;
		}
	}
	else
	{
		G_SpawnInt( "notfree", "0", &i );

		if (i)
		{
			G_FreeEntity( ent );
			return;
		}
	}
	// move editor origin to pos
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// If this isnt CTF then get rid of the CTF flags.
	// @Fenix - remove flags also if we are not playing JUMP mode
	if ((g_gametype.integer != GT_CTF) && (g_gametype.integer != GT_JUMP)) {
//		  if (ent->classname && !Q_stricmp ( ent->classname, "team_CTF_redflag")){
		if (ent->classname && ent->classhash==HSH_team_CTF_redflag){
			G_FreeEntity( ent );
//		  } else if (ent->classname && !Q_stricmp ( ent->classname, "team_CTF_blueflag")) {
		} else if (ent->classname && ent->classhash==HSH_team_CTF_blueflag) {
			G_FreeEntity( ent );
		}
	}

	//If This IS CTF, make sure the flags are broadcast
	// @Fenix - add flags also if we are playing JUMP mode
	if ((g_gametype.integer == GT_CTF) || (g_gametype.integer == GT_JUMP)) {
//		  if (ent->classname && !Q_stricmp ( ent->classname, "team_CTF_redflag")){
		if (ent->classname && ent->classhash==HSH_team_CTF_redflag){
			ent->r.svFlags |= SVF_BROADCAST;
//		  } else if (ent->classname && !Q_stricmp ( ent->classname, "team_CTF_blueflag")) {
		} else if (ent->classname && ent->classhash==HSH_team_CTF_blueflag) {
			ent->r.svFlags |= SVF_BROADCAST;
		}
	}

	if(ent->classname)
	{
		for(i = 0; DontStoreEntities[i] != NULL; i++)
		{
			if(!Q_stricmp( ent->classname, DontStoreEntities[i] ))
			{
				return; // goodbye
			}
		}
	}

	// if we didn't get a classname, don't bother spawning anything
	if (!G_CallSpawn( ent ))
	{
		G_FreeEntity( ent );
		return;
	}
	levelspawn[num_levelspawn].numSpawnVars = level.numSpawnVars;

	for(i = 0; i < levelspawn[num_levelspawn].numSpawnVars; i++)
	{
		levelspawn[num_levelspawn].spawnVars[i][0] =
			bmalloc( strlen( level.spawnVars[i][0] ) + 1 );
		levelspawn[num_levelspawn].spawnVars[i][1] =
			bmalloc( strlen( level.spawnVars[i][1] ) + 1 );
		memcpy( levelspawn[num_levelspawn].spawnVars[i][0],
				level.spawnVars[i][0],
				strlen( level.spawnVars[i][0] ) + 1 );
		memcpy( levelspawn[num_levelspawn].spawnVars[i][1],
				level.spawnVars[i][1],
				strlen( level.spawnVars[i][1] ) + 1 );
	}
	num_levelspawn++; // stored, increase counter
}

/**
 * G_AddSpawnVarToken
 */
char *G_AddSpawnVarToken( const char *string )
{
	int   l;
	char  *dest;

	l = strlen( string );

	if(level.numSpawnVarChars + l + 1 > MAX_SPAWN_VARS_CHARS)
	{
		G_Error( "G_AddSpawnVarToken: MAX_SPAWN_VARS" );
	}
	dest			= level.spawnVarChars + level.numSpawnVarChars;
	memcpy( dest, string, l + 1 );
	level.numSpawnVarChars += l + 1;
	return dest;
}

/**
 * G_ParseSpawnVars
 *
 * Parse a brace bounded set of key / value pairs out of the
 * level's entity strings into level.spawnVars[]
 *
 * This does not actually spawn an entity.
 */
qboolean G_ParseSpawnVars( void )
{
	char  keyname[MAX_TOKEN_CHARS];
	char  com_token[MAX_TOKEN_CHARS];

	level.numSpawnVars	   = 0;
	level.numSpawnVarChars = 0;

	// parse the opening brace
	if (!trap_GetEntityToken( com_token, sizeof(com_token)))
	{
		// end of spawn string
		return qfalse;
	}

	if (com_token[0] != '{')
	{
		G_Error( "G_ParseSpawnVars: found %s when expecting {", com_token );
	}

	// go through all the key / value pairs
	while (1)
	{
		// parse key
		if (!trap_GetEntityToken( keyname, sizeof(keyname)))
		{
			G_Error( "G_ParseSpawnVars: EOF without closing brace" );
		}

		if (keyname[0] == '}')
		{
			break;
		}

		// parse value
		if (!trap_GetEntityToken( com_token, sizeof(com_token)))
		{
			G_Error( "G_ParseSpawnVars: EOF without closing brace" );
		}

		if (com_token[0] == '}')
		{
			G_Error( "G_ParseSpawnVars: closing brace without data" );
		}

		if (levelspawn[num_levelspawn].numSpawnVars == MAX_SPAWN_VARS)
		{
			G_Error( "G_ParseSpawnVars: MAX_SPAWN_VARS" );
		}
		level.spawnVars[level.numSpawnVars][0] = G_AddSpawnVarToken( keyname );
		level.spawnVars[level.numSpawnVars][1] = G_AddSpawnVarToken( com_token );
		level.numSpawnVars++;
	}
	return qtrue;
}

/*QUAKED worldspawn (0 0 0) ?

Every map should have exactly one worldspawn.
"music" 	music wav file
"gravity"	800 is default gravity
"message"	Text to print during connection process
*/
void SP_worldspawn( void )
{
	char  *s;

	G_SpawnString( "classname", "", &s );

	if (Q_stricmp( s, "worldspawn" )) {
		G_Error( "SP_worldspawn: The first entity isn't 'worldspawn'" );
	}

	// make some data visible to connecting client
	trap_SetConfigstring( CS_GAME_VERSION, GAME_VERSION );

	//@Barbatos: added for the updater
	trap_SetConfigstring( CS_GAME_VERSION_UPDATER, va("%i", GAME_VERSION_UPDATER ) );

	level.startTime = level.time;
	trap_SetConfigstring( CS_LEVEL_START_TIME, va("%i", level.startTime ));

	level.roundEndTime = level.time + (int)(g_RoundTime.value * 60 * 1000);
	trap_SetConfigstring( CS_ROUND_END_TIME, va("%i", level.roundEndTime ));

	//Fenix: jump mode config strings
	if (g_gametype.integer == GT_JUMP) {
		trap_SetConfigstring( CS_NODAMAGE, va("%i", g_noDamage.integer ));
	}

	trap_SetConfigstring(CS_FUNSTUFF, va("%i", g_funstuff.integer));

//	G_SpawnString( "music", "", &s );
//	trap_SetConfigstring( CS_MUSIC, s );

	G_SpawnString( "message", "", &s );
	trap_SetConfigstring( CS_MESSAGE, s );				// map specific message

	trap_SetConfigstring( CS_MOTD, g_motd.string ); 	// message of the day

	G_SpawnString( "gravity", "800", &s );
	trap_Cvar_Set( "g_gravity", s );

	G_SpawnString( "enableDust", "0", &s );
	trap_Cvar_Set( "g_enableDust", s );

	G_SpawnString( "enableBreath", "0", &s );
	trap_Cvar_Set( "g_enableBreath", s );

	G_SpawnString( "enablePrecip", "0", &s );	// 0 none, 1 rain, others snow
	trap_Cvar_Set( "g_enablePrecip", s );

	//G_SpawnString( "precipAmount", "0", &s ); // -1 is variable, 0 is none
	//trap_Cvar_Set( "g_precipAmount", s );

//	g_surfacedefault.integer=0;
//	G_SpawnString( "surface_default", "0", &s );
//	trap_Cvar_Set( "g_surfacedefault", s );

	G_SpawnString( "Attacking_Team", "0", &s );

	if(atoi( s ) == 0)
	{
		level.AttackingTeam = TEAM_RED;
	}
	else
	{
		level.AttackingTeam = TEAM_BLUE;
	}

	//if ( g_precipAmount.integer > UTPRECIP_MAX_DROPS ) g_precipAmount.integer = UTPRECIP_MAX_DROPS;

	g_entities[ENTITYNUM_WORLD].s.number  = ENTITYNUM_WORLD;
	g_entities[ENTITYNUM_WORLD].classname = "worldspawn";
	g_entities[ENTITYNUM_WORLD].classhash = HSH_worldspawn;

	// see if we want a warmup time
	trap_SetConfigstring( CS_WARMUP, va("%i", 0));

	if (!g_survivor.integer && g_restarted.integer)
	{
		trap_Cvar_Set( "g_restarted", "0" );
		level.warmupTime = 0;
	}
	else if((g_gametype.integer >= GT_TEAM) && (g_gametype.integer != GT_JUMP))
	{
		level.warmupTime = -1;
		trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime));
		G_LogPrintf( "Warmup:\n" );
	}

	trap_SetConfigstring( CS_ROUNDSTAT, "0 0 0");

	if (g_swaproles.integer)
	{
		level.swaproles = 1;
	}

}

// 27
// Looks in /mapskins, loads all the .txt files, loops through them in sequence until it finds an occurence of a map name
// Then sets the string to whatever is after it. Defaults to 00

void FetchMapTeams(void)
{
	int 		  searchCount;
	int 		  j, r, last, len;
	fileHandle_t  f;
	char		  *cha;
	char		  search[MAX_QPATH * 10];
	char		  maper[MAX_QPATH];
	char		  mapname[MAX_QPATH];
	char		  string[4000];

	memset(&search[0], 0, MAX_QPATH * 10);
	searchCount = trap_FS_GetFileList("mapskins", ".txt", search, MAX_QPATH * 10);

	Com_Printf("^3Searched for MapSkins List: FOUND %d\n", searchCount);
	trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof(mapname));
	//strcpy(g_skins.string,"00");

	r	 = 0;
	last = 0;

	for (j = 0; j < searchCount; j++)
	{
		while (search[r] != 0)
		{
			r++;
		}
		memset(&maper[0], 0, MAX_QPATH);
		memcpy(&maper[0], &search[last], r - last); //voodoo string parsing, copy out between the /0's
		r	+= 1;
		last = r;

		//mapper is a filename, open it and spin through looking for our mapname.
		strcpy(string, "mapskins/");
		strcat(string, maper);
		len = trap_FS_FOpenFile( string, &f, FS_READ );
		Com_Printf("Checking %s ", string);

		memset(string, 0, 4000);

		if (f)
		{
			if (len > 4000)
			{
				len = 4000;
			}
			trap_FS_Read(string, len, f);
			trap_FS_FCloseFile( f );
		}
		Q_strlwr(string);
		Q_strlwr(mapname);
		len = strlen(string);
		cha = strstr(string, mapname);
		strcat(string, "\n");

		if (cha)
		{
			//Turns out q3 does not like the sscanf "%s".  So I do this hax bs instead.
			for (; cha[0] > 31; cha++)
			{
				if (&cha[0] > &string[len])
				{
					return;
				}
			}
			cha -= 2;
/*	   g_skins.string[0]=cha[0]; commented out due to removing gskins
	 g_skins.string[1]=cha[1];
	 g_skins.string[2]=0;

	 if ((g_skins.string[0]<'0')||(g_skins.string[0]>'3')) g_skins.string[0]='0';
	 if ((g_skins.string[1]<'0')||(g_skins.string[1]>'3')) g_skins.string[1]='0';

	 //sscanf(cha,mapname,g_flags.string);
	 Com_Printf("^3Set for: %s is %s\n",mapname,g_skins.string); */
			return;
		}
		Com_Printf("- Negatory.\n");
	}
//	 Com_Printf("^3Did not find a set, so %s uses %s\n",mapname,g_skins.string);
}

/*=---- Spawn Entties from a string ----=*/

void G_SpawnEntitiesFromString( void )
{
#if 0
	fileHandle_t  DebugFile;
	int 		  i, j, len;
	char		  newline = '\n';
#endif

	level.spawning				= qtrue;
	levelspawn[num_levelspawn].numSpawnVars = 0;

	if(!G_ParseSpawnVars())
	{
		level.spawning = qfalse;
		return;
	}
	SP_worldspawn();

	while(G_ParseSpawnVars())
	{
		if(num_levelspawn > MAX_SPAWNENTITIES)
		{
			G_Error( "Too many map entities" );
		}
		G_SpawnGEntityFromSpawnVars();
	}
	level.spawning = qfalse;
	// debug code
#if 0
	trap_FS_FOpenFile( "EntityDebug.txt", &DebugFile, FS_WRITE );

	for(i = 0; i < num_levelspawn; i++)
	{
		trap_FS_Write( levelspawn[i].spawnVarChars, levelspawn[i].numSpawnVarChars, DebugFile );
		trap_FS_Write( &newline, 1, DebugFile );
	}
	trap_FS_FCloseFile( DebugFile );
#endif
}

/*
================
GottaBeKD: Used when an ent targets a func_ut_playerSound trigger, right now, only in SP
Use_UTPlayerSound
================
*/
void Use_UTPlayerSound( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	gentity_t  *te;

	te		= G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
	te->s.eventParm = G_SoundIndex( ent->radioText );
	te->r.svFlags  |= SVF_BROADCAST;
}

/*QUAKED func_ut_playerSound (.5 .3 0) (-8 -8 -8) (8 8 8)
"name"		name of the wav file
*/
void SP_func_ut_playerSound( gentity_t *ent )
{
	char  *name;

	ent->flags |= FL_NO_HUMANS;

	G_SpawnString( "name", "", &name );

	if (name[0])
	{
		ent->radioText = (char *)bmalloc ( strlen ( name ) + 1 );
		strcpy ( ent->radioText, name );
	}

	ent->use = Use_UTPlayerSound;
}

/*
================
GottaBeKD: Used when an ent targets a func_ut_originSound trigger, right now, only in SP
Use_UTOriginSound
================
*/
void Use_UTOriginSound( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	gentity_t  *te;

	te				= G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
	te->s.eventParm = G_SoundIndex( ent->radioText );
	te->r.svFlags  |= SVF_BROADCAST;
}

/*QUAKED func_ut_originSound (.5 .3 0) (-8 -8 -8) (8 8 8)
"name"		name of the wav file
*/
void SP_func_ut_originSound( gentity_t *ent )
{
	char  *name;

	ent->flags |= FL_NO_HUMANS;

	G_SpawnString( "name", "", &name );

	if (name[0])
	{
		ent->radioText = (char *)bmalloc ( strlen ( name ) + 1 );
		strcpy ( ent->radioText, name );
	}

	ent->use = Use_UTOriginSound;
}

///////////////// JUMP MODE ENTITIES ///////////////////////
void SP_ut_jumpstart( gentity_t *ent ) {

    char *name;

    //Make sure we are playing jump mode
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    G_SpawnString("name", "unknown", &name);      // Name of the jump way
    G_SpawnInt("type", "1", &ent->jumpWayNum);    // Index of the jump way
    G_SpawnInt("color", "7", &ent->jumpWayColor); // @r00t:Jump way color, default to white

    ent->model            = G_NewString(name);    // Create a proper string for the way name
    ent->wait             = 0;                    // Wait time set to 0. We handle this in Use_JumpStartLine for single players
    ent->r.contents       = 0;
    ent->s.modelindex     = -1;
    ent->s.eType          = ET_JUMP_START;        // Entity type
    ent->s.pos.trType     = TR_STATIONARY;        // Put stationary since the entity doesn't move
    ent->use              = Use_JumpStartLine;    // Pointer function to be used when the entity is triggered by a a trigger_multiple

}

void SP_ut_jumpstop( gentity_t *ent ) {

    char *name;

    //Make sure we are playing jump mode
    if (g_gametype.integer != GT_JUMP) {
        return;
    }

    G_SpawnString("name", "unknown", &name);      // Name of the jump way
    G_SpawnInt("type", "1", &ent->jumpWayNum);    // Index of the jump way

    ent->model            = G_NewString(name);    // Create a proper string for the way name
    ent->wait             = 0;                    // Wait time set to 0
    ent->r.contents       = 0;
    ent->s.modelindex     = -1;
    ent->s.eType          = ET_JUMP_STOP;         // Entity type
    ent->s.pos.trType     = TR_STATIONARY;        // Put stationary since the entity doesn't move
    ent->use              = Use_JumpStopLine;     // Pointer function to be used when the entity is triggered by a a trigger_multiple

}


void SP_ut_jumpcancel( gentity_t *ent ) {

    char *name;

    //Make sure we are playing jump mode
    if (g_gametype.integer != GT_JUMP) {
            return;
    }

    G_SpawnString("name", "unknown", &name);      // Name of the jump way
    G_SpawnInt("type", "1", &ent->jumpWayNum);    // Index of the jump way

    ent->model          = G_NewString(name);      // Create a proper string for the way name
    ent->wait           = 0;                      // Wait time set to 0
    ent->r.contents     = 0;
    ent->s.modelindex   = -1;
    ent->s.eType        = ET_JUMP_CANCEL;         // Entity type
    ent->s.pos.trType   = TR_STATIONARY;          // Put stationary since the entity doesn't move
    ent->use            = Use_JumpTimerCancel;    // Pointer function to be used when the entity is triggered by a a trigger_multiple

}
