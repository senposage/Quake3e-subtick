/**
 * Filename: $(filename)
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */
#ifndef GLOCAL
#define GLOCAL
// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_local.h -- local definitions for game module

// @r00t: How often send reports (ms)
#define HAX_REPORT_INTERVAL 60000
// server
//#define HAX_SERVER_NAME "127.0.0.1"
//#define HAX_SERVER_PORT 1122

#define HAX_SERVER_NAME "authserver2.urbanterror.info"
//#define HAX_SERVER_NAME "fs3.urbanterror.info"
#define HAX_SERVER_PORT 27954



#include "q_shared.h"
#include "bg_public.h"
#include "g_public.h"
#include "../common/c_aries.h"

#ifdef USE_AUTH

//@Barbatos //@Kalish

extern int		auth_mode;							// auth state
extern char 	auth_header[512];					// command header
extern char 	auth_heartbeat[MAX_INFO_STRING];	// status for heartbeat
extern char 	auth_players[MAX_INFO_STRING];		// players for heartbeat

extern vmCvar_t auth;					// Auth system public value (integer)
extern vmCvar_t auth_status;			// Auth system public status (string)
extern vmCvar_t auth_enable;			// 0 Auth system disabled - 1 enabled
extern vmCvar_t auth_verbosity; 		// 0 no message - 1 messages on top - 2 messages on bottom
extern vmCvar_t auth_cheaters;			// 0 accept cheaters - 1 refuse banned IPs and logins
extern vmCvar_t auth_tags;				// 0 no tag checking - 1 refuse stolen clan tags
extern vmCvar_t auth_notoriety; 		// 0 accept everybody - 1 require valid auth for every players - 10,20,30 etc. limit to players with this notoriety
extern vmCvar_t auth_groups;			// "" accept everybody - "group1 group2" require to be member of one of these groups
extern vmCvar_t auth_owners;			// 0 password only rcon - "group1 group2" require to be admin/referree/member/friend of one of these groups

/*
extern vmCvar_t auth_nicknames; 		// 0 accept all nicknames - 1 refuse stolen nicknames
extern vmCvar_t auth_cmds_anonymous;	// avalaible commands for anonymous rcon
extern vmCvar_t auth_cmds_authed;		// avalaible commands for authed user rcon
extern vmCvar_t auth_cmds_friend;		// avalaible commands for authed friends rcon
extern vmCvar_t auth_cmds_member;		// avalaible commands for authed member rcon
extern vmCvar_t auth_cmds_referee;		// avalaible commands for authed referee rcon
*/

#endif

//==================================================================

// the "gameversion" client command will print this plus compile date
#define MAX_GROUPS_PER_TEAM    16
#define MAX_SERVER_FPS		   128

#define BODY_QUEUE_SIZE 	   MAX_CLIENTS

#define INFINITE			   1000000

#define FRAMETIME			   100						// msec
#define EVENT_VALID_MSEC	   300
#define CARNAGE_REWARD_TIME    3000
#define REWARD_SPRITE_TIME	   2000

#define MAX_BREAKABLE_HITS	   10			// how many breakable objects can a bullet go thru?

#define MAX_BOT_ITEM_GOALS	   64		// amount of item spots that the bot will randomly go to

#define INTERMISSION_DELAY_TIME    1000
#define SP_INTERMISSION_DELAY_TIME 5000

// gentity->flags
#define FL_GODMODE			0x00000010
#define FL_NOTARGET 		0x00000020
#define FL_TEAMSLAVE		0x00000400		   // not the first on the team
#define FL_NO_KNOCKBACK 	0x00000800
#define FL_DROPPED_ITEM 	0x00001000
#define FL_NO_BOTS			0x00002000		   // spawn point not for bot use
#define FL_NO_HUMANS		0x00004000		   // spawn point just for bots
#define FL_FORCE_GESTURE	0x00008000		   // force gesture on client
#define FL_KILLNEXTROUND	0x00010000		   // will be destroyed on round

// Drowning

#define DROWNTIME 15000 			  // was 12000

// movers are things like doors, plats, buttons, etc
typedef enum
{
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1,
	MOVER_LOCKED
} moverState_t;

#define SP_PODIUM_MODEL "models/mapobjects/podium/podium4.md3"

//============================================================================

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;
typedef struct gmover_s  gmover_t;
typedef struct gspawn_s  gspawn_t;

struct gmover_s
{
	moverState_t  moverState;
	int 		  soundPos1;
	int 		  sound1to2;
	int 		  sound2to1;
	int 		  soundPos2;
	int 		  soundLoop;
	gentity_t	  *nextTrain;
	gentity_t	  *currTrain;
	gentity_t	  *destination; // GottaBeKD an interfaceEnt's destination
	vec3_t		  pos1, pos2;

	qboolean	  trigger_only; // true if mover can only be activated with a trigger 9ie: not by player)

	vec3_t		  apos1;				// for rdoors
	vec3_t		  apos2;				// for rdoors
	int 		  direction;			// for forcing opening direction of rdoors
	int 		  trainID;				// uniquely identifies a UT train to a set of control buttons
	float		  goTime;				// train starts moving at this time
	vec3_t		  rotation; 			// each stop gets rotation values for the train.
	int 		  rotationTime; 		// how to continue the rotation
	int 		  team; 			// for doors.. teams!

	int 		  stuckTimer;		// level.time when mover last got stuck
	qboolean	  moving;			// whether or not it is travelling. TR_STATIONARY is sometimes true between stops, this one says if it has reached a final destination

	void		  (*reached)(gentity_t *self, gentity_t *other);	// movers call this when hitting endpoint
	void		  (*blocked)(gentity_t *self, gentity_t *other);

	int 		  startSound;
	int 		  stopSound;

	int 		  display;		  // whether or not the keyboard interface gets a gui popup

	char*		  option1;
	char*		  option2;
	char*		  option3;
	char*		  option4;
	char*		  option5;

	char*		  stop1;
	char*		  stop2;
	char*		  stop3;
	char*		  stop4;
	char*		  stop5;

	char*		  stop1from2;
	char*		  stop1from3;
	char*		  stop1from4;
	char*		  stop1from5;

	char*		  stop2from1;
	char*		  stop2from3;
	char*		  stop2from4;
	char*		  stop2from5;

	char*		  stop3from1;
	char*		  stop3from2;
	char*		  stop3from4;
	char*		  stop3from5;

	char*		  stop4from1;
	char*		  stop4from2;
	char*		  stop4from3;
	char*		  stop4from5;

	char*		  stop5from1;
	char*		  stop5from2;
	char*		  stop5from3;
	char*		  stop5from4;


};

struct gspawn_s
{
	int   gametypes;
	int   team;
	char  *group;
};

// Stores all the information necessary to bring people back in time to
// check for lag shots.
//	Dokta8: commented out apox's code to replace with Neil's Antilag code
/*typedef struct ghistory_s
{
	vec3_t	rOrigin;				// entity.r.currentOrigin
	vec3_t	rAngles;				// entity.r.currentAngles
	vec3_t	sOrigin;				// entity.s.pos.trBase
	vec3_t	sAngles;				// entity.s.apos.trBase
	byte	torsoanim;				// entity.s.torsoAnim
	byte	leganim;				// entity.s.legsAnim
	vec3_t	absmin; 				// entity.r.absmin
	vec3_t	absmax; 				// entity.r.absmax

	int 	time;					// time history item was saved

	struct ghistory_s*		next;	// next history item in list
	struct ghistory_s*		prev;	// prev history item in list

} ghistory_t;*/

struct gentity_s
{
	entityState_t	s;				// communicated by server to clients
	entityShared_t	r;				// shared by both the server system and game

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	gentity_t  *parent; 			// NULL if no parent
	gclient_t  *client; 			// NULL if not a client
	gmover_t   *mover;				// NULL if not a mover
	gspawn_t   *spawn;				// NULL if not a spawn point

	qboolean   inuse;

	char	   *classname;			// set in QuakeEd
	int 	   classhash;
	int 	   spawnflags;			// set in QuakeEd

	qboolean   neverFree;			// if true, FreeEntity will only unlink
	// bodyque uses this

	int 	   flags;				// FL_* variables

	char	   *model;
	char	   *model2;
	int 	   freetime;			// level.time when the object was freed

	int 	   eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean   freeAfterEvent;
	qboolean   unlinkAfterEvent;

	qboolean   physicsObject;		// if true, it can be pushed by movers and fall off edges
	// all game items are physicsObjects,
	float	   physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int 	   clipmask;			// brushes with this content value will be collided against
	// when moving.  items and corpses do not collide against
	// players, for instance

	// movers

	char	   *message;

	int 	   timestamp;			// body queue sinking, etc

	float	   angle;				// set in editor, -1 = up, -2 = down
	char	   *target;
	char	   *targetname;
	char	   *team;
	char	   *targetShaderName;
	char	   *targetShaderNewName;
	gentity_t  *target_ent;

	float	   speed;
	vec3_t	   movedir;

	int 	   nextthink;
	void	   (*think)(gentity_t *self);
	void	   (*touch)(gentity_t *self, gentity_t *other, trace_t *trace);
	void	   (*use)(gentity_t *self, gentity_t *other, gentity_t *activator);
	void	   (*pain)(gentity_t *self, gentity_t *attacker, int damage);
	void	   (*die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

	int 	   pain_debounce_time;
	int 	   pain_bleed_time;
	int 	   fly_sound_debounce_time; 		// wind tunnel
	int 	   last_move_time;

	int 	   health;

	qboolean   takedamage;

	int 	   damage;
	int 	   splashDamage;		// quad will increase this without increasing radius
	int 	   splashRadius;
	int 	   methodOfDeath;
	int 	   splashMethodOfDeath;

	int 	   count;

	gentity_t  *chain;
	gentity_t  *enemy;
	gentity_t  *activator;
	gentity_t  *teamchain;			// next entity in team
	gentity_t  *teammaster; 		// master of the team

	int 	   watertype;
	int 	   waterlevel;

	int 	   noise_index;

	// timing variables
	float	   wait;
	float	   random;

	gitem_t   *item;				// for bonus items

	// Urban Terror additional entity members

	int  breaktype; 				// type of breakable object (eg: glass == 0)
	int  breakshards;				// number of shards to break into
	int  breakaxis; 				// defines the "thin" axis of the glass
	int  breakthickness;			// thickness of "thin" axis, for bullet holes
	int  bombable;					// is entity breaks on bomb.

	// GottaBeKD: NPC stuff
	char	  *npctype; 			// the type of npc. Fish, Cat, Prostitute, Mailman, etc..
	char	  *radioText;			// the radio text that an NPC will say
	qboolean  isNPC;				// the universal isNPC condition
	int 	  currentWaypoint;		// spawns at 1
	int 	  ignoreWaypoint;		// if 1, waypoint is only used for spawning. no patrol.
	int 	  botAwareness;
	qboolean  ignoreSpawn;			// if true, then ClientSpawn
	qboolean  isLeading;			// Is the NPC leading another player
	qboolean  isWaitingUntilVis;	// Is the NPC waiting until a player becomes visible
	qboolean  switchLTG;			// NPC should switch ltg type
	qboolean  fetchingPlayer;		// NPC is going back for trailing player

	//Used for any type of trigger/switch ent->use (NPC's isLeading).
	//It is also used to determine if a switch is Off, On, or Toggle
	int 		   switchType;

	int 		   lastDoorTime;

	int 		   teamBombSite;
	gentity_t	   *bleedEnemy;
	HitLocation_t  bleedHitLocation;

	char		   *interfaceEnt;	  // The interface associated with this ent

	gentity_t	   *triggerLocation;
	gentity_t	   *triggerGravity;

	// For tracking bombed entities.
	int 		   bombedTime;

	// For tracking last owners
	gentity_t	   *lastOwner;
	int 		   lastOwnerThink;

	/* @Barbatos: jump mode stuff */
	int 		   jumpWayNum;		 // the number of the way of a jump map ( a jump map can have several ways )
	int 		   jumpWayColor;	 //@r00t
};

typedef enum
{
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

typedef enum
{
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum
{
	TEAM_BEGIN, 	// Beginning a team game, spawn at base
	TEAM_ACTIVE 	// Now actively playing
} playerTeamStateState_t;

typedef struct
{
	playerTeamStateState_t	state;

	int 		location;

	int 		captures;
	int 		basedefense;
	int 		carrierdefense;
	int 		flagrecovery;
	int 		fragcarrier;
	int 		assists;

	float		lasthurtcarrier;
	float		lastreturnedflag;
	float		flagsince;
	float		lastfraggedcarrier;
} playerTeamState_t;

// the auto following clients don't follow a specific client
// number, but instead follow the first two active players
#define FOLLOW_ACTIVE1 -1
#define FOLLOW_ACTIVE2 -2

//
#define MAX_NETNAME    36
#define MAX_CONFIG	   100
#define MAX_VOTE_COUNT 3

///statsengine
#define S_MAXLOCS 4
#define S_WEAPONS 20

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct
{
	team_t			  sessionTeam;
	team_t			  pendingTeam;
	int 			  spectatorTime;			// for determining next-in-line to play
	spectatorState_t  spectatorState;
	int 			  spectatorClient;			// for chasecam and follow mode
	int 			  wins, losses; 			// tournament stats
	qboolean		  teamLeader;				// true when this client is a team leader
	int 			  muteTimer;				// if > level.time, will prevent client from chatting
	qboolean		  bombHolder;				// if client is the bomb holder
	int 			  queSwitch;
	int 			  inactivityTime;			// kick players when time > this
	int 			  lastFailedVote;
	int 			  connectTime;

	#ifdef USE_AUTH
	/*
	// @Kalish USELESS ATM
	int 			  auth_count;
	int 			  auth_time;
	int 			  auth_state;
	*/
	int 			  renames;								// rename counter
	char			  lastname[MAX_NETNAME];				// last name
	int 			  welcome;								// welcome done
	char			  dropreason[128];						// drop short reason
	char			  dropmessage[256]; 					// drop client message
	int 			  droptime; 							// drop time
	char			  auth_name[32];						// client name with the upper cases and the clan tag
	char			  auth_login[32];						// client account login
	char			  auth_notoriety[32];					// client account notoriety
	int 			  auth_level;							// client level on server
	#endif

	// Jump mode variables
	int 			  jumpRun;								// toggle the race triggers
	int				  jumpRunCount;							// count how many time the players completed a jump run
	int 			  jumpWay;								// the way of the current jump run
	qboolean          ghost;                                // toggle client side ghosting - jump mode
	qboolean 		  regainstamina;						// activate or not the stamina regeneration - jump mode
	qboolean          allowgoto;                            // toggle for personal goto - jump mode
	vec3_t			  savedLocation;						// last saved location coords
	vec3_t			  savedLocationAngles;					// last saved location angle views coords

	char              ip[20];                               // ip
	unsigned char     guid_bin[16];                         // parsed GUID from HEX as binary string

// r00t: Hax reporting
	int               hax_report[6];                        // status as received from client
	int               hax_report_time;                      // when it was received

} clientSession_t;

typedef struct
{
	team_t	  team;
	int 	  time;
	int 	  ping;
	int 	  wins;
	int 	  losses;
	qboolean  playing;
} survivorData_t;


typedef struct
{
	int    BulletsFired;			// Grand total
	int    BulletsMissed;			// BulletsMissed + BulletsHit = BulletsFired
	int    BulletsHit;
	int    BulletsHitTeam;			// Dork.

	int    TotalDamageDone;
	int    TotalDamageDoneTeam; 	// Double Dork

	int    ZoneHits[S_MAXLOCS]; 	// Locations we've hit with this.
	int    OthersHits[S_MAXLOCS];	// Locations we've been hit with this.
	float  Accuracy;				// Percentage ratio of hits to misses
	int    Kills;					// Kills we've made

	int    Deaths;					// Deaths we've taken
	int    TotalDamageTaken;
	int    TotalDamageTakenTeam;
} tStatsRecord;

typedef enum
{
	ST_GREN,
	ST_GRENSELF,
	ST_HKEXPLODE,
	ST_HKSELF,
	ST_HKHIT,
	ST_KNIFESTAB,
	ST_KNIFETHROWN,
	ST_KICKED,
	ST_BLEED,
	ST_GOOMBA,
	ST_LEMMING,
	ST_SUICIDE,
	ST_FLAG,
	ST_BOMBED,
	ST_SPLODED,
	ST_ADMIN,
	ST_ENVIRONMENT,
	ST_TK,
	ST_MAX,
} tr_stats;

typedef enum
{
	ST_KILL_FC,
	ST_STOP_CAP,
	ST_PROTECT_FLAG,
	ST_RETURN_FLAG,

	ST_CAPTURE_FLAG,

	ST_BOMB_DEFUSE,
	ST_BOMB_BOOM,
	ST_BOMB_PLANT,
	ST_KILL_BC,

	ST_PROTECT_BC,
	ST_KILL_DEFUSE,
	ST_SCORE_MAX,
} tr_score_stats;

typedef enum
{
	ST_L_INITIALIZED,
	ST_L_SNIPERACC,
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
	ST_L_HEADSHOT,
	ST_L_SHOTHEAD,
	ST_L_ALLAH,
	ST_L_BESTRATIO,

	ST_L_MAX
} tr_leaderboard_stats;

typedef struct
{
	tStatsRecord  Weapon[S_WEAPONS];

	int 	  Kills[ST_MAX];
	int 	  Deaths[ST_MAX];

	int 	  Scores[ST_SCORE_MAX];

	int 	  KillRatio;
	int 	  DeathRatio;
	float		  KdivD;
} tPlayerStats;

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct {

	clientConnected_t  connected;
	usercmd_t		   cmd; 			 // we would lose angles if not persistant
	qboolean		   initialSpawn;
	qboolean		   localClient; 			// true if "ip" info key is "localhost"
//	qboolean		   pmoveFixed;
	char			   netname[MAX_NETNAME];
	char			   newnetname[MAX_NETNAME];
	int 			   enterTime;				// level.time the client entered the game
	playerTeamState_t  teamState;				// status in teamplay games
	int 			   voteCount;				// to prevent people from constantly calling votes
	int 			   teamVoteCount;			// to prevent people from constantly calling votes
	qboolean		   teamInfo;				// send team overlay updates?
	utGear_t		   gear;
	char			   weaponModes[UT_WP_NUM_WEAPONS];

	int 			   ariesModel;
	int 			   race;
	qboolean		   substitute;
	qboolean		   captain;
	int 			   physics;
	int 			   ut_timenudge;			//for fixing antiwarp lookups
	int 			   autoPickup;
	char			   funstuffred[13][3];
	char			   funstuffblue[13][3];
	char			   funstuff0[13];
	char			   funstuff1[13];
	char			   funstuff2[13];

	int 			   armbandrgb[3];
	tPlayerStats	   statistics;
	qboolean		   wasAutoJoined;			// to prevent from reassigning a team with g_teamautojoin, if client chose to spectate

	int		           ignoredClients[MAX_CLIENTS];
} clientPersistant_t;

//NT - client origin trails
// Modded by DensitY for Urban Terror

#define NUM_CLIENT_TRAILS 20

typedef struct
{
	vec3_t	saved_rOrigin;
	vec3_t	currentOrigin;
	vec3_t	currentAngles;			// D8: added so we can rotate the player's hitbox

	// D8: some other stuff needed for ARIES
	byte  torsoanim;				// entity.s.torsoAnim
	byte  legsanim; 				// entity.s.legsAnim

	// D8: and some more for the extra ARIES angles
	vec3_t	sPosTrDelta;			// traceEnt->s.pos.trDelta
	float	legYawAngle;			// traceEnt->client->legsYawAngle
	float	legPitchAngle;			// traceEnt->client->legsPitchAngle
	float	torsoYawAngle;			// traceEnt->client->torsoYawAngle
	float	torsoPitchAngle;		// traceEnt->client->torsoPitchAngle

	int time;

	int legsFrame;
	int torsoFrame;
} clientTrail_t;

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s
{
	// ps MUST be the first element, because the server expects it
	playerState_t  ps;				// communicated by server to clients

	// the rest of the structure is private to game
	clientPersistant_t	pers;
	clientSession_t 	sess;

	qboolean  readyToExit;	  // wishes to leave the intermission
	qboolean  noclip;

	int 	  lastCmdTime;		  // level.time of last usercmd_t, for EF_CONNECTION
	// we can't just use pers.lastCommand.time, because
	// of the g_sycronousclients case
	int 	  buttons;
	int 	  oldbuttons;
	int 	  latched_buttons;

	int 	  oldAngles[3];

	vec3_t	  oldOrigin;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int 	  damage_armor; 			// damage absorbed by armor
	int 	  damage_blood; 			// damage taken out of health
	int 	  damage_knockback; 		// impact damage
	vec3_t	  damage_from;				// origin for vector calculation
	qboolean  damage_fromWorld; 		// if true, don't use the damage_from vector

	int 	  accurateCount;			// for "impressive" reward sound

	int 	  accuracy_shots;			// total number of shots
	int 	  accuracy_hits;			// total number of hits

	//
	int 	  lastkilled_client;		// last client that this client killed
	int 	  lasthurt_client;			// last client that damaged this client
	int 	  lasthurt_mod; 			// type of damage the client did

	// timers
	int 	  respawnTime;				// can respawn when time > this, force after g_forcerespwan
	int 	  protectionTime;
	qboolean  inactivityWarning;		// qtrue if the five seoond warning has been given
	int 	  rewardTime;				// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	int 	  airOutTime;

	qboolean  fireHeld; 				// used for hook

	int 	  switchTeamTime;			// time the player switched teams

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int 	  timeResidual;

#ifdef MISSIONPACK
	gentity_t  *persistantPowerup;
	int 	   portalID;
	int 	   ammoTimes[UT_WP_NUM_WEAPONS];
	int 	   invulnerabilityTime;
#endif

	char	   *areabits;

// Urban Terror

	qboolean   laserSight;

	int 	   bleedWeapon;
	int 	   bleedCount;
	int 	   bleedCounter;
	float	   bleedRate;
	int 	   bleedResidual;
	int 	   lastHitTime;

	qboolean   ghost;
	qboolean   noGearChange;

	survivorData_t	survivor;

	int 	   teamUseTime;
	vec3_t	   botLastPos;
	int 	   botNotMovedCount;
	int 	   botShouldJumpCount;

	// D8: used for holding ARIES model rotation data
	// to sync up with the client
	float	   legsPitchAngle;
	float	   legsYawAngle;
	float	   torsoPitchAngle;
	float	   torsoYawAngle;

	gentity_t  *stuckTo;

	qboolean   voted;

	int 	   lastZoomTime;			// time of last change to player zoom level

	// Test Reenabled Apoxol's Antilag - DensitY
//	ghistory_t		*historyTail;
//	ghistory_t		*historyHead;
//	ghistory_t		*historyUndo;

	// NT - client origin trails
	//	int 		 trailHead;
//	  clientTrail_t    trail[NUM_CLIENT_TRAILS];
	qboolean	   isSaved;
	clientTrail_t  saved;  // used to restore after time shift
	clientTrail_t  FIFOTrail[NUM_CLIENT_TRAILS];

	// end NT
	int 		savedpersistant[MAX_PERSISTANT];
	int 		savedclientNum;
	int 		savedping;

	// REquired here for weapon firing!
	qboolean	IsDefusing;

	//mini map arrow
	gentity_t  *maparrow;

	int 		LastClientUpdate;
	int 		AttackTime;

	int 		nextSubCallTime;

	////////////////////
	anim_t		animationdata;

	int 		sr8PenaltyTime;
	unsigned	goalbits;
	int 		spectatorDirection;

	/* Freeze Tag — new in 4.3 */
	int 		freezeTime;			/* level.time when player was frozen; 0 = not frozen */
	int 		meltTime;			/* level.time when frozen player will melt (die) */
	int 		thawingBy;			/* client number of player currently thawing us; -1 = none */
	int 		thawStartTime;		/* level.time when thawing started */

	/* Gun Game — new in 4.3 */
	int 		gunGameLevel;		/* current weapon level in Gun Game (0-based) */

};

//
// this structure is cleared as each map is entered
//
#define MAX_SPAWN_VARS		 64
#define MAX_SPAWN_VARS_CHARS 2048

// Greatly improved
typedef struct
{
	int   numSpawnVars;
	char  *spawnVars[MAX_SPAWN_VARS][2];
} local_backup_spawn_t;

typedef struct
{
	struct gclient_s  *clients; 						// [maxclients]

	struct gentity_s  *gentities;
	int 			  gentitySize;
	int 			  num_entities; 					// current number, <= MAX_GENTITIES

	//int			  num_budoors;
	//int			  num_burdoors;

	int 			  warmupTime;						// restart match at this time
	qboolean		  warmupIntro;

	fileHandle_t	  logFile;

	// store latched cvars here that we want to get at often
	int 			  maxclients;

	int 			  framenum;
	int 			  time; 							// in msec
	int 			  previousTime; 					// so movers can back up when blocked
//	 int			  timechop; 						//chopped time delta

	int 			  frameStartTime;					//NT - actual time frame started (for anti-lag)

	int 			  startTime;						// level.time the map was started
	int 			  roundEndTime; 					// time round will end.

	int 			  teamScores[TEAM_NUM_TEAMS];
	int 			  lastTeamLocationTime; 			// last time of client team location update

	qboolean		  newSession;						// don't use any old session data, because
	// we changed gametype

	qboolean		  restarted;						// waiting for a map_restart to fire

	int 			  numConnectedClients;
	int 			  numNonSpectatorClients;			// includes connecting clients
	int 			  numPlayingClients;				// connected, non-spectators
	int 			  sortedClients[MAX_CLIENTS];				// sorted by score
	int 			  follow1, follow2; 				// clientNums for auto-follow spectators

	int 			  snd_fry;							// sound index for standing in lava

	int 			  warmupModificationCount;			// for detecting if g_warmup is changed

	// voting state
	char			  voteString[MAX_STRING_CHARS];
	char			  voteDisplayString[MAX_STRING_CHARS];
	int 			  voteTime; 						// level.time vote was called
	int 			  voteExecuteTime;					// time the vote is executed
	int 			  voteYes;
	int 			  voteNo;
	int 			  numVotingClients; 				// set by CalculateRanks
	int				  voteClientArg;					// contains target client num

	// team voting state
	char			  teamVoteString[2][MAX_STRING_CHARS];
	int 			  teamVoteTime[2];					// level.time vote was called
	int 			  teamVoteYes[2];
	int 			  teamVoteNo[2];
	int 			  numteamVotingClients[2];			// set by CalculateRanks

	// spawn variables
	qboolean		  spawning; 						// the G_Spawn*() functions are valid
	// reused now!
	int 			  numSpawnVars;
	char			  *spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int 			  numSpawnVarChars;
	char			  spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int 			  intermissionQueued;				// intermission was qualified, but
	// wait INTERMISSION_DELAY_TIME before
	// actually going there so the last
	// frag can be watched.  Disable future
	// kills during this delay
	int 			  intermissiontime; 				// time the intermission was started
	char			  *changemap;
	qboolean		  readyToExit;						// at least one client wants to exit
	int 			  exitTime;
	vec3_t			  intermission_origin;				// also used for spectator spawns
	vec3_t			  intermission_angle;

	qboolean		  locationLinked;					// target_locations get linked
	gentity_t		  *locationHead;					// head of the location list
	int 			  locationCount;

//	int 			  bodyQueIndex; 					// dead bodies
//	gentity_t		  *bodyQue[BODY_QUEUE_SIZE];

#ifdef MISSIONPACK
	int 			  portalSequence;
#endif

// Urban Terror

	// Survivor stuff
	int 			  survivorRoundRestartTime;
	int 			  survivorStartTime;
	qboolean		  survivorRestart;

// int				  survivorCaptureTeam;
	qboolean		  survivorNoJoin;
	int 			  survivorRedDeaths;
	int 			  survivorBlueDeaths;
	int 			  survivorWinTeam;

	// UT NPC's
	gentity_t		  *deferredNPCS[100];				  //GottaBeKD array of NPC ents that need to be manually spawned early
	int 			  numDeferredNPCS;					  //GottaBeKD number of current Deferred NPCS

	// UT Bot Long Term Item Goals
	gentity_t		  *LTGItems[MAX_BOT_ITEM_GOALS];	  //GottaBeKD array of spawns that bots use as long term item goals
	int 			  numLTGItems;

	// Gametype stuff
	qboolean		  flagSpawned;
	gentity_t		  *plantedBomb;
	int 			  nextScoreTime;
	int 			  redCapturePointsHeld;
	int 			  blueCapturePointsHeld;
	int 			  totalCapturePoints;

	int 			  voteClient;			   // client who called the present vote
	int 			  spawnsfrommode;		   // What mode our spawns came from

	int 			  g_roundcount;

	// Added DensitY Below
	qboolean		  BombMapAble;				// is the map able to play bomb modein the first place?
	qboolean		  RoundRespawn; 			// Added for Round based game Respawning
	int 			  NumBombSites; 			// Number of Bombsites on the map for Bomb mode
	qboolean		  BombGoneOff;				// has it blown up?
	qboolean		  BombDefused;				// bomb got defused?
	qboolean		  BombPlanted;				// has the bomb been planted?
	int 			  BombHolderLastRound;		// Whole had hte bomb last round
	qboolean		  UseLastRoundBomber;		// do we use the bomber from the last round?
	int 			  BombPlantTime;			// when the bomb was planted!
	vec3_t			  BombPlantOrigin;			// Where the planter was when he started planting.
	vec3_t			  BombExplosionOrigin;
	int 			  BombExplosionTime;
	int 			  BombExplosionCheckTime;
	int 			  Bomber;

	// Added Highsea
	// Game pause stuff.
	int 			  pauseState;
	int 			  pauseTime;
	int 			  numRedTimeouts;
	int 			  numBlueTimeouts;

	// Match stuff.
	int 			  matchState;

	// Bomb
	team_t			  AttackingTeam;
	int 			  RoundCount;			 // round ocunter

	// Start defusing time!
	int 			  StartDefuseTime;

	int 			  lastNukeTime;
	int 			  domoveoutcalltime;

	//
	int 			  swaproles;
	int 			  oldRedScore;
	int 			  oldBlueScore;

	int 			  hotpotatotime;
	int 			  hotpotato;
	int 			  pausedTime;						 // running total ms the map has been paused for.

	int 			  leaderboard[ST_L_MAX];
	int 			  goalindex;
} level_locals_t;

//
// g_spawn.c
//
qboolean G_SpawnString( const char *key, const char *defaultString, char * *out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean G_SpawnVector( const char *key, const char *defaultString, float *out );
void	 G_SpawnEntitiesFromString( void );
char *	 G_NewString( const char *string );
void	 G_FreeString( const char *string );
// New
void	 G_ParseSpawnFields( gentity_t *e, int SpawnNum );

//
// g_cmds.c
//
void Cmd_Score_f (gentity_t *ent);
void Cmd_ScoreSingle_f(gentity_t *src, gentity_t *dst);
void Cmd_ScoreDouble_f(gentity_t *src1, gentity_t *src2, gentity_t *dst);
void Cmd_ScoreGlobal_f(gentity_t *entity);
void StopFollowing( gentity_t *ent, qboolean force );
void BroadcastTeamChange( gclient_t *client, int oldTeam );
void SetTeam( gentity_t *ent, char *s, qboolean silent );
int  Cmd_FollowCycle_f( gentity_t *ent, int dir );
void G_ChatSubstitution( gentity_t *ent, char *text );
void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText );
void Cmd_Ref_f ( gentity_t *ent );
void Cmd_RefLogin_f ( gentity_t *ent );
void Cmd_RefMute_f(void);
void Cmd_RefForceTeam_f(void);
void Cmd_RefKick_f(void);
void Cmd_RefBan_f(void);
void Cmd_RefHelp_f(void);
void SuperSanitizeString( char *in, char *out );

//
// g_items.c
//
void	   G_CheckTeamItems( void );
void	   G_RunItem( gentity_t *ent );
void	   RespawnItem( gentity_t *ent );

void	   UseHoldableItem( gentity_t *ent );
void	   PrecacheItem (gitem_t *it);
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle, qboolean makeent);
gentity_t *Drop_Weapon( gentity_t *ent, int weaponid, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity, gentity_t *owner, int noPickupTime );
void	   SetRespawn (gentity_t *ent, float delay);
void	   G_SpawnItem (gentity_t *ent, gitem_t *item);
void	   FinishSpawningItem( gentity_t *ent );
void	   Think_Weapon (gentity_t *ent);
int    ArmorIndex (gentity_t *ent);
void	   Add_Ammo (gentity_t *ent, int weapon, int clips, int bullets );
void	   Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);
void	   Pickup_Item (gentity_t *ent, gentity_t *other, trace_t *trace, int autoPickup);

void	   ClearRegisteredItems( void );
void	   RegisterItem( gitem_t *item );
void	   SaveRegisteredItems( void );

//
// g_npc.c
//
gentity_t *BotFindWaypoint( gentity_t *bot );
gentity_t *BotFindNextWaypoint( gentity_t *bot );
gentity_t *BotFindNextNextWaypoint( gentity_t *bot, gentity_t *next_waypoint );
gentity_t *BotFindPrevWaypoint( gentity_t *bot );
gentity_t *BotFindFirstWaypoint( gentity_t *bot );
void	   G_NPCKill( gentity_t *bot );
//qboolean *NPCAI(	);
//qboolean NPCAI( bot_state_t *bs, int tfl, int retreat, bot_goal_t *goal );

//
// g_utils.c
//

int HashStr(const char *s);
#define HASHSTR(_s,_o) _o=HashStr(_s)

#define HSH_func_door						-630432598
#define HSH_func_keyboard_interface 		-1571311653
#define HSH_func_rotating_door				-1292454024
#define HSH_func_ut_train					-913528516
#define HSH_func_waypoint					1223927934
#define HSH_hi_portal_destination			-2110280419  // this is "hi_portal destination" (note the space!)
#define HSH_hi_portal_source				1349551961	 // this is "hi_portal source" (note the space!)
#define HSH_info_player_deathmatch			-103258081
#define HSH_info_player_intermission		-2104846030
#define HSH_info_ut_spawn					-1524182362
#define HSH_path_ut_stop					1281217137
#define HSH_team_BTB_bombspawn				-1839129477
#define HSH_team_CTF_blueflag				-665586244
#define HSH_team_CTF_bluespawn				-1195893957
#define HSH_team_CTF_neutralflag			303209758
#define HSH_team_CTF_redflag				-487254211
#define HSH_team_CTF_redspawn				-1364397043
#define HSH_team_blueobelisk				599497738
#define HSH_team_redobelisk 				-1448008508
#define HSH_ut_planted_bomb 				-82007226
#define HSH_func_breakable					-1514181943
#define HSH_func_button 					1234829790
#define HSH_fl								715481840
#define HSH_kamikaze_timer					-642575435	 // this is "kamikaze timer" (note the space!)
#define HSH_func_breakable					-1514181943
#define HSH_func_train						40908848
#define HSH_path_corner 					-1212481584
#define HSH_target_location 				8489281
#define HSH_ut_item_bomb					-581642577
#define HSH_player_bb						-135097346
#define HSH_podium							1180816821
#define HSH_bodyque 						751177490
#define HSH_player							-467462786
#define HSH_disconnected					1026525644
#define HSH_player_corpse					-389407216
#define HSH_door_trigger					-178671086
#define HSH_plat_trigger					1002654712
#define HSH_mrsentry						-631842709
#define HSH_jumpgoal						1863526401
#define HSH_worldspawn						932107823
#define HSH_grenade 						513611514
#define HSH_team_CTF_redplayer				2074630940
#define HSH_team_CTF_blueplayer 			-416302715
#define HSH_noclass 						-1068468807
#define HSH_freed							155463540
#define HSH_tempEntity						662184166
#define HSH_knife_thrown					1270307493
#define HSH_temp_bomb						1784782663



int 		G_ModelIndex( char *name );
int 		G_SoundIndex( char *name );
void		G_TeamCommand( team_t team, char *cmd );
void		G_KillBox (gentity_t *ent);
gentity_t * G_Find (gentity_t *from, size_t fieldofs, const char *match);
gentity_t * G_FindClass (gentity_t *from, const char *match);
gentity_t * G_FindClassHash (gentity_t *from, int matchhash);
gentity_t * G_PickTarget (char *targetname);
void		G_UseTargets (gentity_t *ent, gentity_t *activator);
void		G_SetMovedir ( vec3_t angles, vec3_t movedir);

void		G_InitGentity( gentity_t *e );
gentity_t * G_Spawn (void);
gentity_t * G_TempEntity( vec3_t origin, int event );
void		G_Sound( gentity_t *ent, int channel, int soundIndex );
void		G_FreeEntity( gentity_t *e );
qboolean	G_EntitiesFree( void );

void		G_TouchTriggers (gentity_t *ent);
void		G_TouchSolids (gentity_t *ent);

float * 	tv (float x, float y, float z);
char *		vtos( const vec3_t v );

float		vectoyaw( const vec3_t vec );

void		G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void		G_AddEvent( gentity_t *ent, int event, int eventParm );
void		G_SetOrigin( gentity_t *ent, vec3_t origin );
void		AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char *BuildShaderStateConfig(void);

//
// g_combat.c
//
qboolean CanDamage (gentity_t *targ, vec3_t origin);
void  G_Damage (gentity_t * targ, gentity_t * inflictor, gentity_t * attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod, HitLocation_t);
int  G_EntitiesInRadius(vec3_t center, float radius, gentity_t *ignore, int *entities);
void	 G_BombDamage(vec3_t origin, float damage, float radius, float maxRadius, int mod);
qboolean G_RadiusDamage  (vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod);
void	 body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );
void	 TossClientItems( gentity_t *self );
void	 G_DoKnockBack( gentity_t *targ, vec3_t dir, float knockback );

// damage flags
#define DAMAGE_RADIUS				0x00000001			 // damage was indirect
#define DAMAGE_NO_ARMOR 			0x00000002			 // armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK 		0x00000004			 // do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION		0x00000008			 // armor, shields, invulnerability, and godmode have no effect
#ifdef MISSIONPACK
 #define DAMAGE_NO_TEAM_PROTECTION	0x00000010			 // armor, shields, invulnerability, and godmode have no effect
#endif

//
// g_missile.c
void G_RunCorpse		( gentity_t *ent );
void G_FindSafeAngle(gentity_t *ent);

//
void	   G_RunMissile 		  ( gentity_t *ent );
void	   G_ExplodeMissile 	  ( gentity_t *ent );

gentity_t *fire_blaster (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_plasma (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_grenade (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_bfg (gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_grapple (gentity_t *self, vec3_t start, vec3_t dir);
#ifdef MISSIONPACK
gentity_t *fire_nail( gentity_t *self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up );
gentity_t *fire_prox( gentity_t *self, vec3_t start, vec3_t aimdir );
#endif

// Urban Terror
gentity_t *throw_knife (gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *G_EmbedKnife ( gentity_t *knife, trace_t *trace, vec3_t from );
void	   G_RunKnife ( gentity_t *ent );

void	   G_MrSentryThink( gentity_t *ent);
void	   G_GrenadeShutdown( gentity_t *ent ) ;
void	   G_GrenadeSmoking( gentity_t *ent );

//
// g_mover.c
//
void	   G_RunMover( gentity_t *ent );
void	   Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );
gentity_t *UTFindDesiredStop(char *name );
void	   UTSetDesiredStop(gentity_t *ent, gentity_t *stop);
void	   blocked_ut_train( gentity_t *ent, gentity_t *other );
void	   Think_MatchTeam( gentity_t *ent );
//
// g_trigger.c
//
void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace );

//
// g_misc.c
//
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );
#ifdef MISSIONPACK
void DropPortalSource( gentity_t *ent );
void DropPortalDestination( gentity_t *ent );
#endif

//
// g_weapon.c
//
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
void	 CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint );
void	 CalcEyePoint ( gentity_t *ent, vec3_t eyePoint );
//void	   SnapVectorTowards( vec3_t v, vec3_t to );

#define SnapVectorTowards(v,to) do{\
	 if (to[0] <= v[0]) v[0] = (int)v[0]; else v[0] = (int)v[0] + 1;\
	 if (to[1] <= v[1]) v[1] = (int)v[1]; else v[1] = (int)v[1] + 1;\
	 if (to[2] <= v[2]) v[2] = (int)v[2]; else v[2] = (int)v[2] + 1;\
	}while(0)

qboolean CheckGauntletAttack( gentity_t *ent );
void	 Weapon_HookFree (gentity_t *ent);
void	 Weapon_HookThink (gentity_t *ent);
void	 Weapon_Knife_Fire ( gentity_t *ent );

//
// g_client.c
//
team_t	   TeamCount( int ignoreClientNum, team_t team );
team_t	   TeamLiveCount( int ignoreClientNum, team_t team );
int 	   GhostCount( int ignoreClientNum, team_t team );
int 	   GetAliveClient ( int firstClient );
int 	   TeamLeader( team_t team );
team_t	   PickTeam( int ignoreClientNum );
void	   SetClientViewAngle( gentity_t *ent, vec3_t angle );
//gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles );
#define SelectSpawnPoint SelectRandomFurthestSpawnPoint
gentity_t *SelectRandomFurthestSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles );
//gentity_t *CopyToBodyQue( gentity_t *ent );
void	   respawn (gentity_t *ent);
void	   BeginIntermission (void);
void	   InitClientPersistant (gclient_t *client);
void	   InitClientResp (gclient_t *client);
//void InitBodyQue (void);
void	   ClientSpawn( gentity_t *ent );
void	   player_die (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void	   AddScore( gentity_t *ent, vec3_t origin, int score );
void	   CalculateRanks( void );
qboolean   SpotWouldTelefrag( gentity_t *spot );
gentity_t *SelectNPCSpawnPoint ( gentity_t *npc, vec3_t avoidPoint, vec3_t origin, vec3_t angles );
//
// g_svcmds.c
//
qboolean   ConsoleCommand( void );
void	   G_ProcessIPBans(void);
qboolean   G_FilterPacket (char *from);
qboolean   RemoveIP( char *ip );
qboolean   AddIP ( char *str, qboolean write );
gclient_t *ClientForString( const char *s );

int 	   TimeToMins ( qtime_t *time );
void	   MinsToTime ( const int minutes, qtime_t *result );
void	   expireBans ( void );
qboolean   SetBanTime ( char *ipaddr, int minutes, qboolean write );

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent );
#ifdef MISSIONPACK
void G_StartKamikaze( gentity_t *ent );
#endif

//
// p_hud.c
//
void MoveClientToIntermission (gentity_t *client);
void G_SetStats (gentity_t *ent);

//
// g_cmds.c
//
char *GetMapSoundingLike(gentity_t *ent, const char *s);

//
// g_pweapon.c
//

//
// g_main.c
//
void	   FindIntermissionPoint( void );
void	   SetLeader(team_t team, int client);
void	   CheckTeamLeader( team_t team );
void	   G_RunThink (gentity_t *ent);
void QDECL G_LogPrintf( const char *fmt, ... );
void	   SendScoreboardMessageToAllClients( void );
void	   SendScoreboardSingleMessageToAllClients(gentity_t *entity);
void	   SencScoreBoardDoubleMessageToAllClients(gentity_t *ent1, gentity_t *ent2);
void	   SendScoreboardGlobalMessageToAllClients(void);
void QDECL G_Printf( const char *fmt, ... );
void QDECL G_Error( const char *fmt, ... );
void	   LogExit( const char *string );
#ifdef USE_AUTH
void SendHaxReport(void); //@r00t: Send hax report to master server
void InitHaxNetwork(void);
#endif

//
// g_client.c
//
char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot );
void  ClientUserinfoChanged( int clientNum );
void  ClientDisconnect( int clientNum );
void  ClientBegin( int clientNum );
void  ClientCommand( int clientNum );

//
// g_active.c
//
void ClientThink( int clientNum );
void ClientEndFrame( gentity_t *ent );
void G_RunClient( gentity_t *ent );
void G_SetGhostMode( gentity_t *ent );

//
// g_team.c
//

#define OnSameTeam(ent1,ent2) \
   (ent1->client && ent2->client && g_gametype.integer != GT_JUMP && g_gametype.integer >= GT_TEAM &&\
   ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam)

//qboolean OnSameTeam				   ( gentity_t *ent1, gentity_t *ent2 );
void	 Team_CheckDroppedItem	 ( gentity_t *dropped );
int 	 Team_TouchOurFlag		 ( gentity_t *ent, gentity_t *other, team_t team );
void	 utBombThink			 ( gentity_t *ent );
void	 utBombRespawn			 ( void );
void	 G_InitUTBotItemGoals	 ( void );
qboolean G_InitUTSpawnPoints	 (int gametype );
qboolean G_InitUTSpawnPointsTeamSurvivor ( int gametype, qboolean allowswaps ) ;
// Bomb mode related functions
void	 utBombInitRound(void); 		// initialization of rounds
// Bomb mode think functions
void	 utBombThink_Plant( gentity_t *ent );
void	 utBombThink_Think( gentity_t *ent );
void	 utBombThink_Explode( gentity_t *end );

void	 CheckFlags(void);

void	 G_FindTeams( void );

//
// g_session.c
//
void G_ReadSessionData( gclient_t *client );
void G_InitSessionData( gclient_t *client, char *userinfo );

void G_InitWorldSession(void);
void G_WriteSessionData( void );

//void G_InitSurvivorRoundData ( gclient_t *client );
//void G_ReadSurvivorRoundData ( gclient_t *client );
//void G_WriteSurvivorRoundData ( gclient_t *client );

//
// g_arenas.c
//
void UpdateTournamentInfo( void );
void SpawnModelsOnVictoryPads( void );

//
// g_antilag.c
//
void G_ResetTrail( gentity_t *ent );
//void G_StoreTrail( gentity_t *ent );

void		G_FIFOTimeShiftClient( gentity_t *ent, int time );
void		G_StoreFIFOTrail( gentity_t *ent );

//void G_TimeShiftClient( gentity_t *ent, int time );
void		G_TimeShiftAllClients( int time, gentity_t *skip );
void		G_UnTimeShiftAllClients( gentity_t *skip );
//qboolean AntiLag_ValidateTrace ( gentity_t* ent, gentity_t* other, vec3_t endpos );

void G_SendBBOfAllClients(gentity_t *skip );

//
// g_bot.c
//
void	 G_InitBots( qboolean restart );
void	 G_FreeBots( void );
void	 G_InitNPC( gentity_t *ent );
char *	 G_GetBotInfoByNumber( int num );
char *	 G_GetBotInfoByName( const char *name );
void	 G_CheckBotSpawn( void );
void	 G_RemoveQueuedBotBegin( int clientNum );
qboolean G_BotConnect( int clientNum, qboolean restart );
void	 Svcmd_AddBot_f( void );
void	 Svcmd_BotList_f( void );
void	 BotInterbreedEndMatch( void );
void	 AddNPCToSpawnQueue( int clientNum, int delay );

// ai_main.c
#define MAX_FILEPATH 144

//bot settings
typedef struct bot_settings_s
{
	char   characterfile[MAX_FILEPATH];
	float  skill;
	char   team[MAX_FILEPATH];
} bot_settings_t;

int  BotAISetup( int restart );
int  BotAIShutdown( int restart );
int  BotAILoadMap( int restart );
int  BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);
int  BotAIShutdownClient( int client, qboolean restart );
int  BotAIStartFrame( int time );
void BotTestAAS(vec3_t origin);

#include "g_team.h" // teamplay specific stuff

extern level_locals_t  level;

// DensitY
extern int	num_levelspawn;
extern int	cur_levelspawn;
//extern local_backup_spawn_t		levelspawn[ MAX_GENTITIES ];

extern gentity_t  g_entities[MAX_GENTITIES];

/* TODO: size_t is incorrect */
#define FOFS(x) ((size_t)&(((gentity_t *)0)->x))

extern vmCvar_t  g_fps;
extern vmCvar_t  g_gametype;
extern vmCvar_t  g_dedicated;
extern vmCvar_t  g_dedAutoChat;
extern vmCvar_t  g_cheats;
extern vmCvar_t  g_maxclients;				// allow this many total, including spectators
extern vmCvar_t  g_maxGameClients;			// allow this many active
extern vmCvar_t  g_restarted;

extern vmCvar_t  g_dmflags;
extern vmCvar_t  g_fraglimit;
extern vmCvar_t  g_timelimit;
extern vmCvar_t  g_capturelimit;
extern vmCvar_t  g_friendlyFire;
extern vmCvar_t  g_password;
extern vmCvar_t  g_needpass;
extern vmCvar_t  g_gravity;
extern vmCvar_t  g_speed;
extern vmCvar_t  g_knockback;
extern vmCvar_t  g_forcerespawn;
extern vmCvar_t  g_inactivity;
extern vmCvar_t  g_debugMove;
extern vmCvar_t  g_synchronousClients;
extern vmCvar_t  g_motd;

extern vmCvar_t  g_warmup;
extern vmCvar_t  g_doWarmup;
extern vmCvar_t  g_blood;
extern vmCvar_t  g_allowVote;
extern vmCvar_t  g_teamAutoJoin;
extern vmCvar_t  g_teamForceBalance;
//extern	vmCvar_t	g_banIPs;
extern vmCvar_t  g_filterBan;
extern vmCvar_t  g_antilagvis;
//extern vmCvar_t  g_noSpread;

//extern	vmCvar_t	pmove_fixed;
//extern	vmCvar_t	pmove_msec;
extern vmCvar_t  g_enableDust;
extern vmCvar_t  g_enableBreath;
extern vmCvar_t  g_singlePlayer;
extern vmCvar_t  g_loghits;

// Urban terror
extern vmCvar_t  g_survivor;
extern vmCvar_t  g_RoundTime;
extern vmCvar_t  g_aries;
extern vmCvar_t  g_bombPlantTime;
extern vmCvar_t  g_bombDefuseTime;
extern vmCvar_t  g_bombExplodeTime;
extern vmCvar_t  g_cahTime;
extern vmCvar_t  g_sniperGameStartTime;
extern vmCvar_t  g_enablePrecip;
//extern	vmCvar_t	g_precipAmount;
//extern	vmCvar_t	g_surfacedefault;
//extern	vmCvar_t	g_skins;
extern vmCvar_t  g_gear;
extern vmCvar_t  g_respawnDelay;

extern vmCvar_t  g_redTeamList;
extern vmCvar_t  g_blueTeamList;

/*
extern	vmCvar_t	g_allowBulletPrediction;
*/

extern vmCvar_t  g_mapCycle;
extern vmCvar_t  g_allowChat;
extern vmCvar_t  g_refPass;
extern vmCvar_t  g_refClient;
extern vmCvar_t  g_referee;
extern vmCvar_t  g_refNoBan;
extern vmCvar_t  g_maxteamkills;
extern vmCvar_t  g_teamkillsForgetTime;

extern vmCvar_t  g_respawnProtection;
//extern	vmCvar_t	g_followEnemy;
//extern	vmCvar_t	g_followForced;
extern vmCvar_t  g_followStrict;
extern vmCvar_t  g_removeBodyTime;
extern vmCvar_t  g_bulletPredictionThreshold;
extern vmCvar_t  g_maintainTeam;

extern vmCvar_t  g_failedVoteTime;
extern vmCvar_t  g_newMapVoteTime;
extern vmCvar_t  g_flagReturnTime;
extern vmCvar_t  g_ClientReconnectMin;
extern vmCvar_t  g_JoinMessage;

extern vmCvar_t  g_antiwarp;
extern vmCvar_t  g_antiwarptol;

extern vmCvar_t  g_matchMode;

extern vmCvar_t  g_nameRed;
extern vmCvar_t  g_nameBlue;

extern vmCvar_t  g_timeouts;
extern vmCvar_t  g_timeoutLength;

extern vmCvar_t  g_waveRespawns;
extern vmCvar_t  g_redWave;
extern vmCvar_t  g_blueWave;

extern vmCvar_t  g_pauseLength;

extern vmCvar_t  g_maxrounds;
extern vmCvar_t  g_swaproles;

extern vmCvar_t  g_deadchat;
extern vmCvar_t  g_hotpotato;
extern vmCvar_t  g_armbands;
extern vmCvar_t  g_NextMap;

extern vmCvar_t  g_debugAries;
extern vmCvar_t  g_ctfUnsubWait;

//extern   vmCvar_t g_bombRules;

extern vmCvar_t  g_noDamage;
extern vmCvar_t  g_stamina;
extern vmCvar_t  g_allowPosSaving;
extern vmCvar_t	 g_walljumps;
extern vmCvar_t  g_jumpruns;
extern vmCvar_t  g_allowGoto;
extern vmCvar_t  g_persistentPositions;

extern vmCvar_t  g_skins;
extern vmCvar_t  g_funstuff;
extern vmCvar_t  g_inactivityAction;
extern vmCvar_t  g_shuffleNoRestart;

/* New in 4.3 */
extern vmCvar_t  g_instagib;		/* instagib one-shot-kill modifier */
extern vmCvar_t  g_hardcore;		/* hardcore mode */
extern vmCvar_t  g_randomorder;		/* random spawn order */
extern vmCvar_t  g_thawTime;		/* freeze tag: seconds to thaw a teammate */
extern vmCvar_t  g_meltdownTime;	/* freeze tag: seconds before frozen player melts */
extern vmCvar_t  g_stratTime;		/* strategy phase (Bomb/Survivor; 2-10s) */
extern vmCvar_t  g_autobalance;		/* auto team balance */
extern vmCvar_t  g_vest;			/* kevlar vest forced on/off */
extern vmCvar_t  g_helmet;			/* kevlar helmet forced on/off */
extern vmCvar_t  g_noVest;			/* disable vest in Jump mode */
extern vmCvar_t  g_nameRed;			/* custom red team name */
extern vmCvar_t  g_nameBlue;		/* custom blue team name */
extern vmCvar_t  g_modversion;		/* = "4.3.4" (ROM) */
extern vmCvar_t  g_nextCycleMap;	/* next map queued in cycle */
extern vmCvar_t  g_nextMap;			/* override next map */
extern vmCvar_t  g_refNoExec;		/* prevent referee from exec-ing configs */
extern vmCvar_t  g_teamScores;		/* publish team scores configstring */
extern vmCvar_t  g_bombDefuseTime;	/* bomb defuse time in seconds */
extern vmCvar_t  g_bombExplodeTime;	/* bomb explode time in seconds */
extern vmCvar_t  g_bombPlantTime;	/* bomb plant time in seconds */
extern vmCvar_t  g_spSkill;			/* single player skill level */
extern vmCvar_t  g_mrsentry;		/* enable Mr. Sentry turret entities */

void	 trap_Printf( const char *fmt );
void	 trap_Error( const char *fmt );
int 	 trap_Milliseconds( void );
int 	 trap_Argc( void );
void	 trap_Argv( int n, char *buffer, int bufferLength );
void	 trap_Args( char *buffer, int bufferLength );
int 	 trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void	 trap_FS_Read( void *buffer, int len, fileHandle_t f );
void	 trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void	 trap_FS_FCloseFile( fileHandle_t f );
int 	 trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
void	 trap_SendConsoleCommand( int exec_when, const char *text );
void	 trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags );
void	 trap_Cvar_Update( vmCvar_t *cvar );
void	 trap_Cvar_Set( const char *var_name, const char *value );
int 	 trap_Cvar_VariableIntegerValue( const char *var_name );
float	 trap_Cvar_VariableValue( const char *var_name );
void	 trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void	 trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *gameClients, int sizeofGameClient );
void	 trap_DropClient( int clientNum, const char *reason );
#ifdef USE_AUTH
qboolean trap_NET_StringToAdr( const char *s, netadr_t *a);
void	 trap_Auth_DropClient( int clientNum, const char *reason, const char *message );
void	 trap_NET_SendPacket( netsrc_t sock, int length, const void *data, const char *to );
#endif
void	 trap_SendServerCommand( int clientNum, const char *text );
void	 trap_SetConfigstring( int num, const char *string );
void	 trap_GetConfigstring( int num, char *buffer, int bufferSize );
void	 trap_GetUserinfo( int num, char *buffer, int bufferSize );
void	 trap_SetUserinfo( int num, const char *buffer );
void	 trap_GetServerinfo( char *buffer, int bufferSize );
void	 trap_SetBrushModel( gentity_t *ent, const char *name );
void	 trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
int 	 trap_PointContents( const vec3_t point, int passEntityNum );
qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 );
qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
void	 trap_AdjustAreaPortalState( gentity_t *ent, qboolean open );
qboolean trap_AreasConnected( int area1, int area2 );
void	 trap_LinkEntity( gentity_t *ent );
void	 trap_UnlinkEntity( gentity_t *ent );
int 	 trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
int 	 trap_BotAllocateClient( void );
void	 trap_BotFreeClient( int clientNum );
void	 trap_GetUsercmd( int clientNum, usercmd_t *cmd );
qboolean trap_GetEntityToken( char *buffer, int bufferSize );

int 	 trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void	 trap_DebugPolygonDelete(int id);

int 	 trap_BotLibSetup( void );
int 	 trap_BotLibShutdown( void );
int 	 trap_BotLibVarSet(char *var_name, char *value);
int 	 trap_BotLibVarGet(char *var_name, char *value, int size);
int 	 trap_BotLibDefine(char *string);
int 	 trap_BotLibStartFrame(float time);
int 	 trap_BotLibLoadMap(const char *mapname);
int 	 trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue);
int 	 trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);

int 	 trap_BotGetSnapshotEntity( int clientNum, int sequence );
int 	 trap_BotGetServerCommand(int clientNum, char *message, int size);
void	 trap_BotUserCommand(int client, usercmd_t *ucmd);

int 	 trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
int 	 trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info );
void	 trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info);

int 	 trap_AAS_Initialized(void);
void	 trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
float	 trap_AAS_Time(void);

int 	 trap_AAS_PointAreaNum(vec3_t point);
int 	 trap_AAS_PointReachabilityAreaIndex(vec3_t point);
int 	 trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas);

int 	 trap_AAS_PointContents(vec3_t point);
int 	 trap_AAS_NextBSPEntity(int ent);
int 	 trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size);
int 	 trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v);
int 	 trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value);
int 	 trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value);

int 	 trap_AAS_AreaReachability(int areanum);

int 	 trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int 	 trap_AAS_EnableRoutingArea( int areanum, int enable );
int 	 trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/ *route, int areanum, vec3_t origin,
					   int goalareanum, int travelflags, int maxareas, int maxtime,
					   int stopevent, int stopcontents, int stoptfl, int stopareanum);

int 	 trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
				   void /*struct aas_altroutegoal_s*/ *altroutegoals, int maxaltroutegoals,
				   int type);
int 	 trap_AAS_Swimming(vec3_t origin);
int 	 trap_AAS_PredictClientMovement(void /* aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);

void	 trap_EA_Say(int client, char *str);
void	 trap_EA_SayTeam(int client, char *str);
void	 trap_EA_Command(int client, char *command);

void	 trap_EA_Action(int client, int action);
void	 trap_EA_Gesture(int client);
void	 trap_EA_Talk(int client);
void	 trap_EA_Attack(int client);
void	 trap_EA_Use(int client);
void	 trap_EA_Respawn(int client);
void	 trap_EA_Crouch(int client);
void	 trap_EA_MoveUp(int client);
void	 trap_EA_MoveDown(int client);
void	 trap_EA_MoveForward(int client);
void	 trap_EA_MoveBack(int client);
void	 trap_EA_MoveLeft(int client);
void	 trap_EA_MoveRight(int client);
void	 trap_EA_SelectWeapon(int client, int weapon);
void	 trap_EA_Jump(int client);
void	 trap_EA_DelayedJump(int client);
void	 trap_EA_Move(int client, vec3_t dir, float speed);
void	 trap_EA_View(int client, vec3_t viewangles);

void	 trap_EA_EndRegular(int client, float thinktime);
void	 trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input);
void	 trap_EA_ResetInput(int client);

int 	 trap_BotLoadCharacter(char *charfile, float skill);
void	 trap_BotFreeCharacter(int character);
float	 trap_Characteristic_Float(int character, int index);
float	 trap_Characteristic_BFloat(int character, int index, float min, float max);
int 	 trap_Characteristic_Integer(int character, int index);
int 	 trap_Characteristic_BInteger(int character, int index, int min, int max);
void	 trap_Characteristic_String(int character, int index, char *buf, int size);

int 	 trap_BotAllocChatState(void);
void	 trap_BotFreeChatState(int handle);
void	 trap_BotQueueConsoleMessage(int chatstate, int type, char *message);
void	 trap_BotRemoveConsoleMessage(int chatstate, int handle);
int 	 trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm);
int 	 trap_BotNumConsoleMessages(int chatstate);
void	 trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int 	 trap_BotNumInitialChats(int chatstate, char *type);
int 	 trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int 	 trap_BotChatLength(int chatstate);
void	 trap_BotEnterChat(int chatstate, int client, int sendto);
void	 trap_BotGetChatMessage(int chatstate, char *buf, int size);
int 	 trap_StringContains(char *str1, char *str2, int casesensitive);
int 	 trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context);
void	 trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size);
void	 trap_UnifyWhiteSpaces(char *string);
void	 trap_BotReplaceSynonyms(char *string, unsigned long int context);
int 	 trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname);
void	 trap_BotSetChatGender(int chatstate, int gender);
void	 trap_BotSetChatName(int chatstate, char *name, int client);
void	 trap_BotResetGoalState(int goalstate);
void	 trap_BotRemoveFromAvoidGoals(int goalstate, int number);
void	 trap_BotResetAvoidGoals(int goalstate);
void	 trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal);
void	 trap_BotPopGoal(int goalstate);
void	 trap_BotEmptyGoalStack(int goalstate);
void	 trap_BotDumpAvoidGoals(int goalstate);
void	 trap_BotDumpGoalStack(int goalstate);
void	 trap_BotGoalName(int number, char *name, int size);
int 	 trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int 	 trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int 	 trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags);
int 	 trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime);
int 	 trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal);
int 	 trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal);
int 	 trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal);
int 	 trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal);
int 	 trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal);
float	 trap_BotAvoidGoalTime(int goalstate, int number);
void	 trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void	 trap_BotInitLevelItems(void);
void	 trap_BotUpdateEntityItems(void);
int 	 trap_BotLoadItemWeights(int goalstate, char *filename);
void	 trap_BotFreeItemWeights(int goalstate);
void	 trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void	 trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename);
void	 trap_BotMutateGoalFuzzyLogic(int goalstate, float range);
int 	 trap_BotAllocGoalState(int state);
void	 trap_BotFreeGoalState(int handle);

void	 trap_BotResetMoveState(int movestate);
void	 trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags);
int 	 trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void	 trap_BotResetAvoidReach(int movestate);
void	 trap_BotResetLastAvoidReach(int movestate);
int 	 trap_BotReachabilityArea(vec3_t origin, int testground);
int 	 trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target);
int 	 trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target);
int 	 trap_BotAllocMoveState(void);
void	 trap_BotFreeMoveState(int handle);
void	 trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove);
void	 trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);

int 	 trap_BotChooseBestFightWeapon(int weaponstate, int *inventory);
void	 trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo);
int 	 trap_BotLoadWeaponWeights(int weaponstate, char *filename);
int 	 trap_BotAllocWeaponState(void);
void	 trap_BotFreeWeaponState(int weaponstate);
void	 trap_BotResetWeaponState(int weaponstate);

int 	 trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child);

void	 trap_SnapVector( float *v );
int 	 trap_RealTime( qtime_t *qtime );

// Urban terror
void utLaserOn						   (gentity_t *ent ) ;
void utLaserOff 					   (gentity_t *ent ) ;

void ClientWriteSurvivor			   ( gclient_t *client );
void ClientReadSurvivor 			   ( gclient_t *client );

qboolean   utTrace					   ( gentity_t *ent, trace_t *tr, vec3_t muzzle, vec3_t end, float damage, ariesTraceResult_t *result, qboolean antiLag );
float	   utBulletHit				   ( gentity_t *ent, trace_t *tr, int weapon, ariesTraceResult_t *result, qboolean reportHit, float *damaccum );
void	   utKickHit				   ( gentity_t *ent, gentity_t *other, ariesTraceResult_t *hit );
float	   utWeaponCalcDamage		   ( gentity_t *ent, gentity_t *other, int weapon, ariesTraceResult_t *result, ariesHitLocation_t *hit, float *damaccum );

void	   StatsEngineFire			   (int shooter, int weaponID, int hitloc, int victim, float damage);

void	   G_ReduceStamina			   ( gentity_t *ent, int amount );
void	   G_Kick					   ( gentity_t *kicker, gentity_t *target );

void	   G_Goomba 				   ( gentity_t *kicker, gentity_t *target, int dmg );

void	   target_location_linkup	   (gentity_t *ent);
void	   SP_InitMover 			   ( gentity_t * );

void	   utHistorySet 			   ( gentity_t *ent );
void	   utHistoryApply			   ( gentity_t *ent, gentity_t *ref );
void	   utHistoryUndo			   ( gentity_t *ent );
qboolean   utMapCycleGotoNext		   ( void );

void	   G_WriteClientSessionData    ( gclient_t *client );
qboolean   utHistoryValidateTrace	   ( gentity_t *ent, gentity_t *other, vec3_t endpos );

gentity_t *utFlagSpawn				   ( vec3_t origin, team_t team );

void	   Switch_Train_Stops		   ( gentity_t *ent, int trainID );

void	   G_InitClientSessionData	   ( gclient_t *client );

float	   GetDistanceToFlag		   (vec3_t origin, team_t team);
float	   GetDistanceToBombCarrier    (vec3_t origin);

void	   BuildTeamLists			   (void);

void	   AntiLag_SetReferenceClient  ( gentity_t *ref );
qboolean   AntiLag_ValidateTrace	   ( gentity_t *ent, gentity_t *other, vec3_t endpos );
void	   AntiLag_UpdateClientHistory ( gentity_t *ent );
void	   FetchMapTeams(void);
// 27 for the minimap
void	   MapArrowSpawn(int ClientNum);
void	   MapArrowThink(gentity_t *ent );
int 	   PickBombHolderClientNum( int Seed );
void	   G_CreateCorpse(gentity_t *cent, int damage, int mod);
void	   g_CycleMap(void);

///////////////////////////////////////////////
int  StatsEngineLocID(int l);
int  StatsEngineWepID(int w);
void StatsSendBuffer(char *buffer, int clientnum);

int  Stats_GetTotalKills(gentity_t *ent);
int  Stats_GetTotalDeaths(gentity_t *ent);
int  Stats_GetTotalDamageGiven(gentity_t *ent);
int  Stats_GetMostFiredGun(gentity_t *ent);
int  Stats_GetAmmoFired(gentity_t *ent);

void Stats_UpdateKD(gentity_t *ent);
int  Stats_CalcRatio(int kills, int deaths);

void Stats_AddScore(gentity_t *ent, tr_score_stats type);
void Stats_ClearAll(void);
int  Stats_GetTotalLocations(gentity_t *ent, int loc, qboolean other);
void Stats_AddTK(gentity_t *ent, gentity_t *vic);

void Stats_NegStat(tStatsRecord *st);
int  Stats_FindBestWeaponStat(tStatsRecord *st, int *ut_wepid, int count);
void Stats_Leaderboard(void);
void StatsEnginePrint(gentity_t *sent);
void StatsEngineDeath(int shooter, int mod, int victim);

///Random missing definitions///
void	   G_UnTimeShiftClient( gentity_t *ent ) ;

void	   SendScoreboardDoubleMessageToAllClients(gentity_t *ent1, gentity_t *ent2);
char *	   allowedGear(int arg);
void	   Cmd_Substitute_f(gentity_t *ent);
void	   G_AutoRadio(int number, gentity_t *ent);
int 	   Contraband( gentity_t *ent );
void	   Svcmd_Restart_f(qboolean DoAnotherWarmUp, qboolean KeepScores);
qboolean   BecomeSubstitute(gentity_t *ent);
void	   Svcmd_SwapRoles(void);
void	   Team_ReturnFlagSound( gentity_t *ent, team_t team ) ;
void	   SetMoverState( gentity_t *ent, moverState_t moverState, int time ) ;
void	   G_DoMoveOutCall(void);
gentity_t *Team_ResetFlag( team_t team, int explode ) ;
void	   CheckVote(qboolean veto);
void	   DoMute(void);
////////////////////////////////

//@Barba: jump mode code
void	   Use_JumpStartLine( gentity_t *ent, gentity_t *other, gentity_t *activator );
void	   Use_JumpStopLine( gentity_t *ent, gentity_t *other, gentity_t *activator );
void	   Use_JumpTimerCancel( gentity_t *ent, gentity_t *other, gentity_t *activator );

int    trap_PC_LoadSource( const char *filename );
int    trap_PC_FreeSource( int handle );
int    trap_PC_ReadToken( int handle, pc_token_t *pc_token );

#ifdef USE_AUTH
//@Barbatos @kalish
void SV_Auth_Init (void);
void SV_Auth_Request( int clientNum );
void SV_Authserver_Get_Packet( void );
void SV_Authserver_Heartbeat ( void );
void SV_Authserver_Shutdown( void );
void SV_Authserver_Whois( int idClient );
void SV_Authserver_Ban( int clientNum, int days, int hours, int mins );
void SV_Auth_Print_User_Infos_f( gclient_t *cl, int target );
void SV_Auth_DropClient( int idnum, char *reason, char *message );
void SV_Authserver_Send_Packet( const char *cmd, const char *args );
char *G_Argv( int arg );

int Auth_cat_str( char *output, int outlen, const char *input, int quoted );
int Auth_cat_int( char *output, int outlen, int num, int quoted );
char *Auth_encrypt( char *string );
#endif

#endif
