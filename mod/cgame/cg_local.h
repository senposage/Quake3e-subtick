// Copyright (C) 1999-2000 Id Software, Inc.
//

#ifndef _CG_LOCAL_H_
#define _CG_LOCAL_H_

#include "../game/q_shared.h"
#include "tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"
#include "../ui/ui_shared.h"

// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#ifdef MISSIONPACK
 #define CG_FONT_THRESHOLD 0.1
#endif

#define POWERUP_BLINKS		5

#define POWERUP_BLINK_TIME	1000
#define FADE_TIME			200
#define PULSE_TIME			200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME	400
#define DAMAGE_TIME 		500
#define LAND_DEFLECT_TIME	150
#define LAND_RETURN_TIME	300
#define STEP_TIME			200
#define DUCK_TIME			200
#define PAIN_TWITCH_TIME	200
#define WEAPON_SELECT_TIME	1400
#define ITEM_SELECT_TIME	1400
#define ITEM_SCALEUP_TIME	1000
#define ZOOM_TIME			150
#define ITEM_BLOB_TIME		200
#define MUZZLE_FLASH_TIME	20
#define SINK_TIME			1000			// time for fragments to sink into ground before going away
#define ATTACKER_HEAD_TIME	10000
#define REWARD_TIME 		3000

#define PULSE_SCALE 		1.5 				// amount to scale up the icons when activating

#define MAX_STEP_CHANGE 	32

#define MAX_VERTS_ON_POLY	10
#define MAX_MARK_POLYS		256

#define STAT_MINUS			10			// num frame for '-' stats digit

#define ICON_SIZE			48
#define CHAR_WIDTH			32
#define CHAR_HEIGHT 		48
#define TEXT_ICON_SPACE 	4

#define TEAMCHAT_WIDTH		80
#define TEAMCHAT_HEIGHT 	8

// very large characters
#define GIANT_WIDTH 		32
#define GIANT_HEIGHT		48

#define TEAM_OVERLAY_MAXNAME_WIDTH	   12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH 16

#define DEFAULT_MODEL			   "athena"
#define DEFAULT_SKIN			   "desert_w"

/* URBAN TERROR - we dont have james
#ifdef MISSIONPACK
#define DEFAULT_TEAM_MODEL		"james"
#define DEFAULT_TEAM_HEAD		"*james"
#else
#define DEFAULT_TEAM_MODEL		"sarge"
#define DEFAULT_TEAM_HEAD		"sarge"
#endif
*/

#define DEFAULT_RACE		  "2"

#define DEFAULT_REDTEAM_NAME  "Red"
#define DEFAULT_BLUETEAM_NAME "Blue"

#define CHAT_WIDTH		  80
#define CHAT_HEIGHT 	  8

#define CHATCHAR_WIDTH		  7
#define CHATCHAR_HEIGHT 	  11

#define MSG_WIDTH		  80
#define MSG_HEIGHT		  8

#define MSG_CHAR_WIDTH		  7
#define MSG_CHAR_HEIGHT 	  11

#define UT_FLASH_MAX_TIME	  15000 	/* D8 2.4: upped from 8000 */
#define UT_FLASH_MAX_DISTANCE 1000

#define UT_MAX_SHARDITER	  5
#define UT_MAX_SHARDS		  (1 << (UT_MAX_SHARDITER + 1))

typedef enum
{
	FOOTSTEP_NORMAL,
	FOOTSTEP_BOOT,
	FOOTSTEP_FLESH,
	FOOTSTEP_MECH,
	FOOTSTEP_ENERGY,
	FOOTSTEP_METAL,
	FOOTSTEP_SPLASH,

	FOOTSTEP_TOTAL
} footstep_t;

typedef enum
{
	IMPACTSOUND_DEFAULT,
	IMPACTSOUND_METAL,
	IMPACTSOUND_FLESH
} impactSound_t;

// Urban Terror

typedef enum
{
	UT_SKIN_GREEN,
	UT_SKIN_BLUE,
	UT_SKIN_RED,
	UT_SKIN_PURPLE,
	UT_SKIN_ORANGE,
	UT_SKIN_OLIVE,
	UT_SKIN_WHITE,
	UT_SKIN_BLACK,
	UT_SKIN_DESERT,
	UT_SKIN_COWBOY,
	UT_SKIN_CAVALRY,
  UT_SKIN_DROOGS,
} utSkins_t;

#define UT_SKIN_COUNT UT_SKIN_DROOGS+1

typedef enum
{
	UT_OCFADE_LINEAR_IN,
	UT_OCFADE_LINEAR_OUT,
	UT_OCFADE_SIN_OUT,
} utOverlayColorFade_t;

#define MAX_TRAIL 32

typedef struct
{
	vec3_t	pos;
	vec3_t	top, bottom;
	vec4_t	colour;
	int time;
	int link;
	int isolated;
} trailp_t;

typedef struct
{
	trailp_t  trails[MAX_TRAIL];
	int   lasttime;
	int   head;
	int   trailing;
} trail_t;

typedef struct
{
	vec3_t		 laserpos;
	lerpFrame_t  torso;
	lerpFrame_t  legs;
	lerpFrame_t  view;
	int 	 painTime;
	int 	 painDirection; 		// flip from 0 to 1
	angleData_t  adata; 			// not only used for aries debug

	trail_t 	 trail;
} playerEntity_t;

//==================================================

#define INV_INVCOLS  11 		// how many invaders in a column
#define INV_INVROWS  5			// how many invaders in a row

#define INV_WIDTH	 24 		// width of an invader
#define INV_HEIGHT	 16 		// height of an invader
#define INV_PWIDTH	 24 		// player's width
#define INV_PHEIGHT  16 		// player's height

#define INV_XSPACING 8
#define INV_YSPACING 16 	// was 14 in original game

#define INV_HSTEP	 16 		// how many pixels they move to the right each turn
#define INV_VSTEP	 16 		// how many pixels they move down

#define INV_STARTX	 110		// right side of playfield
#define INV_STARTY	 120		// where invaders start
#define INV_SCORE1Y  16 		// SCORE   HI SCORE   SCORE2
#define INV_SCORE2Y  48 		//	0000	  0000	0000

#define INV_MAXX	 (INV_STARTX + (INV_INVCOLS + 4) * (INV_WIDTH + INV_XSPACING))
#define INV_MAXY	 (INV_STARTY + (INV_INVROWS + 2) * (INV_HEIGHT + INV_YSPACING))
#define INV_PENDX	 INV_MAXX

#define INV_BHEIGHT  30 		// base height
#define INV_BLEFTOFF 60
#define INV_BSPACING 42
#define INV_BWIDTH	 42

#define INV_LINE	 (480 - 32)

#define INV_PSHIPY	 (INV_LINE - 32 - INV_PHEIGHT)		// upper left pixel of player ship

#define INV_BASEY	 (INV_PSHIPY - 32 - INV_BHEIGHT)

#define INV_MAXSHOTS 4

// data for space invader game held in cg_t
typedef struct invadergame_s
{
	qboolean  playing;
	int   lastinvmove;
	int   pausetimer;
	int   speed;
	int   wave;

	int   invaders[INV_INVCOLS][INV_INVROWS];
	int   numInvaders;
	int   icol;
	int   irow;
	int   minx;
	int   maxx;
	int   miny;
	int   maxy;
	int   direction;
	int   invadershoty[INV_MAXSHOTS];
	int   invadershotx[INV_MAXSHOTS];
	int   numActiveInvaderShots;

	int   playerx;
	int   playery;
	int   playerdx;
	int   playershoot;
	int   shotx;
	int   shoty;
	int   playerlives;

	int   score;
	int   barriers[INV_BWIDTH][4][2];

	char	  hinames[32][10];
	int   hiscores[10];
} invadergame_t;

////////// BREAKOUT //////////

#define BREAKOUT_BRICK_COLUMNS 10
#define BREAKOUT_BRICK_ROWS    10

typedef struct paddle_s
{
	float  x;
	float  v;
} paddle_t;

typedef struct ball_s
{
	float  x;
	float  y;
	float  v[2];
} ball_t;

typedef struct breakout_s
{
	// State.
	qboolean  initialized;
	qboolean  play;
	qboolean  reset;
	qboolean  pause;
	qboolean  gameover;

	// Time.
	int  time;
	int  delay;

	// Status.
	int  level;
	int  lives;

	// Bricks.
	int  bricks[BREAKOUT_BRICK_ROWS][BREAKOUT_BRICK_COLUMNS];
	int  bricksOut;

	// Paddle.
	paddle_t  paddle;

	// Ball.
	ball_t	ball;
} breakout_t;

/////////// SNAKE ////////////

#define SNAKE_CELL_COLUMNS	 35
#define SNAKE_CELL_ROWS 	 35
#define SNAKE_MAX_BODY_PARTS 30

typedef struct body_s
{
	int  s;
	int  x;
	int  y;
	int  v[2];
} body_t;

typedef struct snake_s
{
	// State.
	qboolean  initialized;
	qboolean  play;
	qboolean  reset;
	qboolean  pause;
	qboolean  gameover;

	// Time.
	int  time;
	int  delay;

	// Status.
	int  level;
	int  lives;
	int  apples;

	// Cells.
	int  cells[SNAKE_CELL_ROWS][SNAKE_CELL_COLUMNS];

	// Body.
	body_t	body[SNAKE_MAX_BODY_PARTS];
	int parts;
} snake_t;

//==================================================

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
//@r00t: This struct is usually hooked by hacks, so I reshuffled variables around
//previousEvent and currentState.event encrypted using CSE_ENC(), decrypt using CSE_DEC()
typedef struct centity_s
{
	int 	previousEvent;
	int 	miscTime;

	playerEntity_t	pe;

	vec3_t	lerpAngles;

	char	interpolate;		// true if next is valid to interpolate to
	char	currentValid;		// true if cg.frame holds this entity

	//struct centity_s *real_ptr;  // r00t: cg_entities[ent].real_ptr-><actual entitity> indirect addressing

	int snapShotTime;

	entityState_t	currentState;		// from cg.frame

	entityState_t	nextState;		// from cg.nextFrame, if available

	// exact interpolated position of entity on this frame
	vec3_t	lerpOrigin;

	// WARNING, this is a very expensive structure to add stuff too.
	// Please make sure the addition is necessary before doing so.
} centity_t;

//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independantly from all server transmitted entities

typedef struct markPoly_s
{
	struct markPoly_s  *prevMark, *nextMark;
	int 			   time;
	qhandle_t		   markShader;
	qboolean		   alphaFade;		// fade alpha instead of rgb
	float			   color[4];
	poly_t			   poly;
	polyVert_t		   verts[MAX_VERTS_ON_POLY];
	centity_t		   *parent; 	// added so that marks can be associated with entities
} markPoly_t;

typedef enum
{
	LE_MARK,
	LE_EXPLOSION,
	LE_SPRITE_EXPLOSION,
	LE_FRAGMENT,
	LE_MOVE_SCALE_FADE,
	LE_FALL_SCALE_FADE,
	LE_FADE_RGB,
	LE_SCALE_FADE,
	LE_SCOREPLUM,

	// Urban Terror
	LE_EMITTER
} leType_t;

typedef enum
{
	LEF_PUFF_DONT_SCALE = 0x00001,					// do not scale size over time
	LEF_TUMBLE			= 0x00002,					// tumble over time, used for ejecting shells
	LEF_SOUND1			= 0x00004,					// sound 1 for kamikaze
	LEF_SOUND2			= 0x00008,					// sound 2 for kamikaze
	LEF_PART_TRAIL		= 0x00010,					// particle leaves a trail
	LEF_PART_BOUNCE 	= 0x00020,					// particle bounces
	LEF_PARTICLE		= 0x00040,
	LEF_SHARD			= 0x00080,					// used as a shard from a breakable
	LEF_DIEONIMPACT 	= 0x00100,					// used for "emmiter" style ents
	LEF_DONTRENDER		= 0x00200,					// used for "emmiter" style ents
	LEF_ALLOWOVERDRAW	= 0x00400,					// smoke grens use this
	LEF_CLIP			= 0x00800,					// forces a particle to not intersect the bsp.	Done with a single trace at spawn
	LEF_SHOWLASER		= 0x01000,					// particles will fuzz in the laser.
	LEF_BLOODTRAIL		= 0x02000					// particles will fuzz in the laser.
} leFlag_t;

typedef enum
{
	LEMT_NONE,
	LEMT_BURN,
	LEMT_BLOOD,
	LEMT_SMALLBLOOD
} leMarkType_t; 		// fragment local entities can leave marks on walls

typedef enum
{
	LEBS_NONE,
	LEBS_BLOOD,
	LEBS_BRASS,

	// Urban Terror
	LEBS_GLASS,
	LEBS_WOOD,
	LEBS_CERAMIC,
	LEBS_PLASTIC
} leBounceSoundType_t;	// fragment local entities can make sounds on impacts

#define PART_FLAG_MARK		(1 << 0)
#define PART_FLAG_BOUNCE	(1 << 1)
#define PART_FLAG_FADE		(1 << 2)
#define PART_FLAG_TRAIL 	(1 << 3)
#define PART_FLAG_DIEONSTOP (1 << 4)
#define PART_FLAG_FLOATS	(1 << 5)
#define PART_FLAG_INVISIBLE (1 << 6)

// Urban Terror
// Particles are implemented as members of a localentity, so data such as duration, etc
// is stored in the particle's parent le.
//
//	* a particle with a model will be rendered as a ref entity
//	* a particle with a trail set to true will be rendered as a poly
//	  and "smeared" along its trajectory (sparks)
//	* a particle with no trail and no model will be rendered as a square
//	  poly oriented towards the viewer, kind of like snow
//	* a model will always override a trail

typedef struct particle_smoke_s
{
	float	chance; 			// percent chance that trail will happen
	int interval;			// duration / interval = number of puffs
	float	percent;			// 0.5 means will only persist for half the part's duration
	float	floatrate;			// float up (+tve) or down (-tve) 15 is a good number
	float	thickness;			// initial radius of smoke puffs
	vec4_t	color;				// smoke color
} particle_smoke_t;

/*typedef struct particleRender_s {

	//qhandle_t shader;
	float		color[4];
	float		radius; 	// if set to 0 on part->mark will use particle radius instead

} particleRender_t;*/

typedef struct particle_s
{
	int 		  flags;		// PART_FLAG_

	qboolean	  trail;		// if true, smear sprite out over trajectory (eg: sparks)
	qboolean	  bounce;		// bounce instead of stopping when hitting things
	qboolean	  fade;

	int 		  trType;		// trajectory type
	float		  lightness;	// for trtype of TR_GRAVITY2
	int 		  timetolive;	// how long particles lives for in ms
	float		  deviation;	// deviation from start time

	float		  minRadius;	// minimum radius of particle
	float		  maxRadius;	// max radius of particle (min == max for no randomness)

	float		  minVelocity;	// minimum velocity of particle ( used for CG_ParticleExplode )
	float		  maxVelocity;	// maximum velocity of particle

	float		  up;			// added to velocity[2] in CG_ParticleLaunch

	float		  markradius;	// radius of mark left (flag of MARK) use 0 to auto set to part radius
	vec4_t		  markcolor;	// mark colour
	vec4_t		  partcolor;	// particle colour

	particle_smoke_t  smoke;	// smoke trail setting for PART_FLAG_TRAIL
} particle_t;

typedef enum
{
	PART_ATTR_NONE,
	PART_ATTR_SPARK,
	PART_ATTR_BLOOD,
	PART_ATTR_BLOODTRICKLE,
	PART_ATTR_BLOODTRICKLE_INVIS,
	PART_ATTR_STONE,
	PART_ATTR_WOOD,
	PART_ATTR_SPLASH,
	PART_ATTR_EXPLOSION,
	PART_ATTR_EXPLOSION_CHUNKS,
	PART_ATTR_DIRTKICKUP,
	PART_ATTR_GRAVEL,
	PART_ATTR_GRASS,
	PART_ATTR_SNOW,
	PART_ATTR_GLASS,
	PART_ATTR_MAX
} part_attr_t;

// emitter: holds data used to construct a particle emitter
// like particles, an emitter always has a parent local entity for
// holding trajectory data: this emitter only stores details that the
// emitter needs to know.  le->part should hold data about the
// particles emitted by an emitter.

typedef struct emitter_s
{
	vec3_t	velocity;	// initial velocity to impart to spawned particles

	float	deviation;	// between 0 and 1, determines randomness of particle emission

	int current_particle;		// the present particle ready for launch
	int total_particles;		// number of particles to emit over lifetime
	int nextEmitTime;			// cg.time at which next particle should be emitted
} emitter_t;

// precipitation stuff
typedef struct precip_s
{
	int 	  timeOffset;
	vec3_t		  endpos;
	trajectory_t  tr;
} precip_t;

// end Urban Terror

typedef struct localEntity_s
{
	int 		  startTime;
	int 		  endTime;
	int 		  fadeInTime;

	struct localEntity_s  *prev, *next;
	leType_t		  leType;
	int 		  leFlags;


	float			  lifeRate; 			// 1.0 / (endTime - startTime)

	trajectory_t	  pos;
	trajectory_t	  angles;

	float			  bounceFactor; 	// 0.0 = no bounce, 1.0 = perfect
	qhandle_t		  bounceSound;

	float			  color[4];

	float			  radius;

	float			  light;
	vec3_t			  lightColor;

	leMarkType_t		  leMarkType;		// mark to leave on fragment impact
	leBounceSoundType_t   leBounceSoundType;

	refEntity_t 	  refEntity;

	// Urban Terror
	float	   gravity; 				// for TR_GRAVITY2
	//particle_t		part;			// for holding particle data
	emitter_t  emit;					// for holding emitter data
	vec3_t	   scale;
	int 	   partType;				// index into p_attr[] in cg_surfaces.c: enum part_attr_t
	int 	   partMarkShader;
	int 	   smoke;
	int 	   timeTilNextChild;

	vec3_t	   triverts[3];
} localEntity_t;

//======================================================================

typedef struct {
	int  client;
	int  score;
	int  ping;
	int  time;
	int  team;
	int  deaths;
	int  jumpWay;		//@r00t:JumpWayColors
	int  jumpBestWay;	//@r00t:JumpWayColors
	int  jumpBestTime;	//@Fenix
	char account[36];	//@Barbatos
} score_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define MAX_CUSTOM_SOUNDS 32

//@r00t: This struct is usually hooked by hacks, so I reshuffled variables around
typedef struct
{
	qhandle_t	   legsModel;
	qhandle_t	   legsSkin;

	int 		   score;					// updated by score servercmds
	int 		   deaths;
	int 		   location;				// location index for team mode
	int 		   health;					// you only get this info about your teammates
	int 		   armor;
	int 		   curWeapon;
	int 		   botSkill;				// 0 = not bot, 1-5 = bot
	int 		   handicap;
	int 		   wins, losses;			// in tourney mode

	qhandle_t	   vesttorsoModel;
	qhandle_t	   vesttorsoSkin;

	qhandle_t	   torsoModel;
	qhandle_t	   torsoSkin;

	qhandle_t	   headDamageSkin;
	qhandle_t	   torsoDamageSkin[6];
	qhandle_t	   legsDamageSkin[3];

	int 		   jumpStartTime;						 //@Barba: when the player touches the start line
	int 		   jumpBestTime;						 //@Barba: teh best time we did!
	int 		   jumpWay; 							 //@r00t: Current way we are on
	int 		   jumpBestWay; 						 //@r00t: Way for best time
	int 		   jumpLastEstablishedTime; 			 //@Fenix: to track last time recorded
	qboolean	   jumprun; 							 //@Fenix: match /ready status in jump mode

	//@Barbatos: auth system
	#ifdef USE_AUTH
	char	  auth_login[36];
	#endif

	vec3_t		   color;

	int 		   powerups;				// so can display quad/flag status

	int 		   breathPuffTime;
	int 		   breathPuffType;

	sfxHandle_t    sounds[MAX_CUSTOM_SOUNDS];

	int 		   ariesModel;

	int 		   ejectTime;
	int 		   muzzleFlashTime;
	int 		   muzzleFlashAngle;
	int 		   muzzleFlashHand;
	int 		   weaponsmoketime;

	// Sound origin scrambling.
	int 		   lastEventTime;

	// For corpse gibbing.
	qboolean	   gibbed;



	qhandle_t	   headModel;
	qhandle_t	   headSkin;

	qhandle_t	   vestModel;
	qhandle_t	   vestSkin;
	qhandle_t	   vestBlueLeaderSkin;
	qhandle_t	   vestRedLeaderSkin;

	team_t		   team;

	int 		   medkitUsageTime;
	int 		   invulnerabilityStartTime;
	int 		   invulnerabilityStopTime;

	qhandle_t	   nvgModel;
	qhandle_t	   nvgSkin;

	char		   name[MAX_QPATH];
	footstep_t	   footsteps;
	gender_t	   gender;
	int 		   skincolor; // 0 white, 1 black
	int 		   race;	  //0,1,2,3
	char		   funstuff[3][13];
	qhandle_t	   funmodel[3];
	int 		   funmodeltag[3];
	vec3_t		   armbandRGB;


	qhandle_t	   helmetModel;
	qhandle_t	   helmetSkin;
	qhandle_t	   helmetLeaderSkins[UT_SKIN_COUNT];

	qboolean	   ghost;			// true if client is a ghost
	qboolean	   substitute;		// Only changed when checking for scores.
	qboolean	   infoValid;
	qboolean	   nvgVisible;
	qboolean	   hasFlag;
	qboolean	   hasBomb;

	animation_t    animations[MAX_PLAYER_ANIMATIONS];

	qboolean	   ejectBrass;
	int 		   timeofdeath;
	int 		   bleedCount;
	int 		   bleedTime[10];
	int 		   bleedTriangle[10];
	HitLocation_t  bleedHitLoc[10];

	int 		   teamTask;				// task in teamplay (offence/defence)
	qboolean	   teamLeader;				// true when this is a team leader

} clientInfo_t;

#define MAX_PLAYERSKINS  8

#define UT_WPANIMSND_MAX 20

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s
{
	qhandle_t	 handsModel;				// the hands don't actually draw, they just position the weapon
	qhandle_t	 weaponModel;
	qhandle_t	 barrelModel;
	qhandle_t	 flashModel;

	qboolean	 registered;
	gitem_t 	 *item;

	vec3_t		 weaponMidpoint;			// so it will rotate centered instead of by tag

	float		 flashDlight;
	vec3_t		 flashDlightColor;
	sfxHandle_t  flashSound[4]; 		// fast firing weapons randomly choose

	qhandle_t	 weaponIcon;
	qhandle_t	 ammoIcon;

	qhandle_t	 ammoModel;

	qhandle_t	 missileModel;
	sfxHandle_t  missileSound;
	void (*missileTrailFunc)( centity_t *, const struct weaponInfo_s *wi );
	float		 missileDlight;
	vec3_t		 missileDlightColor;
	int 		 missileRenderfx;

	void (*ejectBrassFunc)( centity_t *, refEntity_t *parent, float lengthScale, float diameterScale );

	float		 trailRadius;
	float		 wiTrailTime;

	sfxHandle_t  readySound;
	sfxHandle_t  firingSound;
	qboolean	 loopFireSound;

// urban terror

	qhandle_t	  viewModel;
	qhandle_t	  viewSkin;
	animation_t   viewAnims[MAX_WEAPON_ANIMATIONS];
	animationSound_t  viewSounds[UT_WPANIMSND_MAX];
	qhandle_t	  viewFlashModel;

	sfxHandle_t   sndFireOutside;
	sfxHandle_t   sndFireInside;
	sfxHandle_t   sndFireWater;
	sfxHandle_t   sndFireSilenced;

	sfxHandle_t   sndUnderWater;

	qhandle_t	  scopeShader[2];
} weaponInfo_t;

// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
typedef struct
{
	qhandle_t  icon;
	qhandle_t  icon2;
	vec3_t	   midpoint;				// so it will rotate centered instead of by tag
	vec3_t	   bottompoint;
	qboolean   registered;
	qhandle_t  models[MAX_ITEM_MODELS];
} itemInfo_t;

typedef struct
{
	int  itemNum;
} powerupInfo_t;

#define MAX_SKULLTRAIL 10

typedef struct
{
	vec3_t	positions[MAX_SKULLTRAIL];
	int numpositions;
} skulltrail_t;

#define MAX_REWARDSTACK 10
#define MAX_SOUNDBUFFER 20

//////////////////////////////////////////////////

#define MAX_BOMBPARTICLES 25

typedef struct bombParticle_s
{
	qboolean   active;
	qhandle_t  shader;
	vec4_t	   colour;
	vec3_t	   position;
	vec3_t	   velocity;
	vec3_t	   angles;
	vec3_t	   radius;
	int    fadeTime;
} bombParticle_t;

//////////////////////////////////////////////////

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS 16

typedef struct
{
	int  clientFrame;				// incremented each frame

	int  clientNum;

	//old fps counter
	int  q3fps;

	//True fps counter
	int   fpsnow;
	int   fpscounter;
	int   LastFpsPoll;

	qboolean  demoPlayback;
	qboolean  demoCast;
	qboolean  levelShot;				// taking a level menu screenshot
	int   deferredPlayerLoading;
	qboolean  loading;					// don't defer players at initial startup
	qboolean  intermissionStarted;		// don't play voice rewards, because game will end shortly

	// there are only one or two snapshot_t that are relevent at a time
	int 	latestSnapshotNum;			// the number of snapshots the client system has received
	int 	latestSnapshotTime; 		// the time from latestSnapshotNum, so we don't need to read the snapshot yet

	snapshot_t	*snap;					// cg.snap->serverTime <= cg.time
	snapshot_t	*nextSnap;				// cg.nextSnap->serverTime > cg.time, or NULL
	snapshot_t	activeSnapshots[2];

	float		frameInterpolation; 	// (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

	qboolean	thisFrameTeleport;
	qboolean	nextFrameTeleport;

	int 	framecount;
	int 	framesum;
	int 	frametime;				// cg.time - cg.oldTime
	int 	avgtime;

//	 int	 timechop;
	int 	time;				  // this is the time value that the client
	// is rendering at.
	int 	oldTime;			  // time at last frame, used for missile trails and prediction checking

	int 	physicsTime;		  // either cg.snap->time or cg.nextSnap->time

	qboolean  mapRestart;				// set on a map restart to set back the weapon

	qboolean  renderingThirdPerson; 		// during deaths, chasecams, etc

	// prediction state
	qboolean	   hyperspace;				// true if prediction has hit a trigger_teleport
	playerState_t  predictedPlayerState;
	centity_t	   predictedPlayerEntity;
	qboolean	   validPPS;				// clear until the first call to CG_PredictPlayerState
	int 		   predictedErrorTime;
	vec3_t		   predictedError;

//	vec3_t		predictedLaserOrigin;

	int 	  oldAngles[3];

	int 	  eventSequence;
	int 	  predictableEvents[MAX_PREDICTED_EVENTS];

	float	  stepChange;				   // for stair up smoothing
	int 	  stepTime;

	float	  duckChange;				   // for duck viewheight smoothing
	int 	  duckTime;

	float	  landChange;				   // for landing hard
	int 	  landTime;

	// input state sent to server
	int 	  weaponSelect;

	// auto rotating items
	vec3_t	  autoAngles;
	vec3_t	  autoAxis[3];
	vec3_t	  autoAnglesWeap;
	vec3_t	  autoAxisWeap[3];

	// view rendering
	refdef_t  refdef;
	vec3_t	  refdefViewAngles; 		// will be converted to refdef.viewaxis

	// zoom key
	int 	  zoomed;
	int 	  zoomedFrom;
	int 	  zoomedSaved;
	int 	  zoomTime;
	float	  zoomSensitivity;

	// information screen text during loading
	char	  infoScreenText[MAX_STRING_CHARS];
	int 	  percentLoaded;
	int 	  percentLoadedCounter;

	// scoreboard
	int 	  scoresRequestTime;
	int 	  numScores;
	int 	  selectedScore;
	int 	  teamScores[2];
	score_t   scores[MAX_CLIENTS];
	qboolean  showScores;
	qboolean  scoreBoardShowing;
	int 	  scoreFadeTime;
	char	  killerName[MAX_NAME_LENGTH];
	char	  spectatorList[MAX_STRING_CHARS];										// list of names
	int 	  spectatorLen; 														// length of list
	float	  spectatorWidth;														// width in device units
	int 	  spectatorTime;														// next time to offset
	int 	  spectatorPaintX;														// current paint x
	int 	  spectatorPaintX2; 													// current paint x
	int 	  spectatorOffset;														// current offset from start
	int 	  spectatorPaintLen;													// current offset from start

	// skull trails
	skulltrail_t  skulltrails[MAX_CLIENTS];

	// centerprinting
	int   centerPrintTime;
	int   centerPrintCharWidth;
	int   centerPrintY;
	char  centerPrint[1024];
	int   centerPrintLines;

	// low ammo warning state
	int  lowAmmoWarning;				// 1 = low, 2 = empty

	// kill timers for carnage reward
	int  lastKillTime;

	// crosshair client ID
	int crosshairClientNum;
	int crosshairClientTime;
	int crosshairColorClientNum;
	vec4_t	crosshairRGB;
	vec4_t	crosshairFriendRGB;
	vec4_t	scopeRGB;
	vec4_t	scopeFriendRGB;

	// powerup active flashing
	int  powerupActive;
	int  powerupTime;

	// attacking player
	int  attackerTime;
	int  voiceTime;

	// reward medals
	int    rewardStack;
	int    rewardTime;
	int    rewardCount[MAX_REWARDSTACK];
	qhandle_t  rewardShader[MAX_REWARDSTACK];
	qhandle_t  rewardSound[MAX_REWARDSTACK];

	// sound buffer mainly for announcer sounds
	int    soundBufferIn;
	int    soundBufferOut;
	int    soundTime;
	qhandle_t  soundBuffer[MAX_SOUNDBUFFER];

	// warmup countdown
	int  warmup;
	int  warmupCount;

	// Pause.
	int  pauseState;
	int  pauseTime;

	// Match.
	int  matchState;

	// Team names.
	char  teamNameRed[MAX_NAME_LENGTH];
	char  teamNameBlue[MAX_NAME_LENGTH];

	char  roundstat[MAX_NAME_LENGTH];

	//==========================

	int   itemPickup;
	int   itemPickupTime;
	int   itemPickupBlendTime;			// the pulse around the crosshair is timed seperately

	int   weaponSelectTime;
	int   weaponAnimation;
	int   weaponAnimationWeapon;
	qboolean  weaponHidden;
	int   weaponHideShowTime;
	int   weaponHideDir;

	int   weaponDropTime;
	int   weaponChangeTime;

	// blend blobs
	float  damageTime;
	float  damageX, damageY, damageValue;

	// status bar head
	float  headYaw;
	float  headEndPitch;
	float  headEndYaw;
	int    headEndTime;
	float  headStartPitch;
	float  headStartYaw;
	int    headStartTime;

	// view movement
	float	v_dmg_time;
	float	v_dmg_pitch;
	float	v_dmg_roll;

	vec3_t	kick_angles;		// weapon kicks
	vec3_t	kick_origin;

	// temp working variables for player view
	float  bobfracsin;
	int    bobcycle;
	float  xyspeed;
	int    nextOrbitTime;

	// development tool
	refEntity_t  testModelEntity;
	char		 testModelName[MAX_QPATH];
	qboolean	 testGun;

// Urban Terror
	int  damageIndicator;

	int  itemSelectTime;
	int  itemSelect;

	int  hitting;

	// Color overlay
	vec4_t	overlayColor;
	int overlayColorFade;
	int overlayColorFadeTime;
	int overlayColorDuration;
	int overlayColorTime;

	// Shader overlay
	qhandle_t	   overlayShader;
	int 	   overlayShaderDuration;
	int 	   overlayShaderTime;

	int 	   lastDeathTime;			// cg.time at which player last died

	qboolean	   playinvgame; 	// set to true to bring up invader game overlay
	invadergame_t  invadergame; 	// used for holding space invader data

	////////// BREAKOUT //////////
	breakout_t	breakout;

	/////////// SNAKE ////////////
	snake_t   snake;

	char	  survivorRoundWinner[100];
	int   survivorRoundStart;

	int   cahTime;

	int   respawnTime;

	qboolean  popupSelectTeamMenu;
	qboolean  popupSelectGearMenu;

	// HighSea: damn useless vars taking up var names.
	int  bombDefuseTime;
	int  bombPlantTime;

	//int			bombExplodeTime;
	int bombExplodeCount;
	vec3_t	bombExplodeOrigin;

	// For the minimap
	vec3_t	  redflagpos;
	vec3_t	  blueflagpos;
	vec3_t	  bombpos;
	qboolean  redflagvisible;
	qboolean  blueflagvisible;
	qboolean  bombvisible;

	int   bandageTime;

	char	  weaponModes[UT_WP_NUM_WEAPONS];
	qboolean  bleedingFrom[HL_MAX];
	int   bleedingFromCount;

	int   sniperGameRoundTime;			//GottaBeKD for SP event
	int   raceStartTime;				//GottaBeKD for SP event
	int   raceTimeDisplay;				//GottaBeKD to display msecs properly

	qboolean  reloading;
	qboolean  playingWaterSFX;
	float	  fps;
	vec4_t	  grensmokecolor;

	int   screenshaketime;

	//
	// DensitY - something for the spectator name/info prints
	//		TODO: better system!
	//
	int    NumPlayersSnap;			// Num of players in this snap, cleared each client frame
	short  NumBombSites;

	//
	// Owned Sound
	//
	qboolean  IsOwnedSound; 			 // Default false;
	int   OwnedType;					 // Type of ownage.. 1 = normal 2 = CONED
	int   OwnedSoundTimeStart;			 // cg.time + 2000 (2 seconds)
	int   RoundFadeStart;				 // cg.time + 2000 (2 seconds)

	// 27
	int  firstframeofintermission;

	// Hack for QVM
	float  overlayFade;

	//
	// BOMB Graphics code
	//
	qboolean  DefuseOverlay;
	int   DefuseStartTime;
	int   DefuseEndTime;
	int   DefuseLength;
	qboolean  BombDefused;		 // has the bomb been defused client side

	//
	// Tactical Goggles
	//

	int  nvgVisibleTime[MAX_GENTITIES];

	//
	// Bomb FX
	//

	vec3_t		bombPlantOrigin;
	qboolean	bombExploded;
	int 	bombExplodeTime;
	vec3_t		bombExplosionOrigin;
	float		bombExplosionRadius;
	bombParticle_t	bombParticles[MAX_BOMBPARTICLES];
	int 	nextbombShockwave;
	int 	shockwaveCount;

	//
	// Player prediction.
	//

	qboolean  sprintKey;
	int   lastPredictedNextSnapTime;

	int   lastPickupSound;

	int   lastPPStime;
	int   lastPPScount;
	int   ppsCount;

	int   lastSPStime;
	int   lastSPScount;
	int   spsCount;

	int   lastYawPitchCheck;

	int   radiomenu;
	int   RadioKeyHeld[10]; //Incremented every frame it is held, so we can catch repeating keys

	int   speedxy;
	int   speedxyz;

	int   pausedTime;

	int   g_armbands;
	int   g_skins;

	int   cg_skinAlly;
	int   cg_skinEnemy;
} cg_t;

//
// Special
//

typedef struct				// used for rendering client data
{ const char  *Name;		// Player name
  vec3_t	  XYZ;			// Coords
  int		  Health;		// Health
  int		  Armor;		// Armor
  int		  Team; 		// Team
  int		  Weapon;		// Weapon
  int		  num;			// Used for Traces
	// anything else?
} specplayerinfo_t;

#define MAX_BOMBSITES 2

typedef struct
{
	int EntityNumber;
	vec3_t	Position;
	int Range;

	// For site marker.
	qboolean  noMarker;
	vec3_t	  markerPos;
	vec3_t	  markerDir;
	qboolean  b;
} bombsite_t;

// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct
{
	qhandle_t  charsetShader;
	qhandle_t  charsetProp;
	qhandle_t  charsetPropGlow;
	qhandle_t  charsetPropB;
	qhandle_t  ringShader;
	qhandle_t  whiteShader;
	qhandle_t  whiteHGrad;
	qhandle_t  whiteShaderSolid;
	qhandle_t  laserShader;
	qhandle_t  trailShader;

	qhandle_t  flagModels[UT_SKIN_COUNT];
	qhandle_t  neutralFlagModel;

	qhandle_t  colorFlagShaders[UT_SKIN_COUNT][3];
	qhandle_t  flagShader[4];
#ifdef MISSIONPACK
	qhandle_t  flagPoleModel;
	qhandle_t  flagFlapModel;

	qhandle_t  redFlagFlapSkin;
	qhandle_t  blueFlagFlapSkin;
	qhandle_t  neutralFlagFlapSkin;

	qhandle_t  flagBaseModels[UT_SKIN_COUNT];
	qhandle_t  neutralFlagBaseModel;

#endif


	qhandle_t  teamStatusBar;

	qhandle_t  deferShader;

	qhandle_t  jumpGoalModel;
	qhandle_t  jumpStartModel;
	qhandle_t  jumpStopModel;

	qhandle_t  bloodBoom;
	qhandle_t  sparkBoom;

	// gib explosions
	qhandle_t	 gibAbdomen;
	qhandle_t	 gibArm;
	qhandle_t	 gibChest;
	qhandle_t	 gibFist;
	qhandle_t	 gibFoot;
	qhandle_t	 gibForearm;
	qhandle_t	 gibIntestine;
	qhandle_t	 gibLeg;
	qhandle_t	 gibSkull;
	qhandle_t	 gibBrain;

	qhandle_t	 mrSentryBarrel;
	qhandle_t	 mrSentryBase;
	qhandle_t	 mrSentryRed;
	qhandle_t	 mrSentryBlue;
	sfxHandle_t  mrSentryFire;
	sfxHandle_t  mrSentrySpinUp;
	sfxHandle_t  mrSentrySpinDown;

	qhandle_t	 smoke2;

	qhandle_t	 machinegunBrassModel;
	qhandle_t	 shotgunBrassModel;

	qhandle_t	 connectionShader;

	qhandle_t	 selectShader;
	qhandle_t	 viewBloodShader;
	qhandle_t	 quadShader;
	qhandle_t	 tracerShader;
	qhandle_t	 lagometerShader;
	qhandle_t	 backTileShader;
	qhandle_t	 noammoShader;

	crosshair_t  crosshairs[MAX_CROSSHAIRS];

	qhandle_t	 snowShader;
	qhandle_t	 smokePuffShader;
	qhandle_t	 smokePuffRageProShader;
	qhandle_t	 shotgunSmokePuffShader;
	qhandle_t	 grenadeSmokePuffShader;

	qhandle_t	 UT_woodFragment;
	qhandle_t	 UT_glassFragment;
	qhandle_t	 UT_metalFragment;
	qhandle_t	 UT_stoneFragment;

	qhandle_t	 viewsmokeshader;

	qhandle_t	 medicshader;
	qhandle_t	 plantingShader;
	qhandle_t	 defusingShader;

	qhandle_t  waterBubbleShader;
	qhandle_t  bloodTrailShader;

	qhandle_t  numberShaders[11];

	qhandle_t  shadowMarkShader;

	qhandle_t  botSkillShaders[5];

	// wall mark shaders
	qhandle_t  wakeMarkShader;
	qhandle_t  bloodMarkShader[4];
	qhandle_t  bulletMarkShader;
	qhandle_t  burnMarkShader;
	qhandle_t  holeMarkShader;

	qhandle_t  dishFlashModel;


	// weapon effect shaders
	qhandle_t  plasmaExplosionShader;

	qhandle_t  grenadeExplosionShader;
	qhandle_t  flashExplosionShader;

	qhandle_t  bloodExplosionShader;
	qhandle_t  bloodExplosionShader1;

	// special effects models
	qhandle_t  bombExplosionShaders[8];
	qhandle_t  bombCloudShader;
	qhandle_t  bombShockwaveShaders[3];
	qhandle_t  bombBrightShader;

	// sounds
	sfxHandle_t  tracerSound[2];
	sfxHandle_t  basetracerSound[3];

	sfxHandle_t  selectSound;

	sfxHandle_t  footsteps[FOOTSTEP_TOTAL][4];

	sfxHandle_t  sfx_ric1;
	sfxHandle_t  sfx_ric2;
	sfxHandle_t  sfx_ric3;

	sfxHandle_t  sfx_plasmaexp;

	sfxHandle_t  gibSound;
	sfxHandle_t  gibBounce1Sound;
	sfxHandle_t  gibBounce2Sound;
	sfxHandle_t  gibBounce3Sound;

	sfxHandle_t  talkSound;
	sfxHandle_t  landSound;
	sfxHandle_t  fallSound;

	sfxHandle_t  watrInSoundFast;
	sfxHandle_t  watrInSound;
	sfxHandle_t  watrOutSound;
	sfxHandle_t  watrUnSound;

	qhandle_t  flagShaders[3];

	qhandle_t  cursor;
	qhandle_t  selectCursor;
	qhandle_t  sizeCursor;

	sfxHandle_t  hgrenb1aSound;
	sfxHandle_t  hgrenb2aSound;
	sfxHandle_t  hgrenb1bSound;
	sfxHandle_t  hgrenb2bSound;

	sfxHandle_t  bombBounce1;
	sfxHandle_t  bombBounce2;

	// Urban Terror media

	// UT Graphics
	qhandle_t  nvgStatic;					// static effect overlay
	qhandle_t  nvgBrightA;					// used to brighten enemy players
	qhandle_t  nvgBrightB;					// used to brighten enemy players

	qhandle_t  nvgScope;
	qhandle_t  nvgNoise[3];
	qhandle_t  UT_fragmentShader;

	// UT Models
	qhandle_t  glassFragments[3];
	qhandle_t  metalFragments[3];
	qhandle_t  stoneFragments[3];

	// UT Sounds
	sfxHandle_t  UT_glassBounceSound;
	sfxHandle_t  UT_glassShatterSound;
	sfxHandle_t  UT_woodBounceSound;
	sfxHandle_t  UT_woodShatterSound;
	sfxHandle_t  UT_ceramicBounceSound;
	sfxHandle_t  UT_ceramicShatterSound;
	sfxHandle_t  UT_plasticBounceSound;
	sfxHandle_t  UT_plasticShatterSound;
	sfxHandle_t  UT_metalShatterSound;
	sfxHandle_t  UT_stoneShatterSound;
	sfxHandle_t  UT_zoomSound;
	sfxHandle_t  UT_bandageSound;
	sfxHandle_t  UT_bodyImpactSound;

	sfxHandle_t  UT_bombBeep;
	sfxHandle_t  UT_bombPlant;
	sfxHandle_t  UT_bombDisarm;

	sfxHandle_t  UT_explodeSound;
	sfxHandle_t  UT_explodeH2OSound;
	sfxHandle_t  UT_smokegrensound;
	sfxHandle_t  UT_radiomenuclick;

	sfxHandle_t  UT_brassBounceSound[2];
	sfxHandle_t  UT_shellBounceSound[3];

	qhandle_t	 UT_laserShaderRed;
	qhandle_t	 UT_laserShaderGreen;

	qhandle_t	 UT_DamageIcon[HL_MAX];

	qhandle_t	 UT_SP_Intro1;

	qhandle_t	 UT_PlayerStatusOutline;
	qhandle_t	 UT_PlayerStatusStamina[9];

	qhandle_t	 UT_Backpack;
	qhandle_t	 UT_BackpackNormal;
	qhandle_t	 UT_BackpackExplosives;
	qhandle_t	 UT_BackpackMedic;

	sfxHandle_t  kickSound;

	qhandle_t	 UT_bullet_mrk_glass;

	qhandle_t	 UT_dropPistol;
	qhandle_t	 UT_dropRifle;
	qhandle_t	 UT_dropWater;

	qhandle_t	 UT_bloodPool[3];
	qhandle_t        UT_waveShader[UT_SKIN_COUNT];
	qhandle_t	 UT_timerShader;
	qhandle_t	 UT_potatoShaders[UT_SKIN_COUNT][UT_SKIN_COUNT];

	//@Fenix - jump mode shaders
	qhandle_t	UT_jumpRunningTimer;
	qhandle_t	UT_jumpBestTimer;

	// shaders that are applied to particles
	qhandle_t  UT_ParticleShader;
	qhandle_t  UT_grassShader;
	qhandle_t  UT_chunkShader;

	qhandle_t  UT_FlagIcon[UT_SKIN_COUNT];
	qhandle_t  UT_FlagNeutralIcon;
	qhandle_t  UT_FlagMedic;
	qhandle_t  UT_Radar;

	//Minimap icons/map
	qhandle_t  MiniMap;
	qhandle_t  MiniMapArrow;

	//Iain Added this, i did ? :P
	sfxHandle_t  sndUnderWater;

	sfxHandle_t  UT_CTF_FlagReturn;

	sfxHandle_t  UT_CTF_CaptureRed;
	sfxHandle_t  UT_CTF_CaptureBlue;

	sfxHandle_t  UT_CTF_FlagTaken;

	//
	// End of Round Sounds - DensitY
	//
	sfxHandle_t   UT_RedWinsRound;
	sfxHandle_t   UT_BlueWinsRound;
	sfxHandle_t   UT_DrawnRound;
	sfxHandle_t   UT_LaughRound;		// Played when other team was owned

	clientInfo_t  Base_Orion;	 //holds model info for orion
	clientInfo_t  Base_Athena;	 //holds model info for athena
	clientInfo_t  Red_Skins[4];  //fw fb mw mb	  holds all current models and skins for red
	clientInfo_t  Blue_Skins[4]; //fw fb mw mb	 holds all current models and skins for blue
	clientInfo_t  Free_Skins[4]; //@Barbatos - fw fb mw mb	holds all current models and skins for the free team
	clientInfo_t  Jump_Skins[4]; //@Barbatos - fw fb mw mb holds all current models and skins for the jump mode
	clientInfo_t  Orange_Skins[4];
	clientInfo_t  Olive_Skins[4];
	clientInfo_t  White_Skins[4];
	clientInfo_t  Black_Skins[4];
	clientInfo_t  Desert_Skins[4];
	clientInfo_t  Cowboy_Skins[4];
	clientInfo_t  Cavalry_Skins[4];
	clientInfo_t  Droogs_Skins[4];

	qhandle_t	  Red_HandSkin;
	qhandle_t	  Blue_HandSkin;
	qhandle_t	  Free_HandSkin;
	qhandle_t	  Jump_HandSkin;
	qhandle_t	  Orange_HandSkin;
	qhandle_t	  Olive_HandSkin;
	qhandle_t	  White_HandSkin;
	qhandle_t	  Black_HandSkin;
	qhandle_t	  Desert_HandSkin;
	qhandle_t	  Cowboy_HandSkin;
	qhandle_t	  Cavalry_HandSkin;
	qhandle_t	  Droogs_HandSkin;

	sfxHandle_t   ut_demoCast;

	sfxHandle_t   ut_BombBeep[3];

	qhandle_t	  ut_BombSiteMarker;
	qhandle_t	  ut_BombMiniMapMarker;

	// Bomb explosion spheres.
	qhandle_t	 sphereLoResModel;
	qhandle_t	 sphereHiResModel;

	sfxHandle_t  stomp;
	sfxHandle_t  hit;

	// Trail shader
	//qhandle_t trailShader;

	//@Fenix - jump timers sounds
	sfxHandle_t   UT_JumpTimerStart;
	sfxHandle_t   UT_JumpTimerStop;
	sfxHandle_t   UT_JumpTimerCancel;

	//@Barbatos - register and load all sounds to avoid having to register them when playing (this causes huge lags)
	sfxHandle_t		ledgegrab;
	sfxHandle_t		water1;
	sfxHandle_t		laserOnOff;
	sfxHandle_t		nvgOn;
	sfxHandle_t		nvgOff;
	sfxHandle_t		flashLight;
	sfxHandle_t		noAmmoSound;
	sfxHandle_t		bombDisarmSound;
	sfxHandle_t		bombExplosionSound;
	sfxHandle_t		blastWind;
	sfxHandle_t		blastFire;
	sfxHandle_t		radioSounds[2][10][10]; //@Barbatos - [gender][group][msg]
	sfxHandle_t     kcactionSound;

} cgMedia_t;

typedef struct
{
	int number;
	int team;
	vec3_t	origin;
} cgFlag_t;

// The client game static (cgs) structure holds everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct
{
	gameState_t  	gameState; 					// gamestate from server
	glconfig_t	 	glconfig;					// rendering configuration
	float		 	screenXScale;				// derived from glconfig
	float		 	screenYScale;
	float		 	screenXBias;

	int 	 		serverCommandSequence; 		// reliable command stream counter
	int 	 		processedSnapshotNum;		// the number of snapshots came has requested

	qboolean	 	localServer;				// detected on startup by checking sv_running

	// parsed from serverinfo
	gametype_t		gametype;
	qboolean		survivor;
	qboolean		antilagvis;

	int 			g_roundcount;
	int 			g_maxrounds;

	qboolean		followenemy;

//	int 			surfacedefault;
	char	  		flags[32];

	qboolean  		cheats;
	float	  		survivorRoundTime;
	int   			gameid;
	int   			respawnDelay;
	int   			waveRespawns;
	int   			redWaveRespawnDelay;
	int   			blueWaveRespawnDelay;
	int   			teamflags;
	int   			fraglimit;
	int   			capturelimit;
	int   			timelimit;
	int   			maxclients;
	char	  		mapname[MAX_QPATH];
	char	  		redTeam[MAX_QPATH];
	char	  		blueTeam[MAX_QPATH];

	int   			voteTime;
	int   			voteYes;
	int   			voteNo;
	qboolean  		voteModified; 					// beep whenever changed
	char	  		voteString[MAX_STRING_TOKENS];

	int   			teamVoteTime[2];
	int   			teamVoteYes[2];
	int   			teamVoteNo[2];
	qboolean  		teamVoteModified[2];			// beep whenever changed
	char	  		teamVoteString[2][MAX_STRING_TOKENS];

	int   			levelStartTime;
	int   			roundEndTime;

	int   			scores1, scores2; 				// from configstrings
	int   			redflag, blueflag;				// flag status from configstrings
	int   			flagStatus;

	qboolean  		newHud;

	//
	// locally derived information from gamestate
	//
	cgFlag_t	  	gameFlags[MAX_FLAGS];
	qhandle_t	  	gameModels[MAX_MODELS];
	sfxHandle_t   	gameSounds[MAX_SOUNDS];

	int 	  	  	numInlineModels;
	qhandle_t	  	inlineDrawModel[MAX_MODELS];
	vec3_t		  	inlineModelMidpoints[MAX_MODELS];

	clientInfo_t  	clientinfo[MAX_CLIENTS];

	// teamchat width is *3 because of embedded color codes
	char	   	teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH * 3 + 1];
	int    		teamChatMsgTimes[TEAMCHAT_HEIGHT];
	int   		teamChatPos;
	int    		teamLastChatPos;

	int    		cursorX;
	int    		cursorY;
	qboolean   	eventHandling;
	qboolean   	mouseCaptured;
	qboolean   	sizingHud;
	void	   	*capturedItem;
	qhandle_t  	activeCursor;

	// orders
	int   		currentOrder;
	qboolean  	orderPending;
	int   		orderTime;
	int   		currentVoiceClient;
	int   		acceptOrderTime;
	int   		acceptTask;
	int   		acceptLeader;
	char	  	acceptVoice[MAX_NAME_LENGTH];

	// media
	cgMedia_t  	media;

// Urban Terror

	//ut single player scores
	int  		spSniperKills; 	// ut single player game mode sniper score
	int  		spObstacleTime;

	// chat width is *3 because of embedded color codes
	char  		chatText[CHAT_HEIGHT][CHAT_WIDTH * 3 + 1];
	int   		chatTextTimes[CHAT_HEIGHT];
	int   		chatPos;
	int   		chatLastPos;

	// message width is *3 because of embedded color codes
	char		msgText[MSG_HEIGHT][MSG_WIDTH * 3 + 1];
	int 		msgTextTimes[MSG_HEIGHT];
	int 		msgPos;
	int 		msgLastPos;

	int 		precipEnabled;
	int 		precipAmount;
	char		nextMapName[MAX_QPATH];

	vec3_t		worldmins;
	vec3_t		worldmaxs;
	float		wxdelta;
	float		txdelta;
	float		tydelta;

	bombsite_t	cg_BombSites[MAX_BOMBSITES];

	int 		hotpotato;	   // 1 means we're in countdown, 0 means we're not
	int 		hotpotatotime; // Time the explosion will occur
	float		g_hotpotato;

	int 		g_noDamage;
	int         g_funstuff;

} cgs_t;

typedef struct {
	vec4_t  defaultArmBandColor;
	char*   defaultTeamColorName;
	char*   defaultTeamName;
	vec4_t  ui_teamscores_teambar_color; // color of the plain horizontal bar in the teamscores ui
	char*   ui_text_color; // color code for that team (^X: ^0=black, ^1=red etc)
	vec4_t  ui_inv_color; // default color to display teamname in the teamscore teambar name (has to be different than ui_miniscoreboard_fill_color
	vec4_t  ui_dead_color; // color for dead player and border in teamoverlay
	vec4_t  ui_fl_info_color; // color for free look ui info
	vec4_t  ui_miniscoreboard_fill_color; // color of the background rectangle in the team score mini score board
	vec4_t  ui_teamoverlay_fill_color; // color of the background rectangle in the team overlay
	vec4_t  ui_combat_color; // used for stamina, weaponm and ammo count
	vec4_t  ui_used_stamina_color;
	vec4_t  ui_lost_stamina_color;
	vec4_t  smoke_color;
	vec4_t  trail_color;
	vec4_t  ui_teamscore_color; // color for name, auth, etc in teamscore ui
				    // seems to be the same as ui_combat_color (may be merge)
	char*   ui_item_list_flag_name;
	// asset loading info
	char*   asset_flag_model_path;
	char*   asset_color_flag_shader;
	char*   asset_flag_icon;
} skinInfo_t;

//==============================================================================

extern cgs_t		 cgs;
extern cg_t 	     cg;
extern centity_t	 cg_entities_new[MAX_GENTITIES];
extern weaponInfo_t  cg_weapons[MAX_WEAPONS];
extern itemInfo_t	 cg_items[MAX_ITEMS];
extern markPoly_t	 cg_markPolys[MAX_MARK_POLYS];
extern particle_t	 p_attr[];

extern skinInfo_t        skinInfos[UT_SKIN_COUNT];

extern vmCvar_t 	 cg_centertime;
extern vmCvar_t 	 cg_runpitch;
extern vmCvar_t 	 cg_runroll;
extern vmCvar_t 	 cg_bobup;
extern vmCvar_t 	 cg_bobpitch;
extern vmCvar_t 	 cg_bobroll;
extern vmCvar_t 	 cg_swingSpeed;
extern vmCvar_t 	 cg_shadows;
extern vmCvar_t 	 cg_drawTimer;
extern vmCvar_t 	 cg_drawFPS;
extern vmCvar_t 	 cg_drawSnapshot;
extern vmCvar_t 	 cg_drawCrosshair;
extern vmCvar_t 	 cg_drawCrosshairNames;
extern vmCvar_t 	 cg_crosshairNamesType;
extern vmCvar_t 	 cg_crosshairNamesSize;
extern vmCvar_t 	 cg_crosshairRGB;
extern vmCvar_t 	 cg_crosshairFriendRGB;
extern vmCvar_t 	 cg_crosshairX;
extern vmCvar_t 	 cg_crosshairY;
extern vmCvar_t 	 cg_crosshairSize;
extern vmCvar_t 	 cg_crosshairHealth;
//extern	vmCvar_t		cg_scopeType;
extern vmCvar_t 	 cg_scopeG36;
extern vmCvar_t 	 cg_scopePSG;
extern vmCvar_t 	 cg_scopeSR8;
extern vmCvar_t 	 cg_scopering;
extern vmCvar_t 	 cg_nvg;

extern vmCvar_t 	 cg_drawStatus;
extern vmCvar_t 	 cg_draw2D;
extern vmCvar_t 	 cg_animSpeed;
extern vmCvar_t 	 cg_debugAnim;
extern vmCvar_t 	 cg_debugPosition;
extern vmCvar_t 	 cg_debugEvents;
extern vmCvar_t 	 cg_errorDecay;
extern vmCvar_t 	 cg_nopredict;
extern vmCvar_t 	 cg_noweaponpredict;
//extern	vmCvar_t		cg_antilag;
extern vmCvar_t 	 cg_noPlayerAnims;
extern vmCvar_t 	 cg_showmiss;
extern vmCvar_t 	 cg_servertime;

extern vmCvar_t 	 cg_footsteps;
//extern	vmCvar_t		cg_addMarks;
extern vmCvar_t 	 cg_gun_frame;
extern vmCvar_t 	 cg_gun_x;
extern vmCvar_t 	 cg_gun_y;
extern vmCvar_t 	 cg_gun_z;
extern vmCvar_t 	 cg_drawGun;
extern vmCvar_t 	 cg_viewsize;
extern vmCvar_t 	 cg_tracerChance;
extern vmCvar_t 	 cg_tracerWidth;
extern vmCvar_t 	 cg_tracerLength;
extern vmCvar_t 	 cg_autoswitch;
extern vmCvar_t 	 cg_ignore;
extern vmCvar_t 	 cg_simpleItems;
extern vmCvar_t 	 cg_fov;
extern vmCvar_t 	 cg_zoomFov;
extern vmCvar_t 	 cg_thirdPersonRange;
extern vmCvar_t 	 cg_thirdPersonAngle;
extern vmCvar_t 	 cg_thirdPerson;
extern vmCvar_t 	 cg_stereoSeparation;
extern vmCvar_t 	 cg_lagometer;
extern vmCvar_t 	 cg_drawAttacker;
extern vmCvar_t 	 cg_synchronousClients;
extern vmCvar_t 	 cg_teamChatTime;
extern vmCvar_t 	 cg_teamChatHeight;
extern vmCvar_t 	 cg_stats;
extern vmCvar_t 	 cg_buildScript;
extern vmCvar_t 	 cg_paused;
extern vmCvar_t 	 cg_blood;
extern vmCvar_t 	 cg_drawFriend;
extern vmCvar_t 	 cg_teamChatsOnly;
extern vmCvar_t		 cg_chatSound;
extern vmCvar_t		 cg_cleanFollow;
extern vmCvar_t		 cg_noci;
extern vmCvar_t 	 cg_noVoiceChats;
extern vmCvar_t 	 cg_noVoiceText;
extern vmCvar_t 	 cg_smoothClients;
//extern	vmCvar_t		pmove_fixed;
//extern	vmCvar_t		pmove_msec;
extern vmCvar_t  cg_cameraOrbit;
extern vmCvar_t  cg_cameraOrbitDelay;
extern vmCvar_t  cg_timescaleFadeEnd;
extern vmCvar_t  cg_timescaleFadeSpeed;
extern vmCvar_t  cg_timescale;
extern vmCvar_t  cg_cameraMode;
extern vmCvar_t  cg_smallFont;
extern vmCvar_t  cg_bigFont;
extern vmCvar_t  cg_noTaunt;
extern vmCvar_t  cg_singlePlayer;
extern vmCvar_t  cg_enableDust;
extern vmCvar_t  cg_enableBreath;
extern vmCvar_t  cg_singlePlayerActive;
extern vmCvar_t  cg_recordSPDemo;
extern vmCvar_t  cg_recordSPDemoName;
extern vmCvar_t  cg_drawTeamOverlay;
extern vmCvar_t  cg_drawTeamOverlayScores;

// Urban Terror cvars
extern vmCvar_t  cg_maxFragments;
extern vmCvar_t  cg_spaceInvaders;
extern vmCvar_t  cg_markTotaltime;
extern vmCvar_t  cg_doWarmup;
extern vmCvar_t  cg_visibleBleeding;
extern vmCvar_t  cg_hitsound;
extern vmCvar_t  cg_walljumps;
extern vmCvar_t  cg_skinAlly;
extern vmCvar_t  cg_skinEnemy;
extern vmCvar_t  cg_funstuff;

extern vmCvar_t  cg_drawHands;
extern vmCvar_t  cg_chatHeight;
extern vmCvar_t  cg_chatTime;
extern vmCvar_t  cg_msgHeight;
extern vmCvar_t  cg_msgTime;
extern vmCvar_t  cg_maxPrecip;
//extern	vmCvar_t		cg_boundsoptimize;
extern vmCvar_t  cg_gunSize;
//extern  vmCvar_t		cg_gunCorrectFOV;
extern vmCvar_t  cg_showBulletHits;
extern vmCvar_t  cg_standardChat;
extern vmCvar_t  cg_drawTeamScores;
//extern	vmCvar_t		cg_radar;
//extern	vmCvar_t		cg_radarRange;
extern vmCvar_t  cg_autoScreenshot;
extern vmCvar_t  cg_zoomWrap;
extern vmCvar_t  cg_viewBlob;

extern vmCvar_t  cg_sfxMuzzleFlash;
extern vmCvar_t  cg_sfxSurfaceImpacts;
extern vmCvar_t  cg_sfxBreathing;
extern vmCvar_t  cg_sfxBrassTime;
extern vmCvar_t  cg_sfxShowDamage;
extern vmCvar_t  cg_sfxParticles;
extern vmCvar_t  cg_sfxParticleSmoke;
extern vmCvar_t  cg_sfxVisibleItems;
extern vmCvar_t  cg_autoRadio;

extern vmCvar_t  cg_mapsize;
extern vmCvar_t  cg_mappos;
extern vmCvar_t  cg_maptoggle;

extern vmCvar_t  cg_speedo;

extern vmCvar_t  cg_mapalpha;
extern vmCvar_t  cg_maparrowscale;

extern vmCvar_t  cg_SpectatorShoutcaster;
extern vmCvar_t  cg_followpowerup;
extern vmCvar_t  cg_followkiller;
extern vmCvar_t  cg_WeaponModes;

extern vmCvar_t  cg_autoRecordMatch;

extern vmCvar_t  cg_pauseYaw;
extern vmCvar_t  cg_pausePitch;
//extern	vmCvar_t		cg_debugAW;

extern vmCvar_t  cg_hudWeaponInfo;

extern vmCvar_t  cg_physics;
extern vmCvar_t  cg_optimize;
extern vmCvar_t  ut_timenudge;
extern vmCvar_t  r_fastsky;

extern vmCvar_t  r_debugAries;

extern vmCvar_t  cg_demoFov;
extern vmCvar_t  cg_drawKillLog;

extern vmCvar_t  r_primitives;

extern vmCvar_t  cg_ghost;

//
// cg_main.c
//
const char *CG_ConfigString( int index );
const char *CG_Argv( int arg );

void QDECL	CG_Printf( const char *msg, ... );
void QDECL	CG_Error( const char *msg, ... );

void		CG_StartMusic( void );

void		CG_UpdateCvars( void );
void		CG_LimitCvars( void );
void		CG_UpdateSkins( int );

qboolean CG_IsSkinSelectionValid( void );

int 	CG_CrosshairPlayer( void );
int 	CG_LastAttacker( void );
void		CG_LoadMenus(const char *menuFile);
void		CG_KeyEvent(int key, qboolean down);
void		CG_MouseEvent(int x, int y);
void		CG_EventHandling(int type);
void		CG_RankRunFrame( void );
void		CG_SetScoreSelection(void *menu);
score_t *	CG_GetSelectedScore(void);
void		CG_BuildSpectatorString(void);

//
// cg_view.c
//
void CG_TestModel_f (void);
void CG_TestGun_f (void);
void CG_TestModelNextFrame_f (void);
void CG_TestModelPrevFrame_f (void);
void CG_TestModelNextSkin_f (void);
void CG_TestModelPrevSkin_f (void);
void CG_ZoomDown_f( void );
void CG_ZoomUp_f( void );
void CG_AddBufferedSound( sfxHandle_t sfx);

void CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );

// Urban terror
void CG_InitPrecipitation( void );
void CG_AddPrecipitation( void );
void CG_StartDrop( int index, qboolean snow, qboolean culled );
void CG_DrawFlake( vec3_t result, float radius );

//New mini map
void	 CG_FindLevelExtents(vec3_t worldmins, vec3_t worldmaxs);
void	 CG_BuildMiniMap(void);
void	 CG_LoadMiniMap(void); //Call this One
void	 CG_DrawMiniMap(void);

void	 CG_BuildRainMap(void); //For the rain and snow.. builds a collision map

qboolean CG_InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles);
// density
void	 CG_DrawSpectatorData( void );
void	 CG_DrawOnScreenBombMarkers(void);

//
// cg_drawtools.c
//
#define CG_AdjustFrom640(x, y, w, h) { *(x) *= cgs.screenXScale; \
					   *(y) *= cgs.screenYScale; \
					   *(w) *= cgs.screenXScale; \
					   *(h) *= cgs.screenYScale; }

//void CG_AdjustFrom640( float *x, float *y, float *w, float *h );
void CG_FillRect( float x, float y, float width, float height, const float *color );
void CG_FillRectHGrad( float x, float y, float width, float height, const float *color );
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void CG_DrawString( float x, float y, const char *string,
			float charWidth, float charHeight, const float *modulate );

void   CG_DrawStringExt( int x, int y, const char *string, const float *setColor,
			 qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars );
void   CG_DrawBigString( int x, int y, const char *s, float alpha );
void   CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color );
void   CG_DrawSmallString( int x, int y, const char *s, float alpha );
void   CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color );

int    CG_DrawStrlen( const char *str );

float *CG_FadeColor( int startMsec, int totalMsec );
float *CG_TeamColor( int team );
void   CG_TileClear( void );
void   CG_ColorForHealth( vec4_t hcolor );
void   CG_GetColorForHealth( int health, int armor, vec4_t hcolor );

void   UI_DrawProportionalString( int x, int y, const char *str, int style, vec4_t color );
void   CG_DrawRect( float x, float y, float width, float height, float size, const float *color );
void   CG_DrawSides(float x, float y, float w, float h, float size);
void   CG_DrawTopBottom(float x, float y, float w, float h, float size);

//
// cg_draw.c, cg_newDraw.c
//
//extern	int sortedTeamPlayers[TEAM_MAXOVERLAY];
//extern	int numSortedTeamPlayers;
extern int	 drawTeamOverlayModificationCount;
extern char  systemChat[256];
extern char  teamChat1[256];
extern char  teamChat2[256];

void		CG_AddLagometerFrameInfo( void );
void		CG_AddLagometerSnapshotInfo( snapshot_t *snap, qboolean warped );
void		CG_CenterPrint( const char *str, int y, int charWidth );
void		CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles );
void		CG_DrawActive( stereoFrame_t stereoView );
void		CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D );
void		CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team );
void		CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int ownerDrawParam, int ownerDrawParam2, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle);
void		CG_Text_Paint(float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style);
int 	CG_Text_Width(const char *text, float scale, int limit);
int 	CG_Text_Height(const char *text, float scale, int limit);
void		CG_SelectPrevPlayer(void);
void		CG_SelectNextPlayer(void);
float		CG_GetValue(int ownerDraw);
qboolean	CG_OwnerDrawVisible(int flags);
void		CG_RunMenuScript(itemDef_t *, char * *args);
void		CG_ShowResponseHead(void);
void		CG_SetPrintString(int type, const char *p);
void		CG_InitTeamChat(void);
void		CG_GetTeamColor(vec4_t *color);
const char *CG_GetGameStatusText(void);
const char *CG_GetGameStatusTextOld(void);

const char *CG_GetKillerText(void);
void		CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles );
void		CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);
void		CG_CheckOrderPending(void);
const char *CG_GameTypeString(void);
qboolean	CG_YourTeamHasFlag(void);
qboolean	CG_OtherTeamHasFlag(void);
qhandle_t	CG_StatusHandle(int task);

// Urban Terror
void	 CG_SetColorOverlay 	( float r, float g, float b, float a, int duration, int fade, int fadeTime );
void	 CG_DrawColorOverlay		(void);
void	 CG_SetShaderOverlay		( qhandle_t shaderHandle, int duration );
void	 CG_DrawShaderOverlay		(void);
void	 CG_DrawSmokeOverlay		(void);
void	 CG_DrawFlashGrenadeFade	(void);

void	 inv_doround( void );
void	 inv_initgame( void );
void	 inv_rcforxy ( const int x, const int y, int *row, int *col );
void	 inv_xyforrc ( const int row, const int col, int *x, int *y );
int  inv_shotCollideInvader( int shotx, int shoty );
qboolean inv_shotCollidePlayer( int shotx, int shoty );
qboolean inv_checkbarrier( int sx, int sy, qboolean isplayer );
void	 inv_invaderBounds( void );
void	 inv_ai( void );
void	 inv_uidraw( void );
void	 inv_drawbase( int base );
void	 inv_hiscore( void );

////////// BREAKOUT //////////
void breakout_DoFrame(void);

/////////// SNAKE ////////////
void snake_DoFrame(void);

//
// cg_player.c
//
utSkins_t       CG_WhichSkin( team_t team );
void		CG_PlayerBB( centity_t *cent ); // For Debugging the AntiLag
void		CG_Corpse( centity_t *cent );
void		CG_Player( centity_t *cent );
void		CG_ResetPlayerEntity( centity_t *cent );
void		CG_AddRefEntityWithNVGGlow( refEntity_t *ent, entityState_t *state );
void		CG_NewClientInfo( int clientNum );
sfxHandle_t CG_CustomSound( int clientNum, const char *soundName );
// Added - DensitY
void		CG_BuildPlayerSpectatorList( centity_t *cent );
void CG_LoadSkinOnDemand(utSkins_t skin);
void		CG_SetupPlayerGFX(int clientNum, clientInfo_t *ci);

//
// cg_predict.c
//
void CG_BuildSolidList( void );
int  CG_PointContents( const vec3_t point, int passEntityNum );
void CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
		   int skipNumber, int mask );
void CG_PredictPlayerState( void );
void CG_BuildBoundsOptimizer(void );

//
// cg_events.c
//
void		CG_CheckEvents( centity_t *cent );
const char *CG_PlaceString( int rank );
void		CG_EntityEvent( centity_t *cent, vec3_t position );
void		CG_PainEvent( centity_t *cent, int health );

//
// cg_ents.c
//
void CG_SetEntitySoundPosition( centity_t *cent );
void CG_AddPacketEntities( void );
void CG_Beam( centity_t *cent );
//void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out );
void CG_AdjustPositionForMover( const vec3_t in, const vec3_t ain, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t aout );

void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
				 qhandle_t parentModel, char *tagName );
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
					qhandle_t parentModel, char *tagName );
void CG_AddBombSite( centity_t *ce );

//
// cg_weapons.c
//
void	 CG_BiggestWeapon_f(void);
void	 CG_NextWeapon_f( void );
void	 CG_PrevWeapon_f( void );
void	 CG_Weapon_f( void );

void	 CG_RegisterWeapon( int weaponNum );
void	 CG_RegisterItemVisuals( int itemNum );

qboolean CG_CalcMuzzlePoint( int entityNum, vec3_t muzzle );

void	 CG_FireWeapon( centity_t *cent );
void	 CG_MissileHitWall( int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType );
void	 CG_MissileHitPlayer( int weapon, vec3_t origin, vec3_t dir, int entityNum );
void	 CG_ShotgunFire( entityState_t *es );
void	 CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum, int surfaces, int severity, int weapon );

void	 CG_GrappleTrail( centity_t *ent, const weaponInfo_t *wi );
void	 CG_AddViewWeapon (playerState_t *ps);
void	 CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent, int team );
void	 CG_DrawWeaponSelect( void );
void	 CG_DrawItemSelect( void );

void	 CG_OutOfAmmoChange( void );	// should this be in pmove?

void	 CG_SetViewAnimation ( int anim );
void	 CG_Tracer( vec3_t source, vec3_t dest, qboolean silenced );

void	 CG_DrawLaserDot(vec3_t end);
void	 CG_DrawLaser(vec3_t start, vec3_t drawfrom, vec3_t end, qboolean drawdot, centity_t *cent);
void	 CG_DrawSmokeLines(vec3_t start, vec3_t end);
void	 CG_ClearLaserSmoke(void);
void	 CG_MarkSmokeForLaser(localEntity_t *in);

qboolean RaySphere( vec3_t rayorigin, vec3_t raydir, vec3_t sphereorigin, float radius, vec3_t intersection1, vec3_t intersection2 );

//
// cg_marks.c
//
void CG_InitMarkPolys( void );
void CG_AddMarks( void );
void CG_ImpactMark( qhandle_t markShader,
			const vec3_t origin, const vec3_t dir,
			float orientation,
			float r, float g, float b, float a,
			qboolean alphaFade,
			float radius, qboolean temporary, centity_t *parent );
void CG_RemoveChildMarks( int entitynum );
void CG_ClearAllMarks(void);

//
// cg_localents.c
//
void		   CG_InitLocalEntities( void );
localEntity_t *CG_AllocLocalEntity( void );
void		   CG_AddLocalEntities( void );

void		   CG_AddParticle( localEntity_t *le );
void		   CG_Line( vec3_t source, vec3_t dest, float radius, qhandle_t shader, vec4_t srccol, vec4_t destcol );
//void CG_AddEmitter( localEntity_t *le );
void		   CG_AddParticleToScene ( localEntity_t *le );
void		   CG_AddShardToScene ( localEntity_t *le );

//
// cg_effects.c
//
localEntity_t *CG_SmokePuff( const vec3_t p,
				 const vec3_t vel,
				 float radius,
				 float r, float g, float b, float a,
				 float duration,
				 int startTime,
				 int fadeInTime,
				 int leFlags,
				 qhandle_t hShader );
void		   CG_BubbleTrail( vec3_t start, vec3_t end, float spacing );
void		   CG_ScorePlum( int client, vec3_t org, int score );
void		   CG_BleedPlayer( vec3_t woundOrigin, vec3_t woundDirection, int speed );

void		   CG_GibPlayer(vec3_t origin, vec3_t velocity);
void		   CG_BigExplode( vec3_t playerOrigin );

void		   CG_Bleed( vec3_t origin, int entityNum );

void		   CG_BombExplosion(vec3_t origin, float radius);
void		   CG_BombExplosionThink(void);
localEntity_t *CG_FragExplosion(vec3_t origin, vec3_t dir, int radius, qhandle_t model, qhandle_t shader, int msec);

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir, int radius,
				 qhandle_t hModel, qhandle_t shader, int msec,
				 qboolean isSprite, qboolean particles );

localEntity_t *CG_MakeFlash( vec3_t origin, int msec );

// void CG_BreakBreakable( entityState_t *es, vec3_t dir );
void  CG_BreakBreakable( entityState_t * es, vec3_t, qhandle_t shader );
localEntity_t *CG_LaunchFragment( vec3_t origin, vec3_t velocity, qhandle_t hModel, qhandle_t hShader, int breaktype, float xs, float ys, float zs );
//void CG_LaunchParticle( vec3_t origin, vec3_t velocity, particle_t *part );
void		   CG_Ricochet( vec3_t point, vec3_t from, vec3_t normal, int bits, int surfaces );

void		   CG_CreateEmitter ( vec3_t origin,
				  vec3_t velocity,
				  int duration,
				  int particles,
				  vec3_t pbasevel,
				  float randomness,
				  int trtype,
				  particle_t *part );

void CG_Shards( vec3_t mins, vec3_t maxs, vec3_t normal, int thinaxis, int shards );
void  ut_tesselate( vec3_t a, vec3_t b, vec3_t c, float tris[UT_MAX_SHARDS][4][3], int level );
void BuildLightFloat(vec3_t WorldOrigin, vec3_t StartColor, vec3_t Result);
void BuildLightByte(vec3_t WorldOrigin, vec3_t StartColor, char *Result);

//
// cg_snapshot.c
//
void CG_ProcessSnapshots( void );

//
// cg_info.c
//
void CG_LoadingString( const char *s );
void CG_LoadingItem( int itemNum );
void CG_LoadingClient( int clientNum );
void CG_DrawInformation( void );

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand( void );
void	 CG_InitConsoleCommands( void );
void	 CG_RecordDemo(void);

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands( int latestSequence );
void CG_ParseServerinfo( void );
void CG_SetConfigValues( void );
void CG_ShaderStateChanged(void);
//char* CG_VoiceChatLocal( int mode, qboolean voiceOnly, int clientNum, int color, const char *cmd );
//void	CG_PlayBufferedVoiceChats( void );
void CG_AddChatText ( const char *str );
void CG_AddMessageText ( const char *str );

//
// cg_scoreboard.c
//
void DrawRadioWindow(void);

//
// cg_playerstate.c
//
void CG_Respawn( void );
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void CG_CheckChangedPredictableEvents( playerState_t *ps );

void CG_DrawPersistantBBs(void);
//
// cg_particles.c
//
void CG_ParticleExplode (vec3_t origin,
			 vec3_t from,
			 vec3_t normal,
			 int count,
			 float radius,
			 localEntity_t *le
			 );

void		   CG_ParticleLaunch ( vec3_t origin, vec3_t velocity, localEntity_t *le );
void		   CG_ParticleSetAttributes( localEntity_t *le, int attr, qhandle_t partShader, qhandle_t markShader, qhandle_t model );
localEntity_t *CG_ParticleCreate ( void );
localEntity_t *CG_ParticleClone( const localEntity_t *original );
void		   CG_ParticleSmoke( localEntity_t *le, qhandle_t shader );

//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void trap_Print( const char *fmt );

// abort the game
void trap_Error( const char *fmt );

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int trap_Milliseconds( void );

// console variable interaction
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void trap_Cvar_Update( vmCvar_t *vmCvar );
void trap_Cvar_Set( const char *var_name, const char *value );
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

// ServerCommand and ConsoleCommand parameter access
int  trap_Argc( void );
void trap_Argv( int n, char *buffer, int bufferLength );
void trap_Args( char *buffer, int bufferLength );

// filesystem access
// returns length of file
int  trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void trap_FS_Read( void *buffer, int len, fileHandle_t f );
void trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void trap_FS_FCloseFile( fileHandle_t f );

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void trap_SendConsoleCommand( const char *text );

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void trap_AddCommand( const char *cmdName );

// send a string to the server over the network
void trap_SendClientCommand( const char *s );

// force a screen update, only used during gamestate load
void trap_UpdateScreen( void );

// model collision
void		 trap_CM_LoadMap( const char *mapname );
int 	 trap_CM_NumInlineModels( void );
clipHandle_t trap_CM_InlineModel( int index );		// 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs );
int 	 trap_CM_PointContents( const vec3_t p, clipHandle_t model );
int 	 trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
void		 trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
				   const vec3_t mins, const vec3_t maxs,
				   clipHandle_t model, int brushmask );
void trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
				  const vec3_t mins, const vec3_t maxs,
				  clipHandle_t model, int brushmask,
				  const vec3_t origin, const vec3_t angles );

// Returns the projection of a polygon onto the solid brushes in the world
int trap_CM_MarkFragments( int numPoints, const vec3_t *points,
			   const vec3_t projection,
			   int maxPoints, vec3_t pointBuffer,
			   int maxFragments, markFragment_t *fragmentBuffer );

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
void trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void trap_S_ClearLoopingSounds( qboolean killall );
void trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );

// repatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void  trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed );			// returns buzz if not found
void		trap_S_StartBackgroundTrack( const char *intro, const char *loop ); 		// empty name stops music
void		trap_S_StopBackgroundTrack( void );

void		trap_R_LoadWorldMap( const char *mapname );

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t trap_R_RegisterModel( const char *name ); 			// returns rgb axis if not found
qhandle_t trap_R_RegisterSkin( const char *name );				// returns all white if not found
qhandle_t trap_R_RegisterShader( const char *name );				// returns all white if not found
qhandle_t trap_R_RegisterShaderNoMip( const char *name );			// returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void trap_R_ClearScene( void );
void trap_R_AddRefEntityToScene( const refEntity_t *re );

// polys are intended for simple wall marks, not really for doing
// significant construction
void trap_R_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts );
void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
int  trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void trap_R_RenderScene( const refdef_t *fd );
void trap_R_SetColor( const float *rgba );		// NULL = 1,1,1,1
void trap_R_DrawStretchPic( float x, float y, float w, float h,
				float s1, float t1, float s2, float t2, qhandle_t hShader );
void trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
int  trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame,
			 float frac, const char *tagName );
void trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void trap_GetGlconfig( glconfig_t *glconfig );

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void trap_GetGameState( gameState_t *gamestate );

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned seperately so that
// snapshot latency can be calculated.
void trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime );

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot );

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean trap_GetServerCommand( int serverCommandNumber );

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int  trap_GetCurrentCmdNumber( void );

qboolean trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd );

// used for the weapon select and zoom
void trap_SetUserCmdValue( int stateValue, float sensitivityScale );

// aids for VM testing
void	 testPrintInt( char *string, int i );
void	 testPrintFloat( char *string, float f );

int  trap_MemoryRemaining( void );
void	 trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean trap_Key_IsDown( int keynum );
int  trap_Key_GetCatcher( void );
void	 trap_Key_SetCatcher( int catcher );
int  trap_Key_GetKey( const char *binding );

int  trap_RealTime(qtime_t *qtime);

int  trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic (int handle);
void	 trap_CIN_DrawCinematic (int handle);
void	 trap_CIN_SetExtents (int handle, int x, int y, int w, int h);

void	 trap_SnapVector( float *v );

// Urban Terror;
qboolean	CG_ParseWeaponAnimations		( char *filename, animation_t *animations );
qboolean	CG_ParseWeaponSounds		( const char *filename, weaponInfo_t *wp );
//void			CG_LaserSight				( centity_t *cent);
void		CG_UTZoomIn 					( void );
void		CG_UTZoomOut				( void );
void		CG_UTZoomReset				( void );
void		CG_DrawSniperCrosshair		( void );
void		CG_DrawNVG						( int pass );
const char *CG_GetColoredClientName 	( clientInfo_t * );

void		CG_LaunchParticles			(vec3_t origin,
						 vec3_t from,
						 vec3_t normal,
						 int count,
						 float radius,
						 int minspeed,
						 int maxspeed,
						 particle_t *particle
						 );

/*
extern ariesModel_t*	g_pAriesUpper[ARIES_MODEL_COUNT];
extern ariesModel_t*	g_pAriesLower[ARIES_MODEL_COUNT];
extern ariesModel_t*	g_pAriesHead[ARIES_MODEL_COUNT];
extern ariesModel_t*	g_pAriesHelmet[ARIES_MODEL_COUNT];
*/

void	 preloadAllClientData(void);
void	 preloadAllSurfaceEffects(void);
qboolean surfaceImpactSound  ( vec3_t origin, int entityNum, int surfaceType );
//void		surfaceBulletHit	( vec3_t origin, vec3_t from, vec3_t normal, int surfaces, int severity );
void	 surfaceBulletHit	 ( vec3_t origin, vec3_t from, vec3_t normal, int surfaces, int severity, centity_t *parent );
void	 surfaceFootstep	 ( int entity, int surfaces );

void	 CG_InitPrecipitation	 ( void );
void	 CG_ScoreboardInit		 ( void );
char *	 CG_GetTimerString		 ( void );
char *	 CG_GetWaveTimerString	 ( int team );
char *	 CG_GetHotPotatoString	 ( void );
void	 CG_RadioMessage		 ( int group, int msg );
void	 CG_WeaponMode			 ( int oldMode );
void	 CG_DrawPlayerHealth		 ( rectDef_t *rect, vec4_t color );
void	 CG_DrawPlayerAmmoValue  ( rectDef_t *rect, vec4_t color );
void	 CG_DrawPlayerWeaponMode ( rectDef_t *rect, float scale, vec4_t color, int textStyle );
void	 CG_ReloadHandSkins 	 ( int clientNum );
void	 CG_DrawBB( vec3_t centervec, vec3_t mins, vec3_t maxs, float thickness, int red, int green, int blue);    //27
void  CG_DrawBBAxis( vec3_t centervec, vec3_t mins, vec3_t maxs, float thickness, vec3_t axis[3]);	   //27
void	 CG_WeapToggle_f ( void );
void	 CG_Radio_f ( void );
void	 CG_MemInfo_f ( void );
void	 CG_AddWeapon( refEntity_t *gun );
void	 CG_AddWeaponShiney( refEntity_t *gun );

// Builds a symetrical AABB of a non symetrical transformed BB
void		 ApplyRotationToPoint(vec3_t axis[3], vec3_t in);
void CG_EjectBullet ( centity_t *cent, refEntity_t *parent, float lengthScale, float diameterScale );
void NvgWorldToScreen(vec3_t world, vec2_t screen);

int HashStr(const char *s);


#define CG_ENTITIES(x) (&cg_entities_new[x])

// Used in cg_predict
extern unsigned char predict_xor1;
extern unsigned char predict_xor2;

/* r00t's hax reporting */
#ifdef ENCRYPTED
// trap_R_AddRefEntityToScene check macro
extern unsigned int ent_offsets[]; // refEntity_t offsets to check
extern unsigned int ent_latch[];   // ORed value differences

#define trap_R_AddRefEntityToSceneX(RE) \
 do{ \
  const refEntity_t *___re = RE; \
  unsigned int chk[3]; \
  chk[0]=*(unsigned int*)(((char*)___re)+ent_offsets[0]); /* save old values */ \
  chk[1]=*(unsigned int*)(((char*)___re)+ent_offsets[1]); \
  chk[2]=*(unsigned int*)(((char*)___re)+ent_offsets[2]); \
  trap_R_AddRefEntityToScene(___re); \
  ent_latch[0]|=chk[0] - (*(unsigned int*)(((char*)___re)+ent_offsets[0])); /* any result except zero will be detected */ \
  ent_latch[1]|=chk[1] - (*(unsigned int*)(((char*)___re)+ent_offsets[1])); \
  ent_latch[2]|=chk[2] - (*(unsigned int*)(((char*)___re)+ent_offsets[2])); \
 }while(0)


// For trap_R_RenderScene checking
extern unsigned int refdef_offsets[];  // refEntity_t offsets to check
extern unsigned int refdef_latch[];    // ORed value differences

// Hit counts
extern int hits_num[];

// Detected bit flags
extern int hax_detect_flags;

// Called every frame
void UpdateHaxCounters();

// Send collected info to server
void SendHaxValues();
#else
#define trap_R_AddRefEntityToSceneX trap_R_AddRefEntityToScene
#endif

#endif
