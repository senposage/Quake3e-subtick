// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_utils.c -- misc utility functions for game module

#include "g_local.h"
#include "../common/c_bmalloc.h"

typedef struct
{
	char   oldShader[MAX_QPATH];
	char   newShader[MAX_QPATH];
	float  timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

int	       remapCount = 0;
shaderRemap_t  remappedShaders[MAX_SHADER_REMAPS];

/**
 * $(function)
 */
void AddRemap(const char *oldShader, const char *newShader, float timeOffset)
{
	int  i;

	for (i = 0; i < remapCount; i++)
	{
		if (Q_stricmp(oldShader, remappedShaders[i].oldShader) == 0)
		{
			// found it, just update this one
			strcpy(remappedShaders[i].newShader, newShader);
			remappedShaders[i].timeOffset = timeOffset;
			return;
		}
	}

	if (remapCount < MAX_SHADER_REMAPS)
	{
		strcpy(remappedShaders[remapCount].newShader, newShader);
		strcpy(remappedShaders[remapCount].oldShader, oldShader);
		remappedShaders[remapCount].timeOffset = timeOffset;
		remapCount++;
	}
}

/**
 * $(function)
 */
const char *BuildShaderStateConfig(void)
{
	static char  buff[MAX_STRING_CHARS * 4];
	char	     out[(MAX_QPATH * 2) + 5];
	int	     i;

	memset(buff, 0, MAX_STRING_CHARS);

	for (i = 0; i < remapCount; i++)
	{
		Com_sprintf(out, (MAX_QPATH * 2) + 5, "%s=%s:%5.2f@", remappedShaders[i].oldShader, remappedShaders[i].newShader, remappedShaders[i].timeOffset);
		Q_strcat( buff, sizeof(buff), out);
	}
	return buff;
}

/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
================
G_FindConfigstringIndex

================
*/
int G_FindConfigstringIndex( char *name, int start, int max, qboolean create )
{
	int   i;
	char  s[MAX_STRING_CHARS];

	if (!name || !name[0])
	{
		return 0;
	}

	for (i = 1 ; i < max ; i++)
	{
		trap_GetConfigstring( start + i, s, sizeof(s));

		if (!s[0])
		{
			break;
		}

		if (!strcmp( s, name ))
		{
			return i;
		}
	}

	if (!create)
	{
		return 0;
	}

	if (i == max)
	{
		G_Error( "G_FindConfigstringIndex: overflow" );
	}

	trap_SetConfigstring( start + i, name );

	return i;
}

/**
 * $(function)
 */
int G_ModelIndex( char *name )
{
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, qtrue);
}

/**
 * $(function)
 */
int G_SoundIndex( char *name )
{
	return G_FindConfigstringIndex (name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

//=====================================================================

/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( team_t team, char *cmd )
{
	int  i;

	for (i = 0 ; i < level.maxclients ; i++)
	{
		if (level.clients[i].pers.connected == CON_CONNECTED)
		{
			if (level.clients[i].sess.sessionTeam == team)
			{
				trap_SendServerCommand( i, va("%s", cmd ));
			}
		}
	}
}


int HashStr(const char *s)
{
 int a = 1234567;
 int b = 2345678;
 int c = 3456789;
 while(*s) {
  a=a+*s;
  b=a+b+c;
  c*=b;
  s++;
 }
// trap_Printf( va( "HashStr: %d %d %d\n", a,b,c));
 return b^a^c;
}

int HashStrLC(const char *s)
// converts input to lower case
{
 int a = 1234567;
 int b = 2345678;
 int c = 3456789;
 while(*s) {
  int h = *s;
  if (h>='A' && h<='Z') h+=32;
  a=a+*s;
  b=a+b+c;
  c*=b;
  s++;
 }
// trap_Printf( va( "HashStr: %d %d %d\n", a,b,c));
 return b^a^c;
}

gentity_t *G_FindClass (gentity_t *from, const char *match)
{
	char  *s;
	int matchhash;

//	  gentity_t *res;
//	  res = G_Find(from,FOFS(classname),match);

	HASHSTR(match,matchhash);

	if (!from)
	{
		from = g_entities;
	}
	else
	{
		from++;
	}

	for ( ; from < &g_entities[level.num_entities] ; from++)
	{
		if (!from->inuse)
		{
			continue;
		}
		s = from->classname;

		if (!s)
		{
			continue;
		}

		if (from->classhash==matchhash)
		{
/*
	 if (res!=from) {
	  if (res) {
	   trap_Printf( va( "G_FindClass: Bad result for %s (%s:%d) != (%s:%d)\n", match, res->classname,res->classhash,from->classname,from->classhash));
	  } else {
	   trap_Printf( va( "G_FindClass: Fake result for %s (%s:%d)\n", match, from->classname,from->classhash));
	  }
	 }
*/
//			  if (!Q_stricmp (s, match))
//			  {
				 return from;
//			  }
		}
	}
/*
	 if (res!=from && res) {
	  trap_Printf( va( "G_FindClass: No result for %s (%s:%d)\n", match, res->classname,res->classhash));
	 }
*/
	return NULL;
}

gentity_t *G_FindClassHash (gentity_t *from, int matchhash)
{
	char  *s;

	if (!from)
	{
		from = g_entities;
	}
	else
	{
		from++;
	}

	for ( ; from < &g_entities[level.num_entities] ; from++)
	{
		if (!from->inuse)
		{
			continue;
		}
		s = from->classname;

		if (!s)
		{
			continue;
		}

		if (from->classhash==matchhash)
		{
				 return from;
		}
	}
	return NULL;
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find (gentity_t *from, size_t fieldofs, const char *match)
{
	char  *s;

	if (!from)
	{
		from = g_entities;
	}
	else
	{
		from++;
	}

	for ( ; from < &g_entities[level.num_entities] ; from++)
	{
		if (!from->inuse)
		{
			continue;
		}
		s = *(char **)((char *)from + fieldofs);

		if (!s)
		{
			continue;
		}

		if (!Q_stricmp (s, match))
		{
			return from;
		}
	}

	return NULL;
}

/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES 32

/**
 * $(function)
 */
gentity_t *G_PickTarget (char *targetname)
{
	gentity_t  *ent        = NULL;
	int	   num_choices = 0;
	gentity_t  *choice[MAXCHOICES];

	if (!targetname)
	{
		G_Printf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);

		if (!ent)
		{
			break;
		}
		choice[num_choices++] = ent;

		if (num_choices == MAXCHOICES)
		{
			break;
		}
	}

	if (!num_choices)
	{
		G_Printf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}

/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets( gentity_t *ent, gentity_t *activator ) {
	gentity_t  *t;

	if (!ent) {
		return;
	}

	/*	if (ent->targetShaderName && ent->targetShaderNewName) {
		float f = level.time * 0.001;
		AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
		trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
	} */

	if (!ent->target) {
		return;
	}

	t = NULL;

	while ((t = G_Find (t, FOFS(targetname), ent->target)) != NULL) {
		if (t == ent) {
			G_Printf ("WARNING: Entity used itself.\n");
		}
		else {
			if (t->use) {
				t->use (t, ent, activator);
			}
		}

		if (!ent->inuse) {
			G_Printf("entity was removed while using targets\n");
			return;
		}
	}
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float *tv( float x, float y, float z )
{
	static int     index;
	static vec3_t  vecs[8];
	float	       *v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v     = vecs[index];
	index = (index + 1) & 7;

	v[0]  = x;
	v[1]  = y;
	v[2]  = z;

	return v;
}

/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char *vtos( const vec3_t v )
{
	static int   index;
	static char  str[8][32];
	char	     *s;

	// use an array so that multiple vtos won't collide
	s     = str[index];
	index = (index + 1) & 7;

	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}

/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir )
{
	static vec3_t  VEC_UP	    = { 0, -1, 0};
	static vec3_t  MOVEDIR_UP   = { 0, 0, 1};
	static vec3_t  VEC_DOWN     = { 0, -2, 0};
	static vec3_t  MOVEDIR_DOWN = { 0, 0, -1};

	if (VectorCompare (angles, VEC_UP))
	{
		VectorCopy (MOVEDIR_UP, movedir);
	}
	else if (VectorCompare (angles, VEC_DOWN))
	{
		VectorCopy (MOVEDIR_DOWN, movedir);
	}
	else
	{
		AngleVectorsF (angles, movedir, NULL, NULL);
	}
	VectorClear( angles );
}

/**
 * $(function)
 */
float vectoyaw( const vec3_t vec )
{
	float  yaw;

	if ((vec[YAW] == 0) && (vec[PITCH] == 0))
	{
		yaw = 0;
	}
	else
	{
		if (vec[PITCH])
		{
			yaw = (atan2( vec[YAW], vec[PITCH]) * 180 / M_PI);
		}
		else if (vec[YAW] > 0)
		{
			yaw = 90;
		}
		else
		{
			yaw = 270;
		}

		if (yaw < 0)
		{
			yaw += 360;
		}
	}

	return yaw;
}

/**
 * $(function)
 */
void G_InitGentity( gentity_t *e )
{
	e->inuse      = qtrue;
	e->classname  = "noclass";
	e->classhash  = HSH_noclass;
	e->s.number   = e - g_entities;
	e->r.ownerNum = ENTITYNUM_NONE;
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn( void )
{
	int	   i, force;
	gentity_t  *e;

//	char		OutText[];

	e = NULL;	// shut up warning
	i = 0;		// shut up warning

	for (force = 0 ; force < 2 ; force++)
	{
		// if we go through all entities and can't find one to free,
		// override the normal minimum times before use
		e = &g_entities[MAX_CLIENTS];

		for (i = MAX_CLIENTS ; i < level.num_entities ; i++, e++)
		{
			if (e->inuse)
			{
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if (!force && (e->freetime > level.startTime + 2000) && (level.time - e->freetime < 1000))
			{
				continue;
			}

			// reuse this slot
			G_InitGentity( e );
			return e;
		}

		if (i != MAX_GENTITIES)
		{
			break;
		}
	}

	if (i == ENTITYNUM_MAX_NORMAL)
	{
		for (i = 0; i < MAX_GENTITIES; i++)
		{
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		G_Error( "G_Spawn: no free entities" );
	}

	// open up a new slot
	level.num_entities++;

	if(level.num_entities > MAX_GENTITIES)
	{
		G_Error( "G_Spawn: no free entities" );
	}
/*
	{
		extern local_backup_spawn_t *levelspawn;
	fileHandle_t DebugFile;
	trap_FS_FOpenFile( "EntityDebug.txt", &DebugFile, FS_WRITE );

	for( i = 0; i < num_levelspawn; i++ ) {
		trap_FS_Write( levelspawn[ i ].spawnVarChars, levelspawn[ i ].numSpawnVarChars, DebugFile );
		trap_FS_Write( &newline, 1, DebugFile );
	}
	trap_FS_FCloseFile( DebugFile );
	}
	*/

	trap_LocateGameData( level.gentities, level.num_entities, sizeof(gentity_t),
			     &level.clients[0].ps, sizeof(level.clients[0]));
	G_InitGentity( e );
	return e;
}

/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void )
{
	int	   i;
	gentity_t  *e;

	e = &g_entities[MAX_CLIENTS];

	for (i = MAX_CLIENTS; i < level.num_entities; i++, e++)
	{
		if (e->inuse)
		{
			continue;
		}
		memset( e, 0, sizeof(gentity_t));
		// slot available
		return qtrue;
	}
	return qfalse;
}

/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
void G_FreeEntity( gentity_t *ed )
{
	// Check for invalid pointer.
	if (!ed)
	{
		return;
	}

	trap_UnlinkEntity (ed); 	// unlink from world

	// Free spawn memory
	if (ed->spawn)
	{
		if (ed->spawn->group)
			bfree(ed->spawn->group);
		bfree(ed->spawn);
	}

	// Free strings
	if (ed->model)
	{
		G_FreeString(ed->model);
		ed->model = NULL;
	}

	if (ed->classname) {
		G_FreeString(ed->classname);
		ed->classname = NULL;
	}

	if (ed->model2) {
		G_FreeString(ed->model2);
		ed->model2 = NULL;
	}

	if (ed->target) {
		G_FreeString(ed->target);
		ed->target = NULL;
	}

	if (ed->targetname) {
		G_FreeString(ed->targetname);
		ed->targetname = NULL;
	}

	if (ed->message) {
		G_FreeString(ed->message);
		ed->message = NULL;
	}

	if (ed->team) {
		G_FreeString(ed->team);
		ed->team = NULL;
	}

	if (ed->targetShaderName) {
		G_FreeString(ed->targetShaderName);
		ed->targetShaderName = NULL;
	}

	if (ed->targetShaderNewName) {
		G_FreeString(ed->targetShaderNewName);
		ed->targetShaderNewName = NULL;
	}

	if (ed->neverFree)
	{
		return;
	}

	memset( ed, 0, sizeof(gentity_t));    // required for thrown weapons - DensitY
	ed->classname = "freed";
	ed->classhash = HSH_freed;
	ed->freetime  = level.time;
	ed->inuse     = qfalse;
}

/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( vec3_t origin, int event )
{
	gentity_t  *e;
	vec3_t	   snapped;

	e		  = G_Spawn();
	e->s.eType	  = ET_EVENTS + CSE_ENC(event);

	e->classname	  = "tempEntity";
	e->classhash	  = HSH_tempEntity;
	e->eventTime	  = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );

	// find cluster for PVS
	trap_LinkEntity( e );

	return e;
}

/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
void G_KillBox (gentity_t *ent)
{
	int	   i, num;
	int	   touch[MAX_GENTITIES];
	gentity_t  *hit;
	vec3_t	   mins, maxs;

	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i = 0 ; i < num ; i++)
	{
		hit = &g_entities[touch[i]];

		if (!hit->client)
		{
			continue;
		}

		// nail it
		G_Damage ( hit, ent, ent, NULL, NULL,
			   100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG, HL_UNKNOWN);
	}
}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm )
{
	if (!ent->client)
	{
		return;
	}
	BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}

/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm )
{
	int  bits;

	if (!event)
	{
		G_Printf( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
		return;
	}
	event = CSE_ENC(event);
	// clients need to add the event in playerState_t instead of entityState_t
	if (ent->client)
	{
		bits				  = ent->client->ps.externalEvent & EV_EVENT_BITS;
		bits				  = (bits + EV_EVENT_BIT1) & EV_EVENT_BITS;
		ent->client->ps.externalEvent	  = event | bits;
		ent->client->ps.externalEventParm = eventParm;
		ent->client->ps.externalEventTime = level.time;
	}
	else
	{
		bits		 = ent->s.event & EV_EVENT_BITS;
		bits		 = (bits + EV_EVENT_BIT1) & EV_EVENT_BITS;
		ent->s.event	 = event | bits;
		ent->s.eventParm = eventParm;
	}
	ent->eventTime = level.time;
}

/*
=============
G_Sound
=============
*/
void G_Sound( gentity_t *ent, int channel, int soundIndex )
{
	G_AddEvent ( ent, EV_GENERAL_SOUND, soundIndex );
}

//==============================================================================

/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, vec3_t origin )
{
	VectorCopy( origin, ent->s.pos.trBase );
	ent->s.pos.trType     = TR_STATIONARY;
	ent->s.pos.trTime     = 0;
	ent->s.pos.trDuration = 0;
	VectorClear( ent->s.pos.trDelta );

	VectorCopy( origin, ent->r.currentOrigin );
}

/*
================
DebugLine

  debug polygons only work when running a local game
  with r_debugSurface set to 2
================
*/
int DebugLine(vec3_t start, vec3_t end, int color)
{
	vec3_t	points[4], dir, cross, up = { 0, 0, 1};
	float	dot;

	VectorCopy(start, points[0]);
	VectorCopy(start, points[1]);
	//points[1][2] -= 2;
	VectorCopy(end, points[2]);
	//points[2][2] -= 2;
	VectorCopy(end, points[3]);

	VectorSubtract(end, start, dir);
	VectorNormalizeNR(dir);
	dot = DotProduct(dir, up);

	if ((dot > 0.99) || (dot < -0.99))
	{
		VectorSet(cross, 1, 0, 0);
	}
	else
	{
		CrossProduct(dir, up, cross);
	}

	VectorNormalizeNR(cross);

	VectorMA(points[0], 2, cross, points[0]);
	VectorMA(points[1], -2, cross, points[1]);
	VectorMA(points[2], -2, cross, points[2]);
	VectorMA(points[3], 2, cross, points[3]);

	return trap_DebugPolygonCreate(color, 4, points);
}

///////////////////////////////////
// STATS
//////////////////////////////////

char StatLocStrings[S_MAXLOCS][16] = {
	"HEAD",
	"TORSO",
	"LEGS",
	"ARMS",
};

char  StatWepStrings[S_WEAPONS][16] = {
	{ "G36"    }, //0
	{ "AK103"  }, //1
	{ "LR300"  }, //2
	{ "M4"	   }, //3
	{ "MP5K"   }, //4
	{ "UMP"    }, //5
	{ "SR8"    }, //6
	{ "PSG-1"  }, //7
	{ "DE"	   }, //8
	{ "BERETTA"}, //9
	{ "NEGEV"  }, //10
	{ "SPAS"   }, //11
	{ "GLOCK"  }, //12
	{ "COLT"   }, //13
	{ "MAC11"  }, //14
	//{ "P90"      }, //12
	//{ "BENELLI"  }, //13
	//{ "MAGNUM"   }, //15
	//{ "D-BERETTA"}, //16
	//{ "D-DE"     }, //17
	//{ "D-GLOCK"  }, //18
	//{ "D-MAGNUM" }, //19
};

char  StatLeaderboardStrings[ST_L_MAX][64] = {
	"*dummy",
	"Golden Scope Award",
	"Golden Trigger Award",
	"Dirty Harry Award",
	"The UnaBomber Award",
	"Needs a Bigger Tent",
	"Mr Magoo Award",
	"Nobody Loves You",
	"Wooden Spoon Award",
	"Defender of the Mushroom Kingdom",
	"The Terminator Award",
	"Trigger Happy Award",
	"The Neck Bones Connected To The",
	"Needs to Duck More",
	"Worlds Worst Terrorist Award",
	"Touch Of Death Award",
};

/**
 * $(function)
 */
int StatsEngineWepID(int w) {

	switch (w) {

		case UT_WP_G36:
			return 0;
			break;

		case UT_WP_AK103:
			return 1;
			break;

		case UT_WP_LR:
			return 2;
			break;

		case UT_WP_M4:
			return 3;
			break;

		case UT_WP_MP5K:
			return 4;
			break;

		case UT_WP_UMP45:
			return 5;
			break;

		case UT_WP_SR8:
			return 6;
			break;

		case UT_WP_PSG1:
			return 7;
			break;

		case UT_WP_DEAGLE:
			return 8;
			break;

		case UT_WP_BERETTA:
			return 9;
			break;

		case UT_WP_NEGEV:
			return 10;
			break;

		case UT_WP_SPAS12:
			return 11;
			break;

		case UT_WP_GLOCK:
			return 12;
			break;

		case UT_WP_COLT1911:
            return 13;
            break;

		case UT_WP_MAC11:
            return 14;
            break;

		/*case UT_WP_P90:
			return 12;
			break;

		case UT_WP_BENELLI:
			return 13;
			break;

		case UT_WP_MAGNUM:
			return 15;
			break;

		case UT_WP_DUAL_BERETTA: return 16;
		case UT_WP_DUAL_DEAGLE:  return 17;
		case UT_WP_DUAL_GLOCK:	 return 18;
		case UT_WP_DUAL_MAGNUM:  return 19;*/
	}
	return -1;
}

/**
 * $(function)
 */
int StatsEngineLocID(int l) {

	switch (l) {

		case HL_HEAD:
		case HL_HELMET:
			return 0;
			break;

		case HL_TORSO:
		case HL_VEST:
		case HL_GROIN:
		case HL_BUTT:
			return 1;
			break;

		case HL_LEGUL:
		case HL_LEGUR:
		case HL_LEGLL:
		case HL_LEGLR:
		case HL_FOOTL:
		case HL_FOOTR:
			return 2;
			break;

		case HL_ARML:
		case HL_ARMR:
			return 3;
			break;
	}
	return -1;
}

/**
 * $(function)
 */
int StatsEngineModID(int m) {

	switch (m) {

		case UT_MOD_G36:
			return 0;
			break;

		case UT_MOD_AK103:
			return 1;
			break;

		case UT_MOD_LR:
			return 2;
			break;

		case UT_MOD_M4:
			return 3;
			break;

		case UT_MOD_MP5K:
			return 4;
			break;

		case UT_MOD_UMP45:
			return 5;
			break;

		case UT_MOD_SR8:
			return 6;
			break;

		case UT_MOD_PSG1:
			return 7;
			break;

		case UT_MOD_DEAGLE:
			return 8;
			break;

		case UT_MOD_BERETTA:
			return 9;
			break;

		case UT_MOD_NEGEV:
			return 10;
			break;

		case UT_MOD_SPAS:
			return 11;
			break;

		case UT_MOD_GLOCK:
			return 12;
			break;

		case UT_MOD_COLT1911:
            return 13;
            break;

		case UT_MOD_MAC11:
            return 14;
            break;

		/*case UT_MOD_P90:
			return 12;
			break;

		case UT_MOD_BENELLI:
			return 13;

		case UT_MOD_MAGNUM:
			return 15;*/
	}
	return -1;
}

/**
 * $(function)
 */
void StatsEngineDeath(int shooter, int mod, int victim)
{
	gentity_t  *ent = NULL;
	gentity_t  *vic = NULL;
	int	   weaponID;

	if (shooter == -1)
	{
		return;
	}

	if ((victim == -1) || (victim >= MAX_CLIENTS))
	{
		vic = NULL;
	}
	else
	{
		vic = &g_entities[victim];
	}
	ent	 = &g_entities[shooter];

	weaponID = StatsEngineModID(mod);

	if (weaponID != -1) //Was a weapon
	{
		ent->client->pers.statistics.Weapon[weaponID].Kills++;
		vic->client->pers.statistics.Weapon[weaponID].Deaths++;
		Stats_AddTK(ent, vic);
	}
	else
	{
		//Add it to one of the other catagorys
		switch (mod)
		{
			case MOD_UNKNOWN:
			case MOD_WATER:
			case MOD_SLIME:
			case MOD_LAVA:
			case MOD_CRUSH:
			case MOD_TELEFRAG:
			case MOD_TARGET_LASER:
			case MOD_TRIGGER_HURT:
				{
					vic->client->pers.statistics.Deaths[ST_ENVIRONMENT]++;
					break;
				}

			case UT_MOD_BLED:
				{
					ent->client->pers.statistics.Kills[ST_BLEED]++;
					vic->client->pers.statistics.Deaths[ST_BLEED]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case UT_MOD_KNIFE:
				{
					ent->client->pers.statistics.Kills[ST_KNIFESTAB]++;
					vic->client->pers.statistics.Deaths[ST_KNIFESTAB]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case UT_MOD_KNIFE_THROWN:
				{
					ent->client->pers.statistics.Kills[ST_KNIFETHROWN]++;
					vic->client->pers.statistics.Deaths[ST_KNIFETHROWN]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case UT_MOD_KICKED:
				{
					ent->client->pers.statistics.Kills[ST_KICKED]++;
					vic->client->pers.statistics.Deaths[ST_KICKED]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case UT_MOD_GOOMBA:
				{
					ent->client->pers.statistics.Kills[ST_GOOMBA]++;
					vic->client->pers.statistics.Deaths[ST_GOOMBA]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case UT_MOD_HEGRENADE:
				{
					if (ent == vic)
					{
						vic->client->pers.statistics.Deaths[ST_GRENSELF]++;
					}
					else
					{
						ent->client->pers.statistics.Kills[ST_GREN]++;
						vic->client->pers.statistics.Deaths[ST_GREN]++;
						Stats_AddTK(ent, vic);
					}
					break;
				}

			case UT_MOD_HK69:
				{
					if (ent == vic)
					{
						vic->client->pers.statistics.Deaths[ST_HKSELF]++;
					}
					else
					{
						ent->client->pers.statistics.Kills[ST_HKEXPLODE]++;
						vic->client->pers.statistics.Deaths[ST_HKEXPLODE]++;
						Stats_AddTK(ent, vic);
					}
					break;
				}

			case UT_MOD_HK69_HIT:
				{
					ent->client->pers.statistics.Kills[ST_HKHIT]++;
					vic->client->pers.statistics.Deaths[ST_HKHIT]++;
					Stats_AddTK(ent, vic);
					break;
				}

			case MOD_FALLING:
				{
					vic->client->pers.statistics.Deaths[ST_LEMMING]++;
					break;
				}

			case MOD_SUICIDE:
				{
					vic->client->pers.statistics.Deaths[ST_SUICIDE]++;
					break;
				}

			case UT_MOD_SPLODED:
				{
					vic->client->pers.statistics.Deaths[ST_SPLODED]++;
					break;
				}

			case UT_MOD_SLAPPED:
			case UT_MOD_NUKED:
			case UT_MOD_SMITED: //@Barbatos
				{
					vic->client->pers.statistics.Deaths[ST_ADMIN]++;
					break;
				}

			case UT_MOD_FLAG:
				{
					vic->client->pers.statistics.Deaths[ST_FLAG]++;
					break;
				}

			case UT_MOD_BOMBED:
				{
					ent->client->pers.statistics.Kills[ST_BOMBED]++;
					vic->client->pers.statistics.Deaths[ST_BOMBED]++;

//	      Stats_AddTK(ent,vic);
					break;
				}
		}
	}

	//Update kill/death ratios
	if (ent)
	{
		Stats_UpdateKD(ent);
	}

	if (vic)
	{
		Stats_UpdateKD(vic);
	}

	//Update the leaderboard
	Stats_Leaderboard();
}

/**
 * $(function)
 */
void Stats_UpdateKD(gentity_t *ent)
{
	int  j;

	j					= Stats_CalcRatio(Stats_GetTotalKills(ent), Stats_GetTotalDeaths(ent));
	ent->client->pers.statistics.KillRatio	= Stats_GetTotalKills(ent) / j;
	ent->client->pers.statistics.DeathRatio = Stats_GetTotalDeaths(ent) / j;

	if (Stats_GetTotalDeaths(ent) == 0)
	{
		ent->client->pers.statistics.KdivD = 0;
	}
	else
	{
		ent->client->pers.statistics.KdivD = (float)Stats_GetTotalKills(ent) / (float)Stats_GetTotalDeaths(ent);
	}
}

/**
 * $(function)
 */
void Stats_AddTK(gentity_t *ent, gentity_t *vic)
{
	if (vic && ent)
	{
		if (OnSameTeam(vic, ent) && (ent != vic))
		{
			ent->client->pers.statistics.Kills[ST_TK]++;
			vic->client->pers.statistics.Deaths[ST_TK]++;
		}
	}
}

/**
 * $(function)
 */
void StatsEngineFire (int shooter, int wepID, int hloc, int victim, float damage) {

	gentity_t  *ent = NULL;
	gentity_t  *vic = NULL;
	int	   weaponID;
	int	   hitloc;

	if (shooter == -1) {
		return;
	}

	if ((victim == -1) || (victim >= MAX_CLIENTS)) {
		vic = NULL;
	} else {
		vic = &g_entities[victim];
	}

	ent	     = &g_entities[shooter];
	weaponID = StatsEngineWepID(wepID);
	hitloc	 = StatsEngineLocID(hloc);

	if (!vic) {
		hitloc = -1; //no hit locations unless we have a victim
	}

	if (weaponID == -1) {
		return;      //NFI
	}

	//Ok write what we know out
	ent->client->pers.statistics.Weapon[weaponID].BulletsFired++;

	if (hitloc > -1) {

		//Record the hit for this weapon
		ent->client->pers.statistics.Weapon[weaponID].BulletsHit++;
		ent->client->pers.statistics.Weapon[weaponID].ZoneHits[hitloc]++;
		ent->client->pers.statistics.Weapon[weaponID].TotalDamageDone += damage;

		if (vic) {

			//record being hit by this weapon
			vic->client->pers.statistics.Weapon[weaponID].OthersHits[hitloc]++;
			vic->client->pers.statistics.Weapon[weaponID].TotalDamageTaken += damage;

			if (OnSameTeam(ent, vic)) {
				ent->client->pers.statistics.Weapon[weaponID].TotalDamageDoneTeam  += damage;
				ent->client->pers.statistics.Weapon[weaponID].BulletsHitTeam++;
				vic->client->pers.statistics.Weapon[weaponID].TotalDamageTakenTeam += damage;
			}
		}
	}
	else
	{
	    // Fenix: wtf was this? was checking hit on HL_BODY which was an unused hitloc?!?

        //don't record it as a miss if it hit body
        //if (hloc != HL_BODY) {
            ent->client->pers.statistics.Weapon[weaponID].BulletsMissed++;
        //}
        /*
        else {
            //SPAS
            if (damage == 0) {
                ent->client->pers.statistics.Weapon[weaponID].BulletsMissed++;
            }
            else {

                ent->client->pers.statistics.Weapon[weaponID].BulletsHit++;
                ent->client->pers.statistics.Weapon[weaponID].TotalDamageDone += damage;

                if (vic) {

                    //record being hit by this weapon
                    vic->client->pers.statistics.Weapon[weaponID].TotalDamageTaken += damage;

                    if (OnSameTeam(ent, vic)) {
                        ent->client->pers.statistics.Weapon[weaponID].TotalDamageDoneTeam  += damage;
                        ent->client->pers.statistics.Weapon[weaponID].BulletsHitTeam++;
                        vic->client->pers.statistics.Weapon[weaponID].TotalDamageTakenTeam += damage;
                    }
                }
            }
        }
        */
	}

	//Update accuracy var
	if (ent->client->pers.statistics.Weapon[weaponID].BulletsFired == 0) {
		ent->client->pers.statistics.Weapon[weaponID].Accuracy = 0;
	} else {
		ent->client->pers.statistics.Weapon[weaponID].Accuracy = 100.0f - ((float)ent->client->pers.statistics.Weapon[weaponID].BulletsMissed / (float)ent->client->pers.statistics.Weapon[weaponID].BulletsFired) * 100.0f;
	}

}

/**
 * $(function)
 */
void StatsEnginePrint(gentity_t *sent) {

	int	       r, j;
	int	       cl = sent->client->ps.clientNum;
	float	   percent;
	int	       GrandTotalDmg = 0;
	char	   arg[128];
	gentity_t  *ent;
	char	   buffer[2048];
	int	       firedgun = -1;

	if (trap_Argc() < 3) {
		trap_SendServerCommand(sent->client->ps.clientNum, va("print \"Use the stats UI.\""));
		return;
	}

	//Grab the player details
	trap_Argv( 1, arg, sizeof(arg));

	cl = atoi(arg);

	if ((cl < 0) || (cl > MAX_CLIENTS - 1))
	{
		return;
	}
	ent = &g_entities[cl];

	if (ent->inuse == qfalse)
	{
		return;
	}

	///Grab the catagory we're interested in
	trap_Argv( 2, arg, sizeof(arg));
	j = -1;

//    G_Printf(va("Stats arg2:%s\n",arg));
	for (r = 0; r < S_WEAPONS; r++)
	{
		if (!Q_stricmp(arg, StatWepStrings[r]))
		{
			j = r;
			break;
		}
	}

	if (j == -1)
	{
		//Not a known weapon, do something else
		buffer[0] = 0;

		if (!Q_stricmpn(arg, "AWARDS", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			if (level.leaderboard[ST_L_INITIALIZED] == -1)
			{
				for (r = 1; r < ST_L_MAX; r++)
				{
					if (level.leaderboard[r] > -1)
					{
						Q_strcat(buffer, sizeof(buffer), va("%s: %s\n", StatLeaderboardStrings[r], g_entities[level.leaderboard[r]].client->pers.netname  ));
					}
				}
			}
		}

		if (!Q_stricmpn(arg, "EXPLOSIONS", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nGrenade Kills: %i\n", ent->client->pers.statistics.Kills[ST_GREN]));
			Q_strcat(buffer, sizeof(buffer), va("HK69 Kills: %i\n", ent->client->pers.statistics.Kills[ST_HKEXPLODE]));
			Q_strcat(buffer, sizeof(buffer), va("HK69 Impact Kills: %i\n", ent->client->pers.statistics.Kills[ST_HKHIT]));

			Q_strcat(buffer, sizeof(buffer), va("\nGrenade Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_GREN]));
			Q_strcat(buffer, sizeof(buffer), va("Self-Inflicted Grenade Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_GRENSELF]));
			Q_strcat(buffer, sizeof(buffer), va("HK69 Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_HKEXPLODE]));
			Q_strcat(buffer, sizeof(buffer), va("Self-Inflicted HK69 Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_HKSELF]));
			Q_strcat(buffer, sizeof(buffer), va("HK69 Impact Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_HKHIT]));

			Q_strcat(buffer, sizeof(buffer), va("Bombpack Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_BOMBED]));
			Q_strcat(buffer, sizeof(buffer), va("'Sploded!: %i\n", ent->client->pers.statistics.Deaths[ST_SPLODED]));
		}

		if (!Q_stricmpn(arg, "MELEE", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nKnife Kills: %i\n", ent->client->pers.statistics.Kills[ST_KNIFESTAB]));
			Q_strcat(buffer, sizeof(buffer), va("Thrown Knife Kills: %i\n", ent->client->pers.statistics.Kills[ST_KNIFETHROWN]));
			Q_strcat(buffer, sizeof(buffer), va("Bleedout Kills: %i\n", ent->client->pers.statistics.Kills[ST_BLEED]));
			Q_strcat(buffer, sizeof(buffer), va("Boot Kills: %i\n", ent->client->pers.statistics.Kills[ST_KICKED]));
			Q_strcat(buffer, sizeof(buffer), va("Goomba Kills: %i\n", ent->client->pers.statistics.Kills[ST_GOOMBA]));

			Q_strcat(buffer, sizeof(buffer), va("\nKnife Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_KNIFESTAB]));
			Q_strcat(buffer, sizeof(buffer), va("Thrown Knife Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_KNIFETHROWN]));
			Q_strcat(buffer, sizeof(buffer), va("Bleedout Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_BLEED]));
			Q_strcat(buffer, sizeof(buffer), va("Boot Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_KICKED]));
			Q_strcat(buffer, sizeof(buffer), va("Goomba Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_GOOMBA]));
		}

		if (!Q_stricmpn(arg, "MISC", strlen(arg))) //MISC DEATHS
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nHot-Potato Flag Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_FLAG]));
			Q_strcat(buffer, sizeof(buffer), va("Admin Assasinations: %i\n", ent->client->pers.statistics.Deaths[ST_ADMIN]));
			Q_strcat(buffer, sizeof(buffer), va("Environmental Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_ENVIRONMENT]));
			Q_strcat(buffer, sizeof(buffer), va("Lemming Deaths: %i\n", ent->client->pers.statistics.Deaths[ST_LEMMING]));
			Q_strcat(buffer, sizeof(buffer), va("Suicides: %i\n", ent->client->pers.statistics.Deaths[ST_SUICIDE]));
		}

		if (!Q_stricmpn(arg, "SUMMARY", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nTotal Kills: %i\n", Stats_GetTotalKills(ent)));
			Q_strcat(buffer, sizeof(buffer), va("Total Deaths: %i\n", Stats_GetTotalDeaths(ent)));

			Q_strcat(buffer, sizeof(buffer), va("Kills/Deaths Ratio: ~%i:%i\n", ent->client->pers.statistics.KillRatio, ent->client->pers.statistics.DeathRatio ));

			Q_strcat(buffer, sizeof(buffer), va("Team Kills: %i | Team Killed: %i\n", ent->client->pers.statistics.Kills[ST_TK], ent->client->pers.statistics.Deaths[ST_TK]));
			Q_strcat(buffer, sizeof(buffer), va("Total Damage Done with Bullets: %i\n", Stats_GetTotalDamageGiven(ent)));

			Q_strcat(buffer, sizeof(buffer), va("\nPlayer Hit: Head: %i | Torso : %i | Legs: %i | Arms: %i \n", Stats_GetTotalLocations(ent, 0, qfalse),
							    Stats_GetTotalLocations(ent, 1, qfalse),
							    Stats_GetTotalLocations(ent, 2, qfalse),
							    Stats_GetTotalLocations(ent, 3, qfalse)
							    ));
			Q_strcat(buffer, sizeof(buffer), va("Player Was Hit: Head: %i | Torso : %i | Legs: %i | Arms: %i \n", Stats_GetTotalLocations(ent, 0, qtrue),
							    Stats_GetTotalLocations(ent, 1, qtrue),
							    Stats_GetTotalLocations(ent, 2, qtrue),
							    Stats_GetTotalLocations(ent, 3, qtrue)
							    ));

			firedgun = Stats_GetMostFiredGun(ent);

			if (firedgun > -1)
			{
				Q_strcat(buffer, sizeof(buffer), va("\nMost Fired Gun: %s\n", StatWepStrings[firedgun] ));
				Q_strcat(buffer, sizeof(buffer), va("Accuracy with it: %1.1f percent\n", ent->client->pers.statistics.Weapon[firedgun].Accuracy ));
				Q_strcat(buffer, sizeof(buffer), va("Kills with it: %i\n", ent->client->pers.statistics.Weapon[firedgun].Kills ));
			}
		}

		if (!Q_stricmpn(arg, "CTF", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nFlag Captures: %i\n", ent->client->pers.statistics.Scores[ST_CAPTURE_FLAG]  ));
			Q_strcat(buffer, sizeof(buffer), va("Flag Returns: %i\n", ent->client->pers.statistics.Scores[ST_RETURN_FLAG]  ));

			Q_strcat(buffer, sizeof(buffer), va("\nFlag Carrier Kills: %i\n", ent->client->pers.statistics.Scores[ST_KILL_FC]  ));
			Q_strcat(buffer, sizeof(buffer), va("Prevented Captures: %i\n", ent->client->pers.statistics.Scores[ST_STOP_CAP]  ));
			Q_strcat(buffer, sizeof(buffer), va("Flag Protection Kills: %i\n", ent->client->pers.statistics.Scores[ST_PROTECT_FLAG]  ));
		}

		if (!Q_stricmpn(arg, "BOMBMODE", strlen(arg)))
		{
			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Category: %s\n", arg));

			Q_strcat(buffer, sizeof(buffer), va("\nBomb Plants: %i\n", ent->client->pers.statistics.Scores[ST_BOMB_PLANT]  ));
			Q_strcat(buffer, sizeof(buffer), va("Successful Bombings: %i\n", ent->client->pers.statistics.Scores[ST_BOMB_BOOM]  ));
			Q_strcat(buffer, sizeof(buffer), va("Folks Killed by your Bombings: %i\n", ent->client->pers.statistics.Kills[ST_BOMBED]  ));

			Q_strcat(buffer, sizeof(buffer), va("\nBomb Defusals: %i\n", ent->client->pers.statistics.Scores[ST_BOMB_DEFUSE]  ));
			Q_strcat(buffer, sizeof(buffer), va("Killed Bomb Defusers: %i\n", ent->client->pers.statistics.Scores[ST_KILL_DEFUSE]  ));

			Q_strcat(buffer, sizeof(buffer), va("\nBomb Carrier Kills: %i\n", ent->client->pers.statistics.Scores[ST_KILL_BC] ));
			Q_strcat(buffer, sizeof(buffer), va("Bomb Carrier Protection Kills: %i\n", ent->client->pers.statistics.Scores[ST_PROTECT_BC] ));
		}

		//Send it off
		StatsSendBuffer(buffer, sent->client->ps.clientNum);

		return;
	}

	for (r = 0; r < S_WEAPONS; r++)
	{
		GrandTotalDmg += ent->client->pers.statistics.Weapon[r].TotalDamageDone;
	}

	//j=0;
	{
		//if (ent->client->pers.statistics.Weapon[j].BulletsFired>0 ||ent->client->pers.statistics.Weapon[j].TotalDamageTaken>0 )
		{
			//trap_SendServerCommand(ent->client->ps.clientNum , va("stats \"OMG THATS THE FUNKY SHIT BRWWOR\nNext Line\nand the line after!!\""));
			buffer[0] = 0;

			Q_strcat(buffer, sizeof(buffer), va("Player: %s\n", ent->client->pers.netname));
			Q_strcat(buffer, sizeof(buffer), va("Weapon: %s\n", StatWepStrings[j]));
			Q_strcat(buffer, sizeof(buffer), va("Accuracy: %1.1f percent\n", ent->client->pers.statistics.Weapon[j].Accuracy));
			Q_strcat(buffer, sizeof(buffer), va("Fired: %i | Hit: %i | Hit Team: %i | Missed: %i \n", ent->client->pers.statistics.Weapon[j].BulletsFired
							    , ent->client->pers.statistics.Weapon[j].BulletsHit
							    , ent->client->pers.statistics.Weapon[j].BulletsHitTeam
							    , ent->client->pers.statistics.Weapon[j].BulletsMissed ));

			Q_strcat(buffer, sizeof(buffer), va("\nKills: %i \n", ent->client->pers.statistics.Weapon[j].Kills));
			Q_strcat(buffer, sizeof(buffer), va("Deaths: %i \n", ent->client->pers.statistics.Weapon[j].Deaths));

			if (ent->client->pers.statistics.Weapon[j].TotalDamageDone != 0)
			{
				percent = ((float)ent->client->pers.statistics.Weapon[j].TotalDamageDoneTeam / (float)ent->client->pers.statistics.Weapon[j].TotalDamageDone) * 100.0f;
			}
			else
			{
				percent = 0;
			}

			Q_strcat(buffer, sizeof(buffer), va("\nDamage Done: %i | Damage To Team: %i (%1.1f percent)\n", ent->client->pers.statistics.Weapon[j].TotalDamageDone, ent->client->pers.statistics.Weapon[j].TotalDamageDoneTeam, percent));

			//trap_SendServerCommand(ent->client->ps.clientNum , va("print \"Grand Total Damage: %i\"",GrandTotalDmg));
			if (GrandTotalDmg != 0)
			{
				percent = ((float)ent->client->pers.statistics.Weapon[j].TotalDamageDone / (float)GrandTotalDmg) * 100.0f;
			}
			else
			{
				percent = 0;
			}
			Q_strcat(buffer, sizeof(buffer), va("This weapon does %1.1f percent of players total bullet damage output.\n", percent));

			if (j != 11)
			{
				Q_strcat(buffer, sizeof(buffer), va("Head: %i | Torso: %i | Legs: %i | Arms: %i \n",
								    ent->client->pers.statistics.Weapon[j].ZoneHits[0],
								    ent->client->pers.statistics.Weapon[j].ZoneHits[1],
								    ent->client->pers.statistics.Weapon[j].ZoneHits[2],
								    ent->client->pers.statistics.Weapon[j].ZoneHits[3]
								    ));
			}

			//Taken dmg
			if (ent->client->pers.statistics.Weapon[j].TotalDamageTaken != 0)
			{
				percent = ((float)ent->client->pers.statistics.Weapon[j].TotalDamageTakenTeam / (float)ent->client->pers.statistics.Weapon[j].TotalDamageTaken) * 100.0f;
			}
			else
			{
				percent = 0;
			}
			Q_strcat(buffer, sizeof(buffer), va("\nDamage Taken: %i | From Team: %i (%1.1f percent)\n", ent->client->pers.statistics.Weapon[j].TotalDamageTaken, ent->client->pers.statistics.Weapon[j].TotalDamageTakenTeam, percent));

			//If !=spas
			if (j != 11)
			{
				Q_strcat(buffer, sizeof(buffer), va("Head: %i | Torso: %i | Legs: %i | Arms: %i \n",
								    ent->client->pers.statistics.Weapon[j].OthersHits[0],
								    ent->client->pers.statistics.Weapon[j].OthersHits[1],
								    ent->client->pers.statistics.Weapon[j].OthersHits[2],
								    ent->client->pers.statistics.Weapon[j].OthersHits[3]
								    ));
			}

			//Send it off
			StatsSendBuffer(buffer, sent->client->ps.clientNum);
		}
	}
}

/**
 * $(function)
 */
void StatsSendBuffer(char *buffer, int clientnum)
{
	if (strlen(buffer) > 250)
	{
		buffer[501] = 0;
		trap_SendServerCommand(clientnum, va("stats1 \"%s\"", &buffer[250]));
		buffer[250] = 0;
		trap_SendServerCommand(clientnum, va("stats \"%s\"", buffer));
	}
	else
	{
		trap_SendServerCommand(clientnum, va("stats \"%s\"", buffer));
		trap_SendServerCommand(clientnum, va("stats1 \"\" "));
	}
}

//award_t [S_WEAPONS][16]=

int Stats_GetTotalKills(gentity_t *ent)
{
	int  j;
	int  kills = 0;

	for (j = 0; j < ST_MAX; j++)
	{
		//Add up the kills
		//Ignore ST_BOMBED, they're not real frags
		//Ignore ST_TK, its tracked in other stats
		if ((j != ST_BOMBED) && (j != ST_TK))
		{
			kills += ent->client->pers.statistics.Kills[j];
		}
	}

	for (j = 0; j < S_WEAPONS; j++)
	{
		kills += ent->client->pers.statistics.Weapon[j].Kills;
	}

	return kills;
}
/**
 * $(function)
 */
int Stats_GetTotalDeaths(gentity_t *ent)
{
	int  j;
	int  kills = 0;

	for (j = 0; j < ST_MAX; j++)
	{
		//ignore st_tk, those frags are counted elsewhere
		if (j != ST_TK)
		{
			kills += ent->client->pers.statistics.Deaths[j];
		}
	}

	for (j = 0; j < S_WEAPONS; j++)
	{
		kills += ent->client->pers.statistics.Weapon[j].Deaths;
	}

	return kills;
}
/**
 * $(function)
 */
int Stats_GetTotalDamageGiven(gentity_t *ent)
{
	int  dmg = 0;
	int  j;

	for (j = 0; j < S_WEAPONS; j++)
	{
		dmg += ent->client->pers.statistics.Weapon[j].TotalDamageDone;
	}
	return dmg;
}

/**
 * $(function)
 */
int Stats_GetAmmoFired(gentity_t *ent)
{
	int  dmg = 0;
	int  j;

	for (j = 0; j < S_WEAPONS; j++)
	{
		dmg += ent->client->pers.statistics.Weapon[j].BulletsFired;
	}
	return dmg;
}
//doesnt include knife, hk69, nades
int Stats_GetMostFiredGun(gentity_t *ent)
{
	int  j;
	int  fav   = -1;
	int  fired = 0;

	for (j = 0; j < S_WEAPONS; j++)
	{
		if (ent->client->pers.statistics.Weapon[j].BulletsFired > fired)
		{
			fired = ent->client->pers.statistics.Weapon[j].BulletsFired;
			fav   = j;
		}
	}
	return fav;
}

/**
 * $(function)
 */
int Stats_GetTotalLocations(gentity_t *ent, int loc, qboolean other)
{
	int  j;
	int  kills = 0;

	for (j = 0; j < S_WEAPONS; j++)
	{
		if (other == qfalse)
		{
			kills += ent->client->pers.statistics.Weapon[j].ZoneHits[loc];
		}
		else
		{
			kills += ent->client->pers.statistics.Weapon[j].OthersHits[loc];
		}
	}
	return kills;
}

/**
 * $(function)
 */
void Stats_AddScore(gentity_t *ent, tr_score_stats type)
{
	ent->client->pers.statistics.Scores[type]++;
}

/**
 * $(function)
 */
int Stats_CalcRatio(int kills, int deaths)
{
	//25 44
	//1 3
	//5 1
	int  high;
	int  j;

	if ((kills == 0) || (deaths == 0))
	{
		return 1;
	}

	if (kills > deaths)
	{
		high = kills;
	}
	else
	{
		high = deaths;
	}

	for (j = high; j > 0; j--)
	{
		if ((kills % j == 0) && (deaths % j == 0))
		{
			return j;
		}
	}
	return 1;
}

/**
 * $(function)
 */
void Stats_ClearAll(void)
{
	int	   j;
	gentity_t  *ent;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			memset(&ent->client->pers.statistics, 0, sizeof(ent->client->pers.statistics));
		}
	}

	for (j = 0; j < ST_L_MAX; j++)
	{
		level.leaderboard[j] = -1 ;
	}
}

//What a monster!
//Flips values of level.leaderboard[] to a clientnum if the are the current award winner
void Stats_Leaderboard(void)
{
	tStatsRecord  st;
	int	      check[20];
	int	      count;
	int	      j;
	float	      fcount;
	gentity_t     *ent;

	/*ST_L_SNIPERACC,
	ST_L_AUTOACC,
	ST_L_PISTOLACC,
	ST_L_GRENADE,
	ST_L_SR8WHORE,
	ST_L_TEAMDMG,
	ST_L_TEAMDEATH,
	ST_L_WORSTRATIO,
	ST_L_MARIO,
	ST_L_DAMAGE,
	ST_L_AMMO,
	SL_L_HEADSHOT
	SL_L_SHOTHEAD,
	ST_L_ALLAH*/

	//Init
	level.leaderboard[ST_L_INITIALIZED] = -1;

	for (j = 0; j < ST_L_MAX; j++)
	{
		level.leaderboard[j] = -1 ;
	}

	//Sniper Accuracy.  5 Kills, Highest acc %
	Stats_NegStat(&st);
	st.Accuracy			  = 0;
	st.Kills			  = 5;
	check[0]			  = UT_WP_SR8;
	check[1]			  = UT_WP_PSG1;
	count				  = 2;
	level.leaderboard[ST_L_SNIPERACC] = Stats_FindBestWeaponStat(&st, check, count);

	//Auto Accuracy.  5 Kills, Highest Acc %
	Stats_NegStat(&st);
	st.Accuracy			= 0;
	st.Kills			= 5;
	check[0]			= UT_WP_MP5K;
	check[1]			= UT_WP_UMP45;
	check[2]			= UT_WP_LR;
	check[3]			= UT_WP_G36;
	check[4]			= UT_WP_AK103;
	check[5]			= UT_WP_NEGEV;
	check[6]			= UT_WP_M4;
	check[7]            = UT_WP_MAC11;
	//check[7]			= UT_WP_P90;
	count				= 8;
	level.leaderboard[ST_L_AUTOACC] = Stats_FindBestWeaponStat(&st, check, count);

	//Pistol Accuracy.  5 Kills, Highest acc %
	Stats_NegStat(&st);
	st.Accuracy			  = 0;
	st.Kills			  = 5;
	check[0]			  = UT_WP_BERETTA;
	check[1]			  = UT_WP_DEAGLE;
	check[2]			  = UT_WP_GLOCK;
	check[3]              = UT_WP_COLT1911;
	//check[4]			  = UT_WP_MAGNUM;
	count				  = 4;
	level.leaderboard[ST_L_PISTOLACC] = Stats_FindBestWeaponStat(&st, check, count);

	//Sr8 whoring.	Highest Kills, low deaths
	Stats_NegStat(&st);
	st.Kills			 = 10;
	check[0]			 = UT_WP_SR8;
	count				 = 1;
	level.leaderboard[ST_L_SR8WHORE] = Stats_FindBestWeaponStat(&st, check, count);

	if (level.leaderboard[ST_L_SR8WHORE] > -1)
	{
		if (Stats_GetTotalDeaths(&g_entities[level.leaderboard[ST_L_SR8WHORE]]) > 5)
		{
			level.leaderboard[ST_L_SR8WHORE] = -1;
		}
	}

	//ST_L_GRENADE.  Highest Kills, over 5
	count = 5;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (ent->client->pers.statistics.Kills[ST_GREN] > count)
			{
				count				= ent->client->pers.statistics.Kills[ST_GREN];
				level.leaderboard[ST_L_GRENADE] = j;
			}
		}
	}

	////ST_L_TEAMDMG  -- you shot your own teammates over 10 times
	Stats_NegStat(&st);
	st.BulletsHitTeam		= 10;

	check[0]			= UT_WP_MP5K;
	check[1]			= UT_WP_UMP45;
	check[2]			= UT_WP_LR;
	check[3]			= UT_WP_G36;
	check[4]			= UT_WP_AK103;
	check[5]			= UT_WP_NEGEV;
	check[6]			= UT_WP_M4;
	check[7]			= UT_WP_SR8;
	check[8]			= UT_WP_PSG1;
	check[9]			= UT_WP_BERETTA;
	check[10]			= UT_WP_DEAGLE;
	check[11]			= UT_WP_GLOCK;
	check[12]           = UT_WP_COLT1911;
	check[13]           = UT_WP_MAC11;
	/*check[11]			= UT_WP_P90;
	check[13]			= UT_WP_MAGNUM;
	check[14]			= UT_WP_DUAL_BERETTA;
	check[15]			= UT_WP_DUAL_DEAGLE;
	check[16]			= UT_WP_DUAL_GLOCK;
	check[17]			= UT_WP_DUAL_MAGNUM;*/

	count				= 14;
	level.leaderboard[ST_L_TEAMDMG] = Stats_FindBestWeaponStat(&st, check, count);

	////ST_L_TEAMDEATH  -- Own Team killed you over 5
	count = 5;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (ent->client->pers.statistics.Deaths[ST_TK] > count)
			{
				count				  = ent->client->pers.statistics.Deaths[ST_TK];
				level.leaderboard[ST_L_TEAMDEATH] = j;
			}
		}
	}

	// ST_L_WORSTRATIO -- Lowest KdivD, but deaths over 5, kdiv under 0.75
	fcount = 0.75f;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if ((Stats_GetTotalDeaths(ent) > 5) && (ent->client->pers.statistics.KdivD < fcount))
			{
				fcount				   = ent->client->pers.statistics.KdivD;
				level.leaderboard[ST_L_WORSTRATIO] = j;
			}
		}
	}

	// ST_L_BESTRATIO -- Highest KdivD, but kills over 5, kdiv over 2
	fcount = 2;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if ((Stats_GetTotalKills(ent) > 5) && (ent->client->pers.statistics.KdivD > fcount))
			{
				fcount				  = ent->client->pers.statistics.KdivD;
				level.leaderboard[ST_L_BESTRATIO] = j;
			}
		}
	}

	//ST_L_MARIO
	count = 0;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (ent->client->pers.statistics.Kills[ST_GOOMBA] > count)
			{
				count			      = ent->client->pers.statistics.Kills[ST_GOOMBA];
				level.leaderboard[ST_L_MARIO] = j;
			}
		}
	}

	//ST_L_DAMAGE
	count = 2000;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (Stats_GetTotalDamageGiven(ent) > count)
			{
				count			       = Stats_GetTotalDamageGiven(ent);
				level.leaderboard[ST_L_DAMAGE] = j;
			}
		}
	}

	// ST_L_AMMO
	count = 1000;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (Stats_GetAmmoFired(ent) > count)
			{
				count			     = Stats_GetAmmoFired(ent);
				level.leaderboard[ST_L_AMMO] = j;
			}
		}
	}

	//SL_L_HEADSHOT -- Shot over 15 headshots
	Stats_NegStat(&st);
	st.ZoneHits[0]			 = 15;

	check[0]			 = UT_WP_MP5K;
	check[1]			 = UT_WP_UMP45;
	check[2]			 = UT_WP_LR;
	check[3]			 = UT_WP_G36;
	check[4]			 = UT_WP_AK103;
	check[5]			 = UT_WP_NEGEV;
	check[6]			 = UT_WP_M4;
	check[7]			 = UT_WP_SR8;
	check[8]			 = UT_WP_PSG1;
	check[9]			 = UT_WP_BERETTA;
	check[10]			 = UT_WP_DEAGLE;
	check[11]			 = UT_WP_GLOCK;
	check[12]            = UT_WP_COLT1911;
	check[13]            = UT_WP_MAC11;
	/*check[11]			 = UT_WP_P90;
	check[13]			 = UT_WP_MAGNUM;
	check[14]			 = UT_WP_DUAL_BERETTA;
	check[15]			 = UT_WP_DUAL_DEAGLE;
	check[16]			 = UT_WP_DUAL_GLOCK;
	check[17]			 = UT_WP_DUAL_MAGNUM;*/

	count				 = 14;
	level.leaderboard[ST_L_HEADSHOT] = Stats_FindBestWeaponStat(&st, check, count);

	//SL_L_SHOTHEAD -- Shot over 15 in the head
	Stats_NegStat(&st);
	st.OthersHits[0]		 = 15;

	check[0]			 = UT_WP_MP5K;
	check[1]			 = UT_WP_UMP45;
	check[2]			 = UT_WP_LR;
	check[3]			 = UT_WP_G36;
	check[4]			 = UT_WP_AK103;
	check[5]			 = UT_WP_NEGEV;
	check[6]			 = UT_WP_M4;
	check[7]			 = UT_WP_SR8;
	check[8]			 = UT_WP_PSG1;
	check[9]			 = UT_WP_BERETTA;
	check[10]			 = UT_WP_DEAGLE;
	check[11]			 = UT_WP_GLOCK;
	check[12]            = UT_WP_COLT1911;
	check[13]             = UT_WP_MAC11;
	/*check[11]			 = UT_WP_P90;
	check[13]			 = UT_WP_MAGNUM;
	check[14]			 = UT_WP_DUAL_BERETTA;
	check[15]			 = UT_WP_DUAL_DEAGLE;
	check[16]			 = UT_WP_DUAL_GLOCK;
	check[17]			 = UT_WP_DUAL_MAGNUM;*/

	count				 = 14;
	level.leaderboard[ST_L_SHOTHEAD] = Stats_FindBestWeaponStat(&st, check, count);

	//ST_L_ALLAH.  Highest Self Inflicted deaths, over 5
	count = 5;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse)
		{
			if (ent->client->pers.statistics.Deaths[ST_GRENSELF] > count)
			{
				count			      = ent->client->pers.statistics.Deaths[ST_GRENSELF];
				level.leaderboard[ST_L_ALLAH] = j;
			}
		}
	}
}

//Util function for  Stats_FindBestWeaponStat
void Stats_NegStat(tStatsRecord *st)
{
	st->Accuracy	   = -1;
	st->BulletsFired   = -1;
	st->BulletsHit	   = -1;
	st->BulletsHitTeam = -1;
	st->BulletsMissed  = -1;
	st->Deaths	   = -1;
	st->Kills	   = -1;
	st->ZoneHits[0]    = -1;
	st->OthersHits[0]  = -1;
}

//Fill st with -1's for stats you dont care about
//or a minimum value for those you do
int Stats_FindBestWeaponStat(tStatsRecord *st, int *ut_wepid, int count)
{
	int	   j;
	gentity_t  *ent;
	int	   best = -1;
	int	   r;
	int	   findme;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		ent = &g_entities[j];

		if (ent->inuse) //Only doing a subset for now, yuck!
		{
			for (r = 0; r < count; r++)
			{
				findme = StatsEngineWepID(ut_wepid[r]);

				if ((ent->client->pers.statistics.Weapon[findme].Accuracy > st->Accuracy) &&
				    (ent->client->pers.statistics.Weapon[findme].BulletsFired > st->BulletsFired) &&
				    (ent->client->pers.statistics.Weapon[findme].BulletsHit > st->BulletsHit) &&
				    (ent->client->pers.statistics.Weapon[findme].BulletsHitTeam > st->BulletsHitTeam) &&
				    (ent->client->pers.statistics.Weapon[findme].BulletsMissed > st->BulletsMissed) &&
				    (ent->client->pers.statistics.Weapon[findme].Deaths > st->Deaths) &&
				    (ent->client->pers.statistics.Weapon[findme].Kills > st->Kills) &&
				    (ent->client->pers.statistics.Weapon[findme].ZoneHits[0] > st->ZoneHits[0]) &&
				    (ent->client->pers.statistics.Weapon[findme].OthersHits[0] > st->OthersHits[0])
				    )
				{
					if (st->Accuracy != -1)
					{
						st->Accuracy = ent->client->pers.statistics.Weapon[findme].Accuracy;
						best	     = j;
					}

					if (st->BulletsFired != -1)
					{
						st->BulletsFired = ent->client->pers.statistics.Weapon[findme].BulletsFired;
						best		 = j;
					}

					if (st->BulletsHit != -1)
					{
						st->BulletsHit = ent->client->pers.statistics.Weapon[findme].BulletsHit;
						best	       = j;
					}

					if (st->BulletsHitTeam != -1)
					{
						st->BulletsHitTeam = ent->client->pers.statistics.Weapon[findme].BulletsHitTeam;
						best		   = j;
					}

					if (st->BulletsMissed != -1)
					{
						st->BulletsMissed = ent->client->pers.statistics.Weapon[findme].BulletsMissed;
						best		  = j;
					}

					if (st->Deaths != -1)
					{
						st->Deaths = ent->client->pers.statistics.Weapon[findme].Deaths;
						best	   = j;
					}

					if (st->Kills != -1)
					{
						st->Kills = ent->client->pers.statistics.Weapon[findme].Kills;
						best	  = j;
					}

					if (st->ZoneHits[0] != -1)
					{
						st->ZoneHits[0] = ent->client->pers.statistics.Weapon[findme].ZoneHits[0];
						best		= j;
					}

					if (st->OthersHits[0] != -1)
					{
						st->OthersHits[0] = ent->client->pers.statistics.Weapon[findme].OthersHits[0];
						best		  = j;
					}
				}
			}
		}
	}
	return best;
}
