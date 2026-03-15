// Copyright (C) 1999-2000 Id Software, Inc.
//

//@Barbatos
#define LMS_BONUS						   5


#define CTF_CAPTURE_BONUS				   15					//UT: what you get for capture
#define CTF_TEAM_BONUS					   10					//UT: what your team gets for capture
#define CTF_RECOVERY_BONUS				   1					//UT: what you get for recovery
#define CTF_FRAG_CARRIER_BONUS			   1					//UT: what you get for fragging enemy flag carrier
#define CTF_CARRIER_DANGER_PROTECT_BONUS   1					// bonus for fraggin someone who has recently hurt your flag carrier

#define CTF_CARRIER_DANGER_PROTECT_TIMEOUT 8000 		//UT: Time for FC protection

#define CTF_PROTECT_RADIUS				   600
#define CTF_FLAG_DEFENSE_BONUS			   2			// bonus for fraggin someone while either your target is near your flag carrier
#define CTF_PREVENT_CAPTURE_BONUS		   1			//UT: for killing someone who almost capped :P
#define CTF_FLAG_RETURN_TIME			   40000		// seconds until auto return

#define CTF_TARGET_PROTECT_RADIUS		   1000 		// the radius around an object being defended where a target will be worth extra frags
#define CTF_ATTACKER_PROTECT_RADIUS 	   1000 		// the radius around an object being defended where an attacker will get extra frags when making kills

#define CTF_CARRIER_DANGER_PROTECT_TIMEOUT 8000
#define CTF_FRAG_CARRIER_ASSIST_TIMEOUT    10000
#define CTF_RETURN_FLAG_ASSIST_TIMEOUT	   10000

#define CTF_GRAPPLE_SPEED				   750				// speed of grapple in flight
#define CTF_GRAPPLE_PULL_SPEED			   750				// speed player is pulled at

#define OVERLOAD_ATTACK_BASE_SOUND_TIME    20000

// Prototypes

int 	OtherTeam(team_t team);
const char *TeamName(team_t team);
const char *ColoredTeamName(team_t team);
const char *OtherTeamName(team_t team);
const char *TeamColorString(team_t team);
void		AddTeamScore(team_t team, int score);

void		Team_DroppedFlagThink(gentity_t *ent);
void		Team_FragBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker);
void		Team_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker);
void		Team_InitGame(void);
void		Team_ReturnFlag(team_t team);
void		Team_FreeEntity(gentity_t *ent);
gentity_t * SelectCTFSpawnPoint ( team_t team, playerTeamStateState_t teamstate, vec3_t origin, vec3_t angles );
gentity_t * SelectUTSpawnPoint ( team_t team, playerTeamStateState_t teamstate, vec3_t origin, vec3_t angles ); // GottaBeKD: UT spawn selection routine
gentity_t * Team_GetLocation(gentity_t *ent);
qboolean	Team_GetLocationMsg(gentity_t *ent, char *loc, int loclen);
void		TeamplayInfoMessage( gentity_t *ent );
void		CheckTeamStatus(void);

int 	Pickup_Team( gentity_t *ent, gentity_t *other );
