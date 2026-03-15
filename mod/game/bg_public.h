/**
 * Filename: bg_public.h
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 * Copyright (C) 2010-2018 FrozenSand
 *
 * This file is part of Urban Terror 4.3.4 source code.
 * Reconstructed from UrT 4.2 GPL source and 4.3.4 QVM binary analysis.
 *
 * Definitions shared by both the server game and client game
 * modules because games can change separately from the main system
 * version we need a second version that must match between game and cgame
 */
#ifndef _BG_PUBLIC_H_
#define _BG_PUBLIC_H_

#include "animdef.h"
#include "../common/c_players.h"

#define _MAINTAIN_TEAM

#define PMOVE_MSEC 8
/*1000/125 = 8	ie 125fps */

#define DEFAULT_GRAVITY 			800
#define GIB_HEALTH					-40
#define ARMOR_PROTECTION			0.66

#define MAX_ITEMS					256

#define UT_MAX_HEALTH				100
#define UT_MAX_HEAL_MEDKIT			90
#define UT_MAX_HEAL 				50
#define UT_STAM_MUL 				300
#define UT_STAM_SLIDE_DRAIN 		33.33f // Levant called it. wtfbbq
#define UT_WEAPONHIDE_TIME			300
#define UT_SHOTGUN_COUNT			20
#define UT_DRIFT_RADIUS 			0.8f
#define UT_MAX_RECOIL				30000
#define UT_HEAL_TIME				1500
#define UT_BANDAGE_TIME 			1000
#define UT_RECOIL_TIME				800
#define UT_MAX_RUNSPEED 			220
#define UT_MAX_SPRINTSPEED			340
#define UT_MAX_DISTANCE 			(8192 * 2)
#define UT_MAX_CLIPS				8

#define UT_INACCURACY_THRESHOLD 	300
#define UT_INACCURACY_TIME			1400
#define UT_MAX_MOVEMENT_INACCURACY	(UT_INACCURACY_THRESHOLD + UT_INACCURACY_TIME)

#define RANK_TIED_FLAG				0x4000

#define DEFAULT_SHOTGUN_SPREAD		700
#define DEFAULT_SHOTGUN_COUNT		11

/* item sizes are needed for client side pickup detection */
#define ITEM_RADIUS 				15

#define LIGHTNING_RANGE 			768

/* for the CS_SCORES[12] when only one player is present */
#define SCORE_NOT_PRESENT			-9999

/* 30 seconds before vote times out */
#define VOTE_TIME					30000

#define MINS_Z						-24
#define DEFAULT_VIEWHEIGHT			38
#define CROUCH_VIEWHEIGHT			20
#define DEAD_VIEWHEIGHT 			-16
#define SWIM_VIEWHEIGHT 			4

/* maximum number of rain/snow particles allowed */
#define UTPRECIP_MAX_DROPS			256

/*
 * config strings are a general means of communicating variable length
 * strings from the server to all connected clients.
 */

/* CS_SERVERINFO and CS_SYSTEMINFO are defined in q_shared.h */
/* Swapped , old red score, old blue score */
#define CS_ROUNDSTAT		  2
/* from the map worldspawn's message field */
#define CS_MESSAGE			  3
/* g_motd string for server message of the day */
#define CS_MOTD 			  4
/* server time when the match will be restarted */
#define CS_WARMUP			  5
#define CS_SCORES1			  6
#define CS_SCORES2			  7
#define CS_VOTE_TIME		  8
#define CS_VOTE_STRING		  9
#define CS_VOTE_YES 		  10
#define CS_VOTE_NO			  11

#define CS_TEAMVOTE_TIME	  12
#define CS_TEAMVOTE_STRING	  14
#define CS_TEAMVOTE_YES 	  16
#define CS_TEAMVOTE_NO		  18

#define CS_GAME_VERSION 	  20
/* so the timer only shows the current level */
#define CS_LEVEL_START_TIME   21
/*
 * when 1, fraglimit/timelimit has been hit and intermission will start in a
 * second or two
 */
#define CS_INTERMISSION 	  22
/* string indicating flag status in CTF */
#define CS_FLAGSTATUS		  23
#define CS_ROUND_END_TIME	  24
#define CS_BOTINFO			  25

/*#define CS_TEAM_NAME_RED	  34 */
/*#define CS_TEAM_NAME_BLUE   35 */
#define CS_MODELS 32

/*#define CG_DEFUSE_TIMER	  37 Amount of timer to defuse the bomb */
/*#define CG_DEFUSE_START	  38 */

//@Barbatos
//#define CS_GAME_VERSION_UPDATER 26

#define CS_SOUNDS	 			(CS_MODELS + MAX_MODELS)
#define CS_PLAYERS	 			(CS_SOUNDS + MAX_SOUNDS)
#define CS_FLAGS	 			(CS_PLAYERS + MAX_CLIENTS)
#define CS_LOCATIONS 			(CS_FLAGS + MAX_FLAGS)
#define CS_UTEXT	 			(CS_LOCATIONS + MAX_LOCATIONS)
#define CS_MAX		 			(CS_UTEXT + MAX_UTEXT)

#define CS_ROUNDCOUNT		  	(CS_UTEXT+0)
#define CS_ROUNDWINNER		  	(CS_UTEXT+1)
#define CS_CAPTURE_SCORE_TIME 	(CS_UTEXT+2)
#define CS_GAME_ID			  	(CS_UTEXT+3)
#define CS_PAUSE_STATE		  	(CS_UTEXT+4)
#define CS_MATCH_STATE		  	(CS_UTEXT+5)
#define CS_HOTPOTATOTIME	  	(CS_UTEXT+6)
#define CS_BOMBPLANTTIME	  	(CS_UTEXT+7)

#define CS_GAME_VERSION_UPDATER (CS_UTEXT+8)

//Jump mode config strings
#define CS_NODAMAGE 		  	(CS_UTEXT+9)
#define CS_FUNSTUFF             (CS_UTEXT+10)

/* New in 4.3 */
#define CS_TEAMNAME_RED         (CS_UTEXT+11)   /* custom red team name */
#define CS_TEAMNAME_BLUE        (CS_UTEXT+12)   /* custom blue team name */
#define CS_FREEZE_THAW          (CS_UTEXT+13)   /* freeze tag: thaw timer configstring */

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif

typedef enum {
	GT_FFA, 			/* free for all */
	GT_LASTMAN, 		/* single player ffa */
	GT_SINGLE_PLAYER,	/* single player- reserved by quake 3. */

	/*-- team games go after this -- */

	GT_TEAM,			/* team deathmatch */
	GT_UT_SURVIVOR, 	/* team survivor */
	GT_UT_ASSASIN,		/* follow the leader */
	GT_UT_CAH,			/* capture and hold */
	GT_CTF, 			/* capture the flag */
	GT_UT_BOMB, 		/* bomb mode! */
	GT_JUMP,			/* Jump */
	GT_FREEZE,			/* Freeze Tag — new in 4.3 */
	GT_GUNGAME,			/* Gun Game — new in 4.3 */

	GT_MAX_GAME_TYPE,
} gametype_t;

typedef enum { GENDER_MALE, GENDER_FEMALE, GENDER_NEUTER } gender_t;
typedef enum { SKINCOLOR_WHITE, SKINCOLOR_BLACK }	   skincolor_t;

/**
 * pmove module
 * The pmove code takes a player_state_t and a usercmd_t and generates a
 * new player_state_t and some other output data.  Used for local prediction
 * on the client game and true movement on the server game.
 */

typedef enum {
	PM_NORMAL,		/* can accelerate and turn */
	PM_NOCLIP,		/* noclip movement */
	PM_SPECTATOR,	/* still run into walls */
	PM_DEAD,		/* no acceleration or turning, but free falling */
	PM_FREEZE,		/* stuck in place with no control */
	PM_INTERMISSION,/* no movement or status bar */
} pmtype_t;

typedef enum {
	WEAPON_READY,
	WEAPON_RAISING,
	WEAPON_DROPPING,
	WEAPON_FIRING
} weaponstate_t;

typedef enum {
	CCPRINT_FLAG_TAKEN,
	CCPRINT_FLAG_CAPTURED,
	CCPRINT_FLAG_DROPPED,
	CCPRINT_FLAG_RETURN,
	CCPRINT_ENTERED,
	CCPRINT_JOIN,
	CCPRINT_MATCH_SUB,
	CCPRINT_MATCH_ACTIVE,
	CCPRINT_MATCH_FORCESUB,
	CCPRINT_MATCH_UNCAP,
	CCPRINT_MATCH_CAP,
	CCPRINT_TEAM_FULL,
} ccprint_t;

/* pmove->pm_flags */
#define PMF_DUCKED		   (1 << 0)
#define PMF_JUMP_HELD	   (1 << 1)
#define PMF_LADDER_UPPER   (1 << 2)  /* Upper Lader. */
#define PMF_BACKWARDS_RUN  (1 << 3)  /* coast down to backwards run */
#define PMF_UNUSED		   (1 << 4)  /* pm_time is time before rejump */
#define PMF_TIME_KNOCKBACK (1 << 5)  /* pm_time is an air-accelerate only time */
#define PMF_TIME_WATERJUMP (1 << 6)  /* pm_time is waterjump */
#define PMF_RESPAWNED	   (1 << 7)  /* clear after attack and jump buttons come up */
#define PMF_FORCE_DUCKED   (1 << 8)
#define PMF_HAUNTING	   (1 << 9)
#define PMF_FOLLOW		   (1 << 10) /* spectate following another player */
#define PMF_WALL_JUMP	   (1 << 11) /* spectate as a scoreboard */
#define PMF_OTHERLEG	   (1 << 12) /* Upper ladder */
#define PMF_LADDER_LOWER   (1 << 13) /* Lower ladder */
#define PMF_LADDER_IGNORE  (1 << 14)
#define PMF_WALL		   (1 << 15) /* Wall in front of player last time. */

#define PMF_LADDER	   (PMF_LADDER_UPPER | PMF_LADDER_LOWER)

/*#define	PMF_ALL_TIMES	(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_KNOCKBACK) */
#define PMF_ALL_TIMES		(PMF_TIME_WATERJUMP | PMF_TIME_KNOCKBACK)

#define UT_PMF_TEAM_BANDAGE 	(1 << 0)
#define UT_PMF_RELOADING		(1 << 1)
#define UT_PMF_LIMPING			(1 << 2)
#define UT_PMF_BLEEDING 		(1 << 3)
#define UT_PMF_FIRE_HELD		(1 << 4)
#define UT_PMF_BANDAGING		(1 << 5)
#define UT_PMF_CLIMBING 		(1 << 6)
#define UT_PMF_GHOST			(1 << 7)
#define UT_PMF_FROZEN			(1 << 8)
#define UT_PMF_KICKING			(1 << 9)
#define UT_PMF_TEAMSITE 		(1 << 10)
#define UT_PMF_USE_HELD 		(1 << 11)
#define UT_PMF_PLANTING 		(1 << 12)
#define UT_PMF_DEFUSING 		(1 << 13)
#define UT_PMF_READY_FIRE		(1 << 14)
#define UT_PMF_WEAPON_MODE_HELD (1 << 15)

/* New in 4.3 — Freeze Tag state */
#define UT_PMF_FROZEN			(1 << 16)	/* player is currently frozen */

//Fenix
#define UT_PMF_CRASHED          (1 << 0)


#define MAXTOUCH		32
typedef struct {
	/* state (in / out) */
	playerState_t  *ps;

	/* command (in) */
	usercmd_t		cmd;
	/* collide against these types of surfaces */
	int 			tracemask;
	/* if set, diagnostic output will be printed */
	int 			 debugLevel;
	qboolean		 noFiring;
	/* true if a gauntlet attack would actually hit something */
	qboolean		 gauntletHit;

	int    framecount;
	int    evstepHack;

	/* results (out) */
	int numtouch;
	int touchents[MAXTOUCH];

	/* bounding box size */
	vec3_t	mins, maxs;

	int watertype;
	int waterlevel;

	float	xyspeed;

	/* for fixed msec Pmove */
	/*	int pmove_fixed; */
	/*	int pmove_msec; */

	int physics;

	/* callbacks to test the world */
	/* these will be different functions during game and cgame */
	void (*trace)(trace_t *results, const vec3_t start,
			  const vec3_t mins, const vec3_t maxs,
			  const vec3_t end, int passEntityNum, int contentMask);
	int (*pointcontents)(const vec3_t point, int passEntityNum);

	/* Urban terror */
	int   warmup;
	qboolean  isBot;
	qboolean  freeze;
	qboolean paused; //@Barbatos
	int   ariesModel;

	/*
	 * The following two events must not get overwritten by other events.
	 * Therefore they will be added at the end of PmoveSingle.
	 * Note: playerstate event buffer has only two slots.
	 */

	qboolean  weapModeEvent;
	int   weapModeEventParam;

	qboolean  weapFireEvent;
	int   weapFireEventParam;
} pmove_t;

/* if a full pmove isn't done on the client, you can just update the angles */
void PM_UpdateViewAngles(playerState_t *ps, const usercmd_t *cmd);
void Pmove(pmove_t *pmove);

/*=================================================================================== */

/* player_state->stats[] indexes */
/* NOTE: may not have more than 16 */
typedef enum {
	STAT_HEALTH,
	/* 16 bit fields */
	STAT_WEAPONS,
	/*
	 * copy of the clients current buttons.
	 * Could be cool but only used for zooming
	 */
	STAT_BUTTONS,
	/* Amount of time left on recoil */
	STAT_RECOIL,
	/* Amount of tiem tille recoil dissipates */
	STAT_RECOIL_TIME,
	/* Climbing time */
	STAT_CLIMB_TIME,
	/* number of rounds left in a burst */
	STAT_BURST_ROUNDS,
	/* amount of fall damage to inflict */
	STAT_EVENT_PARAM,
	STAT_UTPMOVE_FLAGS,
	/* Players remaining stamina */
	STAT_STAMINA,
	/* Used to show what regions of a player are damaged on the hud */
	STAT_HITS,
	/* Amount of inaccuracy due to movement */
	STAT_MOVEMENT_INACCURACY,
	/* used for flash grenade blindness and sound */
	STAT_BLIND,
	/* Fenix: not proper airmove, used for client side prediction adjustment */
	STAT_AIRMOVE_FLAGS
} statIndex_t;

/*
 * player_state->persistant[] indexes
 * these fields are the only part of player_state that isn't
 * cleared on respawn
 * NOTE: may not have more than 16
 * WARNING: Each entry is ONLY 16bit wide, not 32 !!!
 */
typedef enum {
	PERS_SCORE,  /* !!! PERS_SCORE MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!! */
	/*
	 * total points damage inflicted so damage beeps can
	 * sound on change
	 */
	PERS_HITS,
	/* player rank or team rank */
	PERS_RANK,
	/* player team */
	PERS_TEAM,
	/* incremented every respawn */
	PERS_SPAWN_COUNT,
	/* 16 bits that can be flipped for events */
	PERS_PLAYEREVENTS,
	/* clientnum of last damage inflicter */
	PERS_ATTACKER,
	/* health/armor of last person we attacked */
	PERS_UNUSED0, // was ATTACKEE_ARMOR,
	/* count of the number of times you died */
	PERS_KILLED,
	/* player awards tracking */
	PERS_COMMAND_TIME,

    PERS_SCORERB, //r00t: packed as RED|(BLUE<<8)
    PERS_FLAGSRB, //r00t: packed as RED|(BLUE<<8)

	PERS_UNUSED1,
    PERS_JUMPWAYS, //r00t: packed, CurrentWay | (BestWay<<8)
	PERS_JUMPSTARTTIME,
	PERS_JUMPBESTTIME
} persEnum_t;

/* entityState_t->eFlags */
//@r00t:DEAD/FIRING used by wallhack, change these regularly
/* UT - player wants a medic */
#define EF_UT_MEDIC         0x00000020  /* UT - player wants a medic */
#define EF_POWERSLIDE       0x00000400  /* used to make players play the prox mine ticking sound */
#define EF_TELEPORT_BIT     0x00080000  /* toggled every time the origin abruptly changes */
#define EF_AWARD_EXCELLENT  0x00000100  /* draw an excellent sprite */
#define EF_PLAYER_EVENT     0x00000002
#define EF_BOUNCE           0x00000002  /* for missiles */
#define EF_CHASE            0x00000002  /* for missiles */
#define EF_BOUNCE_HALF      0x00000040  /* for missiles */
#define EF_NOPICKUP         0x00000080  /* Dont allow picking up */
#define EF_UT_DEFUSING      0x00000010  /* Player is defusing the bomb. */
#define EF_VOTED            0x00020000  /* already cast a vote */
#define EF_KAMIKAZE         0x00000004
#define EF_MOVER_STOP       0x00000008  /* will push otherwise */
#define EF_AWARD_CAP        0x00010000  /* draw the capture sprite */
#define EF_TALK             0x00000200  /* draw a talk balloon */
#define EF_CONNECTION       0x00004000  /* draw a connection trouble sprite */
#define EF_FIRING           0x00008000  /* for lightning gun */
#define EF_DEAD             0x00001000  /* don't draw a foe marker over players with EF_DEAD */
#define EF_NODRAW           0x00040000  /* may have an event, but no model (unspawned items) */
#define EF_UT_PLANTING      0x00000800  /* Player is planting the bomb. */
#define EF_WHATEVER         0x00000001  /* denied */
#define EF_TEAMVOTED	       0x00002000  /* already cast a team vote */

/* NOTE: may not have more than 16 */
typedef enum {
	PW_NONE,

	PW_REDFLAG,
	PW_BLUEFLAG,
	PW_NEUTRALFLAG,

	PW_VEST,
	PW_SILENCER,
	PW_LASER,
	PW_NVG,
	PW_BLEEDING,
	PW_HELMET,
	PW_BACKPACK,
	PW_MEDKIT,

	PW_NOWEAPON,
	PW_HASNOHEAD,
	PW_HASBULLETHOLEINHEAD,
	PW_CARRYINGBOMB, /*for the bombadeer bands */
	PW_NUM_POWERUPS
} powerup_t;

#define POWERUP(x) (1 << x)

typedef enum {
	HI_NONE,

	HI_TELEPORTER,
	HI_MEDKIT,
	HI_KAMIKAZE,
	HI_PORTAL,
	HI_INVULNERABILITY,

	HI_NUM_HOLDABLE
} holdable_t;

/* reward sounds (stored in ps->persistant[PERS_PLAYEREVENTS]) */
#define PLAYEREVENT_DENIEDREWARD   0x0001
#define PLAYEREVENT_GAUNTLETREWARD 0x0002
#define PLAYEREVENT_HOLYSHIT	   0x0004

/* entityState_t->event values */
/* entity events are for effects that take place reletive */
/* to an existing entities origin.	Very network efficient. */

//@r00t: Dynamic event mapping - call to initialize enc/dec arrays
//DISABLED - NOT STABLE
//#define DYNEVENTS

#ifdef DYNEVENTS
int CSE_ENC(int x);
int CSE_DEC(int x);
extern unsigned char cse_enc_tab[256];
extern unsigned char cse_dec_tab[256];
#else
#define CSE_ENC(x) x
#define CSE_DEC(x) x
#endif

/* two bits at the top of the entityState->event field */
/* will be incremented with each change in the event so */
/* that an identical event started twice in a row can */
/* be distinguished.  And off the value with ~EV_EVENT_BITS */
/* to retrieve the actual event number */
#define EV_EVENT_BIT1	 0x00000100
#define EV_EVENT_BIT2	 0x00000200
#define EV_EVENT_BITS	 (EV_EVENT_BIT1 | EV_EVENT_BIT2)

#define EVENT_VALID_MSEC 300
//@r00t: Events hardcoded in wallhack, randomize often
typedef enum {
	EV_NONE,

	EV_STEP,
	EV_STEP_4,
	EV_STEP_8,
	EV_STEP_12,
	EV_STEP_16,

	/* Urban Terror extra events */
	EV_UT_READY_FIRE,
	EV_UT_BREAK,
	EV_UT_RELOAD,
	EV_UT_BANDAGE,
	EV_UT_LADDER,
	EV_UT_TEAM_BANDAGE,
	EV_UT_BOMB_BASE,
	EV_UT_KICK,
	EV_UT_USE,
	EV_UT_CLEARMARKS,
	EV_UT_C4_DEFUSE,
	EV_UT_C4_PLANT,
	EV_UT_CAPTURE_SCORE,
	EV_UT_CAPTURE,
	EV_UT_SP_SNIPERSTART,	/* start sniper game */
	EV_UT_SP_SNIPEREND, /* end sniper game */
	EV_UT_SP_SNIPERSCORE,	/* scored sniper kill */
	EV_UT_RACESTART,	/* begin timer for obstacle type objective */
	EV_UT_RACEEND,		/* end timer for obstacle type objective */
	EV_UT_SP_SOUND, 	/* play single player map sound */

	EV_FOOTSTEP,
	EV_FOOTSTEP_METAL,
	EV_FOOTSPLASH,
	EV_FOOTWADE,
	EV_SWIM,

	EV_NOAMMO,
	EV_CHANGE_WEAPON,
	EV_FIRE_WEAPON,

	EV_FALL_SHORT,
	EV_FALL_MEDIUM,
	EV_FALL_FAR,

	EV_GENERAL_SOUND,
	EV_GLOBAL_SOUND,	/* no attenuation */
	EV_GLOBAL_TEAM_SOUND,

	EV_BULLET_HIT_FLESH,
	EV_BULLET_HIT_WALL,
	EV_BULLET_HIT_BREAKABLE,

	EV_UT_REDWINSROUND_SOUND,
	EV_UT_BLUEWINSROUND_SOUND,
	EV_UT_DRAWNROUND_SOUND,

	EV_JUMP_START,
	EV_JUMP_STOP,
	EV_JUMP_CANCEL,

	EV_UT_BOMBBEEP,
	EV_UT_BOMBBOUNCE,

	EV_MISSILE_HIT,
	EV_MISSILE_MISS,
	EV_MISSILE_MISS_METAL,
	EV_SHOTGUN,
	EV_BULLET,		/* otherEntity is the shooter */

	EV_PAIN,
	EV_DEATH1,
	EV_DEATH2,
	EV_DEATH3,
	EV_DEATH4,
	EV_DEATH5,
	EV_DEATH6,
	EV_DEATH7,
	EV_DEATH8,
	EV_OBITUARY,

	EV_GIB_PLAYER,		/* gib a previously living player */
	EV_SCOREPLUM,		/* score plum */

	EV_KAMIKAZE,		/* kamikaze explodes */

	EV_POWERUP_QUAD,
	EV_POWERUP_BATTLESUIT,
	EV_POWERUP_REGEN,

	EV_DEBUG_LINE,
	EV_STOPLOOPINGSOUND,
	EV_TAUNT,
	EV_TAUNT_YES,
	EV_TAUNT_NO,
	EV_TAUNT_FOLLOWME,
	EV_TAUNT_GETFLAG,
	EV_TAUNT_GUARDBASE,
	EV_TAUNT_PATROL,

	EV_ITEM_RESPAWN,
	EV_PLAYER_TELEPORT_IN,
	EV_PLAYER_TELEPORT_OUT,

	EV_GRENADE_BOUNCE,	/* eventParm will be the soundindex */

	EV_JUMP,
	EV_WATER_TOUCH, 	/* foot touches */
	EV_WATER_TOUCH_FAST,/* foot touches */
	EV_WATER_LEAVE, 	/* foot leaves */
	EV_WATER_UNDER, 	/* head touches */
	EV_WATER_CLEAR, 	/* head leaves */

	EV_ITEM_PICKUP, 	/* normal item pickups are predictable */
	EV_GLOBAL_ITEM_PICKUP,	/* powerup / team sounds are broadcast to everyone */

	EV_UT_BOMBDEFUSE_START,
	EV_UT_BOMBDEFUSE_STOP,
	ET_UT_WALLSLIDE,
	ET_UT_WALLJUMP,
	EV_UT_GOOMBA,
	EV_UT_STARTRECORD, //@Barbatos

	EV_KNIFE_HIT_WALL,
	EV_UT_WEAPON_MODE,
	EV_UT_HEAL_OTHER,
	EV_UT_GRAB_LEDGE,
	EV_UT_BULLET_HIT_PROTECTED,

	EV_UT_SLAP_SOUND,	/* Specail Slapper */
	EV_UT_NUKE_SOUND,	/* Nuke airplane sound. */

	EV_UT_FADEOUT,		/* Fade to black */

	/* Freeze Tag — new in 4.3 */
	EV_UT_FREEZE,		/* player was frozen */
	EV_UT_THAW,			/* player was thawed */
	EV_UT_MELT,			/* frozen player melted (died) */

} entity_event_t;

typedef enum {
	GTS_RED_CAPTURE,
	GTS_BLUE_CAPTURE,
	GTS_RED_RETURN,
	GTS_BLUE_RETURN,
	GTS_RED_TAKEN,
	GTS_BLUE_TAKEN,
	GTS_REDOBELISK_ATTACKED,
	GTS_BLUEOBELISK_ATTACKED,
	GTS_REDTEAM_SCORED,
	GTS_BLUETEAM_SCORED,
	GTS_REDTEAM_TOOK_LEAD,
	GTS_BLUETEAM_TOOK_LEAD,
	GTS_TEAMS_ARE_TIED,
	GTS_KAMIKAZE
} global_team_sound_t;

typedef struct animationSound_s {
	int 	 frame;
	sfxHandle_t  sound;
} animationSound_t;

typedef enum {
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_SPECTATOR,

	TEAM_NUM_TEAMS
} team_t;

/* Time between location updates */
#define TEAM_LOCATION_UPDATE_TIME 1000

/* How many players on the overlay */
#define TEAM_MAXOVERLAY 32

/*team task */
typedef enum {
	TEAMTASK_NONE,
	TEAMTASK_OFFENSE,
	TEAMTASK_DEFENSE,
	TEAMTASK_PATROL,
	TEAMTASK_FOLLOW,
	TEAMTASK_RETRIEVE,
	TEAMTASK_ESCORT,
	TEAMTASK_CAMP
} teamtask_t;

/* means of death */
typedef enum {
	MOD_UNKNOWN,

	MOD_WATER,
	MOD_SLIME,
	MOD_LAVA,
	MOD_CRUSH,
	MOD_TELEFRAG,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TARGET_LASER,
	MOD_TRIGGER_HURT,
	MOD_CHANGE_TEAM,

	UT_MOD_WEAPON,

	UT_MOD_KNIFE,
	UT_MOD_KNIFE_THROWN,
	UT_MOD_BERETTA,
	UT_MOD_DEAGLE,
	UT_MOD_SPAS,
	UT_MOD_UMP45,
	UT_MOD_MP5K,
	UT_MOD_LR,
	UT_MOD_G36,
	UT_MOD_PSG1,
	UT_MOD_HK69,
	UT_MOD_BLED,
	UT_MOD_KICKED,
	UT_MOD_HEGRENADE,
	UT_MOD_FLASHGRENADE,
	UT_MOD_SMOKEGRENADE,
	UT_MOD_SR8,
	UT_MOD_SACRIFICE,
	UT_MOD_AK103,
	UT_MOD_SPLODED,
	UT_MOD_SLAPPED, /* Slapped! */
	UT_MOD_SMITED, //@Barbatos
	UT_MOD_BOMBED,
	UT_MOD_NUKED,
	UT_MOD_NEGEV,
	UT_MOD_HK69_HIT,
	UT_MOD_M4,
	UT_MOD_GLOCK,
	UT_MOD_COLT1911,
	UT_MOD_MAC11,
	UT_MOD_FLAG,
	UT_MOD_GOOMBA,
	/*UT_MOD_P90,
	UT_MOD_BENELLI,
	UT_MOD_MAGNUM,*/
} meansOfDeath_t;

/* gitem_t->type */
typedef enum {
	IT_BAD,
	IT_WEAPON,	  /* EFX: rotate + upscale + minlight */
	IT_AMMO,	  /* EFX: rotate */
	IT_ARMOR,	  /* EFX: rotate + minlight */
	IT_HEALTH,	  /* EFX: static external sphere + rotating internal */
	IT_POWERUP,   /* instant on, timer based */
	/* EFX: rotate + external ring that rotates */
	IT_HOLDABLE,	  /* single use, holdable item */
	/* EFX: rotate + bob */
	IT_PERSISTANT_POWERUP,
	IT_TEAM
} itemType_t;

typedef enum {
	UT_ITEM_NONE,

	UT_ITEM_REDFLAG,
	UT_ITEM_BLUEFLAG,
	UT_ITEM_NEUTRALFLAG,

	UT_ITEM_KNIFE,		   // E
	UT_ITEM_BERETTA,	   // F
	UT_ITEM_DEAGLE, 	   // G
	UT_ITEM_SPAS12, 	   // H
	UT_ITEM_MP5K,		   // I
	UT_ITEM_UMP45,		   // J
	UT_ITEM_HK69,		   // K
	UT_ITEM_LR, 		   // L
	UT_ITEM_G36,		   // M
	UT_ITEM_PSG1,		   // N

	UT_ITEM_GRENADE_HE,    // O
	UT_ITEM_GRENADE_FLASH,
	UT_ITEM_GRENADE_SMOKE, // Q

	UT_ITEM_VEST,		   // R
	UT_ITEM_NVG,		   // S
	UT_ITEM_MEDKIT, 	   // T
	UT_ITEM_SILENCER,	   // U
	UT_ITEM_LASER,		   // V
	UT_ITEM_HELMET, 	   // W
	UT_ITEM_AMMO,		   // X
	UT_ITEM_APR,		   // Y

	UT_ITEM_SR8,		   // Z

	UT_ITEM_AK103,		   // a
	UT_ITEM_BOMB,		   // b
	UT_ITEM_NEGEV,		   // c
	UT_ITEM_GRENADE_FRAG,  // d
	UT_ITEM_M4, 		   // e
	UT_ITEM_GLOCK,		   // f
	UT_ITEM_COLT1911,      // g
	UT_ITEM_MAC11,         // h

	/*UT_ITEM_P90,		   // h
	UT_ITEM_BENELLI,	   // i
	UT_ITEM_MAGNUM, 	   // j

	// Dualies
	UT_ITEM_DUAL_BERETTA,  // k
	UT_ITEM_DUAL_DEAGLE,   // l
	UT_ITEM_DUAL_GLOCK,    // m
	UT_ITEM_DUAL_MAGNUM,   // n */

	UT_ITEM_MAX,

	UT_ITEM_C4,
} utItemID_t;

#define MAX_ITEM_MODELS 4

typedef qboolean(*PFNUTTURNITEMON)(playerState_t *);

typedef struct gitem_s {
	/* spawning name */
	char		*classname;
	char		*pickup_sound;
	char		*world_model[MAX_ITEM_MODELS];

	char		*icon;
	char		*icon2;
	/* for printing on pickup */
	char		*pickup_name;

	/* for ammo how much, or duration of powerup */
	int 		quantity;
	/* IT_* flags */
	itemType_t	giType;

	int 		giTag;

	/* string of all models and images this item will use */
	char		*precaches;
	/* string of all sounds this item will use */
	char		*sounds;

	int 		powerup;

} gitem_t;

/* included in both the game dll and the client */
extern gitem_t	bg_itemlist[];
extern int	bg_numItems;

#include "../common/c_weapons.h"

gitem_t *BG_FindItem(const char *pickupName);
gitem_t *BG_FindItemForClassname ( const char *classname );
gitem_t *BG_FindItemForWeapon(weapon_t weapon);
gitem_t *BG_FindItemForPowerup(powerup_t pw);
gitem_t *BG_FindItemForHoldable(holdable_t pw);
#define ITEM_INDEX(x) ((x) - bg_itemlist)

qboolean BG_CanItemBeGrabbed(int gametype, const entityState_t *ent,
				 const playerState_t *ps);

/* g_dmflags->integer flags */
#define DF_NO_FALLING	8
#define DF_FIXED_FOV	16
#define DF_NO_FOOTSTEPS 32

/* content masks */
#define MASK_ALL	 (-1)
#define MASK_SOLID	 (CONTENTS_SOLID)
#define MASK_PLAYERSOLID (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)
#define MASK_DEADSOLID	 (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)
#define MASK_WATER	 (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define MASK_OPAQUE  (CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA)
#define MASK_SHOT	 (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE)

/* */
/* entityState_t->eType */
/* */
typedef enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER,
	ET_CORPSE,
	ET_MR_SENTRY,

	ET_JUMP_START,
	ET_JUMP_STOP,
	ET_JUMP_CANCEL,

	ET_PLAYERBB,
	ET_PORTAL,
	ET_SPEAKER,
	ET_PUSH_TRIGGER,
	ET_TELEPORT_TRIGGER,
	ET_INVISIBLE,
	ET_TEAM,
	ET_HUDARROW,
	/* Bomb game mode */
	ET_BOMBSITE,
	/* any of the EV_* events can be added freestanding */
	ET_EVENTS
	/* by setting eType to ET_EVENTS + eventNum */
	/* this avoids having to set eFlags and eventNum */
} entityType_t;

void	 BG_EvaluateTrajectory(const trajectory_t *tr, int atTime,
				   vec3_t result);
void	 BG_EvaluateTrajectoryDelta(const trajectory_t *tr, int atTime,
					vec3_t result);

void	 BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm,
		playerState_t *ps);

void	 BG_PlayerStateToEntityState(playerState_t *ps, entityState_t *s,
					 qboolean snap);
void	 BG_PlayerStateToEntityStateExtraPolate(playerState_t *ps,
		entityState_t *s, int time, qboolean snap);

qboolean BG_PlayerTouchesItem(playerState_t *ps, entityState_t *item,
				  int atTime);

/*
 * Urban Terror (Dokta8)
 * angledata stores values that CGame needs to place
 * values in cent->pe.torso and .legs; used only
 * by BG_PlayerAngles
 */
typedef struct {
	vec3_t	legAngles;
	vec3_t	torsoAngles;
	vec3_t	headAngles;
} angleData_t;

void BG_PlayerAngles(vec3_t viewAngles,
			 vec3_t pVelocity,
			 int plegsAnim,
			 int ptorsoAnim,
			 int frametime,
			 angleData_t *adata,
			 float *legsPitchAngle,
			 float *legsYawAngle,
			 float *torsoPitchAngle,
			 float *torsoYawAngle,
			 int *pryFlags);

void BG_SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed, float *angle, qboolean *swinging, int frametime);

#define ARENAS_PER_TIER 	4
#define MAX_ARENAS			1024
#define MAX_ARENAS_TEXT 	8192

#define MAX_BOTS			1024
#define MAX_BOTS_TEXT		8192

typedef struct gradiomessage_s {
	char	   *textMale;
	char	   *textFemale;
	char	   *file;
	qhandle_t  sound[2];
} gradiomessage_t;

#define UT_HLFLAG_CRITICAL	   (1 << 0)
#define UT_HLFLAG_LIMP		   (1 << 1)
#define UT_HLFLAG_KEVLAR	   (1 << 2)
#define UT_HLFLAG_HELMET	   (1 << 3)

extern char 	   RadioTextList[9][10][40];
extern char 	   bg_handSkins[12][32];

extern float		   trGravity;	   /* hack for trajectory gravity */

extern unsigned int ut_weapon_xor;

#define UT_WEAPON_GETID(x)          (((x)^ut_weapon_xor) & 0x000000FF)
#define UT_WEAPON_GETBULLETS(x)    ((((x)^ut_weapon_xor) & 0x0000FF00) >> 8)
#define UT_WEAPON_GETMODE(x)       ((((x)^ut_weapon_xor) & 0x00FF0000) >> 16)
#define UT_WEAPON_GETCLIPS(x)      ((((x)^ut_weapon_xor) & 0xFF000000) >> 24)

#define UT_WEAPON_SETID(x, y)      ((x) = (((x) & 0xFFFFFF00) | (0x000000FF &  ((y)       ^ut_weapon_xor)) ))
#define UT_WEAPON_SETBULLETS(x, y) ((x) = (((x) & 0xFFFF00FF) | (0x0000FF00 & (((y) << 8 )^ut_weapon_xor)) ))
#define UT_WEAPON_SETMODE(x, y)    ((x) = (((x) & 0xFF00FFFF) | (0x00FF0000 & (((y) << 16)^ut_weapon_xor)) ))
#define UT_WEAPON_SETCLIPS(x, y)   ((x) = (((x) & 0x00FFFFFF) | (0xFF000000 & (((y) << 24)^ut_weapon_xor)) ))

#define UT_ITEM_GETID(x)		   (x & 0x00FF)
#define UT_ITEM_GETFLAGS(x) 	   ((x & 0xFF00) >> 8)

#define UT_ITEM_SETID(x, y) 	   (x = ((x & 0xFF00) + (0x00FF & (y))))
#define UT_ITEM_SETFLAGS(x, y)	   (x = ((x & 0x00FF) + ((0x00FF & (y)) << 8)))

#define UT_PAUSE_ON 0x01
#define UT_PAUSE_TR 0x02
#define UT_PAUSE_AC 0x04		/* Admin called. */
#define UT_PAUSE_RC 0x08		/* Red called. */
#define UT_PAUSE_BC 0x10		/* Blue called. */

#define UT_MATCH_ON 0x01		/* Match mode on. */
#define UT_MATCH_RR 0x02		/* Red ready. */
#define UT_MATCH_BR 0x04		/* Blue ready. */


#define 	utPSGetWeaponID(p)		   UT_WEAPON_GETID(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot])
#define 	utPSGetWeaponClips(p)	   UT_WEAPON_GETCLIPS(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot])
#define 	utPSGetWeaponBullets(p)    UT_WEAPON_GETBULLETS(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot])
#define 	utPSGetWeaponMode(p)	   UT_WEAPON_GETMODE(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot])

#define 	utPSSetWeaponID(p, i)	   UT_WEAPON_SETID(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot], (i))
#define 	utPSSetWeaponClips(p, c)   UT_WEAPON_SETCLIPS(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot], (c))
#define 	utPSSetWeaponBullets(p, b) UT_WEAPON_SETBULLETS(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot], (b))
#define 	utPSSetWeaponMode(p, m)    UT_WEAPON_SETMODE(((playerState_t *)p)->weaponData[((playerState_t *)p)->weaponslot], (m))

int   utPSGiveWeapon(playerState_t *, weapon_t, int mode);
int   utPSGetWeaponByID(const playerState_t *, weapon_t);
int   utPSGiveItem(playerState_t *, utItemID_t);
int   utPSGetItemByID(const playerState_t *, utItemID_t);
qboolean  utPSHasItem(const playerState_t *, utItemID_t);
qboolean  utPSIsItemOn(const playerState_t *, utItemID_t);
void	  utPSTurnItemOn(playerState_t *, utItemID_t);

void BG_RandomInit(const char *serverinfo);

#include "../common/c_players.h"
#include "../common/c_arieshl.h"
typedef ariesHitLocation_t HitLocation_t;

#include "bg_gear.h"

#endif /* _BG_PUBLIC_H_ */
