/**
 * Filename: cg_players.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * Handle the media and animation for player entities
 */

#include "cg_local.h"
#include "../common/c_players.h"
#include "cg_drawaries.h"

/* Player info in spectator mode */
specplayerinfo_t  PlayerInfo[MAX_CLIENTS];

extern vec3_t  bg_colors[20];

void CG_DrawTrail(centity_t *cent, vec3_t top, vec3_t bot);

char  *cg_customSoundNames[MAX_CUSTOM_SOUNDS] = {
	"*death1.wav",
	"*death2.wav",
	"*death3.wav",
	"*pain25_1.wav",
	"*pain50_1.wav",
	"*pain75_1.wav",
	"*pain100_1.wav",
	"*death1_h2o.wav",
	"*death2_h2o.wav",
	"*death3_h2o.wav",
	"*pain25_1_h2o.wav",
	"*pain50_1_h2o.wav",
	"*pain75_1_h2o.wav",
	"*pain100_1_h2o.wav",
	"*gasp.wav",
	"*drown.wav",
	"*fall1.wav",
	"*bleeding1.wav",
	"*bleeding2.wav",
	"*bleeding3.wav",
	"*breathin1.wav",
	"*breathout1.wav",
};

static qboolean CG_LoadPlayerModels(void);
static qboolean CG_RegisterModel(clientInfo_t *ci, const char *modelName) ;
static void	CG_CopyClientInfoModel(clientInfo_t *from, clientInfo_t *to) ;
void		CG_SetupPlayerGFX(int clientNum, clientInfo_t *ci);

/**
 * CG_WhichSkin
 */
utSkins_t CG_WhichSkin(team_t team)
{
	int       selector;
	utSkins_t dft;

	switch (cgs.gametype) {
	/*
	 * In FFA, LMS, Jump, consider everyone as an enemy
	 */
	case GT_FFA: case GT_LASTMAN: case GT_SINGLE_PLAYER:
		dft = UT_SKIN_GREEN;
		selector = cg.cg_skinEnemy;
		break;
	case GT_JUMP:
		dft = UT_SKIN_PURPLE;
		selector = cg.cg_skinEnemy;
		break;
	/*
	 * In other modes, consider blue to be the ally team if we are in spectator
	 */
	default:
		if ( (cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR && team == TEAM_BLUE) ||
		     (cgs.clientinfo[cg.clientNum].team == team) ) {
			selector = cg.cg_skinAlly;
		} else {
			selector = cg.cg_skinEnemy;
		}
		dft = (team == TEAM_BLUE) ? UT_SKIN_BLUE : UT_SKIN_RED;
		break;
	}

	if (cg.g_skins > 0) {
		switch (selector) {
            case 0: return dft;
            case 1: return UT_SKIN_GREEN;
            case 2: return UT_SKIN_RED;
            case 3: return UT_SKIN_BLUE;
            case 4: return UT_SKIN_PURPLE;
            case 5: return UT_SKIN_ORANGE;
            case 6: return UT_SKIN_OLIVE;
            case 7: return UT_SKIN_WHITE;
            case 8: return UT_SKIN_BLACK;
            case 9: return UT_SKIN_DESERT;
            case 10: return UT_SKIN_COWBOY;
            case 11: return UT_SKIN_CAVALRY;
            case 12: return UT_SKIN_DROOGS;
            default: return dft;
		}
	}
	else {
		return dft;
	}
}

/**
 * CG_CustomSound
 */
sfxHandle_t	CG_CustomSound(int clientNum, const char *soundName)
{
	clientInfo_t  *ci;
	int	      i;

	if (soundName[0] != '*') {
		return trap_S_RegisterSound(soundName, qfalse);
	}

	if ((clientNum < 0) || (clientNum >= MAX_CLIENTS)) {
		clientNum = 0;
	}
	ci = &cgs.clientinfo[clientNum];

	for (i = 0 ; i < MAX_CUSTOM_SOUNDS && cg_customSoundNames[i] ; i++) {
		if (!strcmp(soundName, cg_customSoundNames[i])) {
			return ci->sounds[i];
		}
	}

	CG_Printf("Unknown custom sound: %s", soundName);
	return 0;
}

/* only works in the case of an object placed on terrain (normal Z+) */
void AxisFromNormal(vec3_t normal, vec3_t axis[3], float yaw)
{
	vec3_t	forward;
	vec3_t	tempvec;

	VectorCopy(normal, axis[2]);
	tempvec[1] = tempvec[2] = 0.0f;
	tempvec[0] = 1.0f;

	ProjectPointOnPlane(axis[1], tempvec, axis[2]);
	VectorNormalizeNR(axis[1]);

	RotatePointAroundVector(forward, axis[2], axis[1], yaw + 90);
	VectorCopy(forward, axis[1]);
	CrossProduct(axis[1], axis[2], axis[0]);
}

/**
 * CG_RegisterClientSkin
 */
static qboolean CG_RegisterClientSkin(clientInfo_t *ci,
	const char *modelName, const char *skinName)
{
	//int i;
	char  filename[MAX_QPATH];

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/lower_%s.skin", modelName, skinName);
	ci->legsSkin = trap_R_RegisterSkin(filename);

	if (!ci->legsSkin) {
		Com_Printf("Leg skin load failure: %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/upper_%s.skin", modelName, skinName);
	ci->torsoSkin = trap_R_RegisterSkin(filename);

	if (!ci->torsoSkin) {
		Com_Printf("Torso skin load failure: %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/vest_%s.skin", modelName, skinName);
	ci->vesttorsoSkin = trap_R_RegisterSkin(filename);

	if (!ci->vesttorsoSkin) {
		Com_Printf("VestTorso skin load failure: %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/head_%s_.skin", modelName, skinName);
	ci->headSkin = trap_R_RegisterSkin(filename);

	if (!ci->headSkin) {
		Com_Printf("Head skin load failure: %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/vest_%s.skin", modelName, skinName);
	ci->vestSkin = trap_R_RegisterSkin(filename);

	if (!ci->vestSkin) {
		Com_Printf("Vest skin load failure: %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_%s.skin", modelName, skinName);
	ci->helmetSkin = trap_R_RegisterSkin(filename);

	if (!ci->helmetSkin) {
		Com_Printf("Helmet skin load failure: %s\n", filename);
	}

	/*
	 * Theres only going to be one of these depending on team,
	 * but no big deal that ones missing..
	 */
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/vest_red_leader.skin", modelName);
	ci->vestRedLeaderSkin  = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/vest_blue_leader.skin", modelName);
	ci->vestBlueLeaderSkin = trap_R_RegisterSkin(filename);

	/*
	 * Theres only going to be one of these depending on team,
	 * but no big deal that ones missing..
	 */
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_green_leader.skin", modelName);
	ci->helmetLeaderSkins[0] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_blue_leader.skin", modelName);
	ci->helmetLeaderSkins[1] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_red_leader.skin", modelName);
	ci->helmetLeaderSkins[2] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_purple_leader.skin", modelName);
	ci->helmetLeaderSkins[3] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_orange_leader.skin", modelName);
	ci->helmetLeaderSkins[4] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_olive_leader.skin", modelName);
	ci->helmetLeaderSkins[5] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_white_leader.skin", modelName);
	ci->helmetLeaderSkins[6] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_black_leader.skin", modelName);
	ci->helmetLeaderSkins[7] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_desert_leader.skin", modelName);
	ci->helmetLeaderSkins[8] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_cowboy_leader.skin", modelName);
	ci->helmetLeaderSkins[9] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_cavalry_leader.skin", modelName);
	ci->helmetLeaderSkins[10] = trap_R_RegisterSkin(filename);
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet_droogs_leader.skin", modelName);
	ci->helmetLeaderSkins[11] = trap_R_RegisterSkin(filename);

	/* NVG */
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/nvg_%s.skin", modelName, skinName);
	ci->nvgSkin = trap_R_RegisterSkin(filename);

	if (!ci->nvgSkin) {
		Com_Printf("NVG skin load failure: %s\n", filename);
	}

	return qtrue;
}

/**
 * CG_SetupPlayerGFX
 *
 * This is called to change the appearance of a character.
 * Seeing its al; already loaded, there is no hassle.
 */
void CG_SetupPlayerGFX(int clientNum, clientInfo_t *ci)
{
	clientInfo_t* gfxInfo;
	utSkins_t skin = CG_WhichSkin(ci->team);
	switch (skin) {
	case UT_SKIN_GREEN:
		gfxInfo = cgs.media.Free_Skins;
		break;
	case UT_SKIN_BLUE:
		gfxInfo = cgs.media.Blue_Skins;
		break;
	case UT_SKIN_RED:
		gfxInfo = cgs.media.Red_Skins;
		break;
	case UT_SKIN_PURPLE:
		gfxInfo = cgs.media.Jump_Skins;
		break;
	case UT_SKIN_ORANGE:
		gfxInfo = cgs.media.Orange_Skins;
		break;
	case UT_SKIN_OLIVE:
		gfxInfo = cgs.media.Olive_Skins;
		break;
	case UT_SKIN_WHITE:
		gfxInfo = cgs.media.White_Skins;
		break;
	case UT_SKIN_BLACK:
		gfxInfo = cgs.media.Black_Skins;
		break;
	case UT_SKIN_DESERT:
		gfxInfo = cgs.media.Desert_Skins;
		break;
	case UT_SKIN_COWBOY:
		gfxInfo = cgs.media.Cowboy_Skins;
		break;
	case UT_SKIN_CAVALRY:
		gfxInfo = cgs.media.Cavalry_Skins;
		break;
	case UT_SKIN_DROOGS:
		gfxInfo = cgs.media.Droogs_Skins;
		break;
	}

	CG_CopyClientInfoModel(&gfxInfo[ci->race], ci);
	cgs.clientinfo[clientNum].infoValid = qtrue;
}

/**
 * preloadAllClientData
 *
 * Preload all the client data
 */
void preloadAllClientData(void)
{
	CG_LoadPlayerModels();
}

//@r00t: On demand skin loading
#define CHECKANDLOADSKINS(where,namew,nameb) \
		if (where[0].infoValid) return; \
		CG_RegisterClientSkin(&where[0], "athena", namew);\
		CG_RegisterClientSkin(&where[1], "athena", nameb);\
		CG_RegisterClientSkin(&where[2], "orion", namew);\
		CG_RegisterClientSkin(&where[3], "orion", nameb);\
		where[0].skincolor = 0;\
		where[1].skincolor = 1;\
		where[2].skincolor = 0;\
		where[3].skincolor = 1;\
		where[0].infoValid = qtrue;

void CG_LoadSkinOnDemand(utSkins_t skin)
{
	switch (skin) {
	case UT_SKIN_GREEN:
		CHECKANDLOADSKINS(cgs.media.Free_Skins,"free_w","free_b");
		break;
	case UT_SKIN_BLUE:
		CHECKANDLOADSKINS(cgs.media.Blue_Skins,"swat_w","swat_b");
		break;
	case UT_SKIN_RED:
		CHECKANDLOADSKINS(cgs.media.Red_Skins,"tag_w","tag_b");
		break;
	case UT_SKIN_PURPLE:
		CHECKANDLOADSKINS(cgs.media.Jump_Skins,"jump_w","jump_b");
		break;
	case UT_SKIN_ORANGE:
		CHECKANDLOADSKINS(cgs.media.Orange_Skins,"orange_w","orange_b");
		break;
	case UT_SKIN_OLIVE:
		CHECKANDLOADSKINS(cgs.media.Olive_Skins,"olive_w","olive_b");
		break;
	case UT_SKIN_WHITE:
		CHECKANDLOADSKINS(cgs.media.White_Skins,"white_w","white_b");
		break;
	case UT_SKIN_BLACK:
		CHECKANDLOADSKINS(cgs.media.Black_Skins,"black_w","black_b");
		break;
	case UT_SKIN_DESERT:
		CHECKANDLOADSKINS(cgs.media.Desert_Skins,"desert_w","desert_b");
		break;
	case UT_SKIN_COWBOY:
		CHECKANDLOADSKINS(cgs.media.Cowboy_Skins,"cowboy_w","cowboy_b");
		break;
	case UT_SKIN_CAVALRY:
		CHECKANDLOADSKINS(cgs.media.Cavalry_Skins,"cavalry_w","cavalry_b");
		break;
	case UT_SKIN_DROOGS:
		CHECKANDLOADSKINS(cgs.media.Droogs_Skins,"droogs_w","droogs_b");
		break;
	}
}


/**
 * CG_RegisterClientModelname
 */
static qboolean CG_LoadPlayerModels(void)
{
	qboolean  val;
	int	  blueteam, redteam = 0;
	char	  filename[255];

	/*set us to the set the server says */
	//if (cgs.flags) {
		if (cgs.flags[0] == '1') {
			redteam = 1;
		}

		if (cgs.flags[0] == '2') {
			redteam = 2;
		}

		if (cgs.flags[0] == '3') {
			redteam = 3;
		}

		if (cgs.flags[1] == '1') {
			blueteam = 1;
		}

		if (cgs.flags[1] == '2') {
			blueteam = 2;
		}

		if (cgs.flags[1] == '3') {
			blueteam = 3;
		}
	//}
	redteam += 4; /*there are 4 sets of 4, so we skip the first 4 for red */

	/*Load the initial models and skins that are shared */
	cgs.media.Base_Orion.gender = GENDER_MALE;
	CG_LoadingString("Tom");
	val = CG_RegisterModel(&cgs.media.Base_Orion, "orion");

	if (val == qfalse) {
		return val;
	}

	CG_LoadingString("Sarah");
	cgs.media.Base_Athena.gender = GENDER_FEMALE;
	val = CG_RegisterModel(&cgs.media.Base_Athena, "athena");

	if (val == qfalse) {
		return val;
	}

	/*make copys of all the loaded stuff */
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Red_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Red_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Red_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Red_Skins[3]);
    cgs.media.Red_Skins[0].infoValid = qfalse;

	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Blue_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Blue_Skins[3]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Blue_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Blue_Skins[1]);
    cgs.media.Blue_Skins[0].infoValid = qfalse;

	//@Barbatos - FFA / LMS skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Free_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Free_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Free_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Free_Skins[3]);
    cgs.media.Free_Skins[0].infoValid = qfalse;

	//@Barbatos - Jump skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Jump_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Jump_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Jump_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Jump_Skins[3]);
    cgs.media.Jump_Skins[0].infoValid = qfalse;

	//@gsigms - extra Orange skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Orange_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Orange_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Orange_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Orange_Skins[3]);
    cgs.media.Orange_Skins[0].infoValid = qfalse;

	//@gsigms - extra Olive skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Olive_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Olive_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Olive_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Olive_Skins[3]);
    cgs.media.Olive_Skins[0].infoValid = qfalse;

	//@gsigms - extra White skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.White_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.White_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.White_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.White_Skins[3]);
    cgs.media.White_Skins[0].infoValid = qfalse;

	//@gsigms - extra Black skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Black_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Black_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Black_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Black_Skins[3]);
    cgs.media.Black_Skins[0].infoValid = qfalse;

	//@gsigms - extra Desert skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Desert_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Desert_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Desert_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Desert_Skins[3]);
    cgs.media.Desert_Skins[0].infoValid = qfalse;

	//@gsigms - extra Cowboy skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Cowboy_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Cowboy_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Cowboy_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Cowboy_Skins[3]);
    cgs.media.Cowboy_Skins[0].infoValid = qfalse;

	//@gsigms - extra Cavalry skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Cavalry_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Cavalry_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Cavalry_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Cavalry_Skins[3]);
    cgs.media.Cavalry_Skins[0].infoValid = qfalse;

	//@gsigms - extra Cavalry skins
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Droogs_Skins[0]);
	CG_CopyClientInfoModel(&cgs.media.Base_Athena, &cgs.media.Droogs_Skins[1]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Droogs_Skins[2]);
	CG_CopyClientInfoModel(&cgs.media.Base_Orion, &cgs.media.Droogs_Skins[3]);
    cgs.media.Droogs_Skins[0].infoValid = qfalse;
	
  /* Load the Relevant skins */

//@r00t: Will be loaded on demand later
#if 0
	CG_LoadingString("Red Dragon Skins");
	CG_RegisterClientSkin(&cgs.media.Red_Skins[0], "athena", "tag_w");
	cgs.media.Red_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Red_Skins[1], "athena", "tag_b");
	cgs.media.Red_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Red_Skins[2], "orion", "tag_w");
	cgs.media.Red_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Red_Skins[3], "orion", "tag_b");
	cgs.media.Red_Skins[3].skincolor = 1;

	CG_LoadingString("SWAT Skins");
	CG_RegisterClientSkin(&cgs.media.Blue_Skins[0], "athena", "swat_w");
	cgs.media.Blue_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Blue_Skins[1], "athena", "swat_b");
	cgs.media.Blue_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Blue_Skins[2], "orion", "swat_w");
	cgs.media.Blue_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Blue_Skins[3], "orion", "swat_b");
	cgs.media.Blue_Skins[3].skincolor = 1;

	//@Barbatos - free team skins
	CG_LoadingString("FreeTeam Skins");
	CG_RegisterClientSkin(&cgs.media.Free_Skins[0], "athena", "free_w");
	cgs.media.Free_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Free_Skins[1], "athena", "free_b");
	cgs.media.Free_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Free_Skins[2], "orion", "free_w");
	cgs.media.Free_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Free_Skins[3], "orion", "free_b");
	cgs.media.Free_Skins[3].skincolor = 1;

	//@Barbatos - jump skins
	CG_LoadingString("Jump Skins");
	CG_RegisterClientSkin(&cgs.media.Jump_Skins[0], "athena", "jump_w");
	cgs.media.Jump_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Jump_Skins[1], "athena", "jump_b");
	cgs.media.Jump_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Jump_Skins[2], "orion", "jump_w");
	cgs.media.Jump_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Jump_Skins[3], "orion", "jump_b");
	cgs.media.Jump_Skins[3].skincolor = 1;

	CG_LoadingString("Orange Skins");
	CG_RegisterClientSkin(&cgs.media.Orange_Skins[0], "athena", "orange_w");
	cgs.media.Orange_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Orange_Skins[1], "athena", "orange_b");
	cgs.media.Orange_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Orange_Skins[2], "orion", "orange_w");
	cgs.media.Orange_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Orange_Skins[3], "orion", "orange_b");
	cgs.media.Orange_Skins[3].skincolor = 1;

	CG_LoadingString("Olive Skins");
	CG_RegisterClientSkin(&cgs.media.Olive_Skins[0], "athena", "olive_w");
	cgs.media.Olive_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Olive_Skins[1], "athena", "olive_b");
	cgs.media.Olive_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Olive_Skins[2], "orion", "olive_w");
	cgs.media.Olive_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Olive_Skins[3], "orion", "olive_b");
	cgs.media.Olive_Skins[3].skincolor = 1;

	CG_LoadingString("White Skins");
	CG_RegisterClientSkin(&cgs.media.White_Skins[0], "athena", "white_w");
	cgs.media.White_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.White_Skins[1], "athena", "white_b");
	cgs.media.White_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.White_Skins[2], "orion", "white_w");
	cgs.media.White_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.White_Skins[3], "orion", "white_b");
	cgs.media.White_Skins[3].skincolor = 1;

	CG_LoadingString("Black Skins");
	CG_RegisterClientSkin(&cgs.media.Black_Skins[0], "athena", "black_w");
	cgs.media.Black_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Black_Skins[1], "athena", "black_b");
	cgs.media.Black_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Black_Skins[2], "orion", "black_w");
	cgs.media.Black_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Black_Skins[3], "orion", "black_b");
	cgs.media.Black_Skins[3].skincolor = 1;

	CG_LoadingString("Desert Skins");
	CG_RegisterClientSkin(&cgs.media.Desert_Skins[0], "athena", "desert_w");
	cgs.media.Desert_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Desert_Skins[1], "athena", "desert_b");
	cgs.media.Desert_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Desert_Skins[2], "orion", "desert_w");
	cgs.media.Desert_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Desert_Skins[3], "orion", "desert_b");
	cgs.media.Desert_Skins[3].skincolor = 1;

	CG_LoadingString("Cowboy Skins");
	CG_RegisterClientSkin(&cgs.media.Cowboy_Skins[0], "athena", "cowboy_w");
	cgs.media.Cowboy_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Cowboy_Skins[1], "athena", "cowboy_b");
	cgs.media.Cowboy_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Cowboy_Skins[2], "orion", "cowboy_w");
	cgs.media.Cowboy_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Cowboy_Skins[3], "orion", "cowboy_b");
	cgs.media.Cowboy_Skins[3].skincolor = 1;

	CG_LoadingString("Cavalry Skins");
	CG_RegisterClientSkin(&cgs.media.Cavalry_Skins[0], "athena", "cavalry_w");
	cgs.media.Cavalry_Skins[0].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Cavalry_Skins[1], "athena", "cavalry_b");
	cgs.media.Cavalry_Skins[1].skincolor = 1;
	CG_RegisterClientSkin(&cgs.media.Cavalry_Skins[2], "orion", "cavalry_w");
	cgs.media.Cavalry_Skins[2].skincolor = 0;
	CG_RegisterClientSkin(&cgs.media.Cavalry_Skins[3], "orion", "cavalry_b");
	cgs.media.Cavalry_Skins[3].skincolor = 1;
#endif

	/*Load the hand skins */
	CG_LoadingString("Hand Skins");

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[1]);
	cgs.media.Red_HandSkin = 0;
	cgs.media.Red_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[0]);
	cgs.media.Blue_HandSkin = 0;
	cgs.media.Blue_HandSkin = trap_R_RegisterSkin(filename);

	// @Barbatos - free team hand skins
	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[2]);
	cgs.media.Free_HandSkin = 0;
	cgs.media.Free_HandSkin = trap_R_RegisterSkin(filename);

	//@Barbatos - jump mode hand skins
	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[3]);
	cgs.media.Jump_HandSkin = 0;
	cgs.media.Jump_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[4]);
	cgs.media.Orange_HandSkin = 0;
	cgs.media.Orange_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[5]);
	cgs.media.Olive_HandSkin = 0;
	cgs.media.Olive_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[6]);
	cgs.media.White_HandSkin = 0;
	cgs.media.White_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[7]);
	cgs.media.Black_HandSkin = 0;
	cgs.media.Black_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[8]);
	cgs.media.Desert_HandSkin = 0;
	cgs.media.Desert_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[9]);
	cgs.media.Cowboy_HandSkin = 0;
	cgs.media.Cowboy_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[10]);
	cgs.media.Cavalry_HandSkin = 0;
	cgs.media.Cavalry_HandSkin = trap_R_RegisterSkin(filename);

	strcpy(filename, "models/weapons2/handskins/");
	strcat(filename, bg_handSkins[11]);
	cgs.media.Droogs_HandSkin = 0;
	cgs.media.Droogs_HandSkin = trap_R_RegisterSkin(filename);
	return val;
}

/**
 * CG_RegisterModel
 */
static qboolean CG_RegisterModel(clientInfo_t *ci, const char *modelName)
{
	char  filename[MAX_QPATH];
	int   i;
	char  *s;

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/lower.md3", modelName);
	ci->legsModel = trap_R_RegisterModel(filename);

	if (!ci->legsModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/upper.md3", modelName);
	ci->torsoModel = trap_R_RegisterModel(filename);

	if (!ci->torsoModel) {
		Com_Printf("Failed to load model file %s\n", filename);

		/* try torso */
		Com_sprintf(filename, sizeof(filename),
			"models/players/%s/torso.md3", modelName);
		ci->torsoModel = trap_R_RegisterModel(filename);

		if (!ci->torsoModel) {
			Com_Printf("Failed to load model file %s\n", filename);
		}
	}

	Com_sprintf(filename, sizeof(filename),
			"models/players/%s/vesttorso.md3", modelName);
	ci->vesttorsoModel = trap_R_RegisterModel(filename);

	if (!ci->vesttorsoModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/vest.md3", modelName);
	ci->vestModel = trap_R_RegisterModel(filename);

	if (!ci->vestModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/helmet.md3", modelName);
	ci->helmetModel = trap_R_RegisterModel(filename);

	if (!ci->helmetModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/nvg.md3", modelName);
	ci->nvgModel = trap_R_RegisterModel(filename);

	if (!ci->nvgModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/head.md3", modelName);
	ci->headModel = trap_R_RegisterModel(filename);

	if (!ci->headModel) {
		Com_Printf("Failed to load model file %s\n", filename);
	}

	/* Parse the animation file. */
	Com_sprintf(filename, sizeof(filename),
		"models/players/%s/animation.cfg", modelName);
	C_ParsePlayerAnimations(filename, ci->animations);

	/* Load the damage skins */
	for (i = 0; i < 6; i++) {
		ci->torsoDamageSkin[i] =
			trap_R_RegisterSkin(
				va("models/players/%s/upper_damage_%d.skin",
				modelName, i+1));
	}

	for (i = 0; i < 3; i++) {
		ci->legsDamageSkin[i] =
			trap_R_RegisterSkin(
				va("models/players/%s/lower_damage_%d.skin",
					modelName, i+1));
	}
	ci->headDamageSkin =
		trap_R_RegisterSkin(va("models/players/%s/head_damage.skin",
					modelName));

	/* Sounds */
	if (ci->gender == GENDER_FEMALE) {
		for (i = 0 ; i < MAX_CUSTOM_SOUNDS ; i++) {
			s = cg_customSoundNames[i];

			if (!s) {
				break;
			}
			ci->sounds[i] = 0;
			ci->sounds[i] = trap_S_RegisterSound(
				va("sound/player/female/%s", s + 1), qfalse);
		}
	} else {
		for (i = 0 ; i < MAX_CUSTOM_SOUNDS ; i++) {
			s = cg_customSoundNames[i];

			if (!s) {
				break;
			}
			ci->sounds[i] = 0;
			ci->sounds[i] = trap_S_RegisterSound(
				va("sound/player/male/%s", s + 1), qfalse);
		}
	}

	return qtrue;
}

/**
 * CG_ColorFromString
 */
static void CG_ColorFromString(const char *v, vec3_t color)
{
	int  val;

	VectorClear(color);

	val = atoi(v);

	if ((val < 1) || (val > 7)) {
		VectorSet(color, 1, 1, 1);
		return;
	}

	if (val & 1) {
		color[2] = 1.0f;
	}

	if (val & 2) {
		color[1] = 1.0f;
	}

	if (val & 4) {
		color[0] = 1.0f;
	}
}

/**
 * $(function)
 */
void CG_ReloadHandSkins(int clientNum)
{
	qhandle_t  skin;
	int        weapon;

	switch (CG_WhichSkin(cgs.clientinfo[clientNum].team)) {
	case UT_SKIN_GREEN:
		skin = cgs.media.Free_HandSkin;
		break;
	case UT_SKIN_BLUE:
		skin = cgs.media.Blue_HandSkin;
		break;
	case UT_SKIN_RED:
		skin = cgs.media.Red_HandSkin;
		break;
	case UT_SKIN_PURPLE:
		skin = cgs.media.Jump_HandSkin;
		break;
	case UT_SKIN_ORANGE:
		skin = cgs.media.Orange_HandSkin;
		break;
	case UT_SKIN_OLIVE:
		skin = cgs.media.Olive_HandSkin;
		break;
	case UT_SKIN_WHITE:
		skin = cgs.media.White_HandSkin;
		break;
	case UT_SKIN_BLACK:
		skin = cgs.media.Black_HandSkin;
		break;
	case UT_SKIN_DESERT:
		skin = cgs.media.Desert_HandSkin;
		break;
	case UT_SKIN_COWBOY:
		skin = cgs.media.Cowboy_HandSkin;
		break;
	case UT_SKIN_CAVALRY:
		skin = cgs.media.Cavalry_HandSkin;
		break;
	case UT_SKIN_DROOGS:
		skin = cgs.media.Droogs_HandSkin;
		break;
	}

	/* Spin through the guns, and assign the proper skin to each */
	for (weapon = UT_WP_NONE + 1; weapon < UT_WP_NUM_WEAPONS; weapon++) {
		cg_weapons[weapon].viewSkin = skin;
	}
}

/**
 * CG_CopyClientInfoModel
 */
static void CG_CopyClientInfoModel(clientInfo_t *from, clientInfo_t *to)
{
	int  n;

	to->footsteps		 = from->footsteps;
	to->gender		 = from->gender;
	to->skincolor		 = from->skincolor;

	to->legsModel		 = from->legsModel;
	to->legsSkin		 = from->legsSkin;
	to->torsoModel		 = from->torsoModel;
	to->torsoSkin		 = from->torsoSkin;
	to->vesttorsoModel	 = from->vesttorsoModel;
	to->vesttorsoSkin	 = from->vesttorsoSkin;

	to->headModel		 = from->headModel;
	to->headSkin		 = from->headSkin;
	to->vestModel		 = from->vestModel;
	to->vestSkin		 = from->vestSkin;
	to->vestBlueLeaderSkin	 = from->vestBlueLeaderSkin;
	to->vestRedLeaderSkin	 = from->vestRedLeaderSkin;
	to->nvgModel		 = from->nvgModel;
	to->nvgSkin		 = from->nvgSkin;
	to->helmetModel 	 = from->helmetModel;
	to->helmetSkin		 = from->helmetSkin;
	for ( n = 0; n < UT_SKIN_COUNT; ++n ) {
		to->helmetLeaderSkins[n] = from->helmetLeaderSkins[n];
	}
	to->ariesModel		 = from->ariesModel;

	to->headDamageSkin	 = from->headDamageSkin;
	to->legsDamageSkin[0]	 = from->legsDamageSkin[0];
	to->legsDamageSkin[1]	 = from->legsDamageSkin[1];
	to->legsDamageSkin[2]	 = from->legsDamageSkin[2];
	to->torsoDamageSkin[0]	 = from->torsoDamageSkin[0];
	to->torsoDamageSkin[1]	 = from->torsoDamageSkin[1];
	to->torsoDamageSkin[2]	 = from->torsoDamageSkin[2];
	to->torsoDamageSkin[3]	 = from->torsoDamageSkin[3];
	to->torsoDamageSkin[4]	 = from->torsoDamageSkin[4];
	to->torsoDamageSkin[5]	 = from->torsoDamageSkin[5];

// gsigms: don t mess with funstuff when changing skins
//	for (n = 0; n < 3; n++) {
//		to->funmodel[n]    = from->funmodel[n];
//		to->funmodeltag[n] = from->funmodeltag[n];
//	}

	memcpy(to->animations, from->animations, sizeof(to->animations));
	memcpy(to->sounds, from->sounds, sizeof(to->sounds));
	/* CG_ReloadHandSkins ( clientNum, to ); */
}

/*
======================
CG_NewClientInfo
======================
*/
void CG_NewClientInfo(int clientNum)
{
	clientInfo_t  *ci;
	const char    *configstring;
	const char    *v;
	char	      t[128];
	char	      colors[64];
	char	      path[128];
	int	      r, q;
	int	      i;

	ci	     = &cgs.clientinfo[clientNum];

	configstring = CG_ConfigString(clientNum + CS_PLAYERS);

	if (!configstring[0]) {
		memset(ci, 0, sizeof(*ci));
		return; /* player just left */
	}

	/* isolate the player's name */
	v = Info_ValueForKey(configstring, "n");
	Q_strncpyz(ci->name, v, sizeof(ci->name));

	/* colors */
	v = Info_ValueForKey(configstring, "c1");
	CG_ColorFromString(v, ci->color);

	/* bot skill */
	v	     = Info_ValueForKey(configstring, "skill");
	ci->botSkill = atoi(v);

	/* wins */
	v	 = Info_ValueForKey(configstring, "w");
	ci->wins = atoi(v);

	/* losses */
	v	   = Info_ValueForKey(configstring, "l");
	ci->losses = atoi(v);

	/* team */
	v	 = Info_ValueForKey(configstring, "t");
	ci->team = atoi(v);

	if (clientNum == cg.clientNum) {
		/* If this is us set the team */
		trap_Cvar_Set("ui_team", v);
		cg.predictedPlayerState.persistant[PERS_TEAM] = ci->team;
	}

	/* team task */
	v	     = Info_ValueForKey(configstring, "tt");
	ci->teamTask = atoi(v);

	/* team leader */
	v	       = Info_ValueForKey(configstring, "tl");
	ci->teamLeader = atoi(v);

	/* Fun stuff */
	for (i = 0; i < 3; i++) {
		Com_sprintf(t, sizeof(t), "f%d", i);
		v = Info_ValueForKey(configstring, t);
		strcpy(ci->funstuff[i], v);
	}

	/* model */
	v	      = Info_ValueForKey(configstring, "r");

	ci->gender    = GENDER_FEMALE;
	ci->skincolor = SKINCOLOR_WHITE;
	ci->race      = 0;

	switch (v[0]) {
	case '0':
		ci->race      = 0;
		ci->gender    = GENDER_FEMALE;
		ci->skincolor = SKINCOLOR_WHITE;
		break;
	case '1':
		ci->race      = 1;
		ci->gender    = GENDER_FEMALE;
		ci->skincolor = SKINCOLOR_BLACK;
		break;
	case '2':
		ci->race      = 2;
		ci->gender    = GENDER_MALE;
		ci->skincolor = SKINCOLOR_WHITE;
		break;
	case '3':
		ci->race      = 3;
		ci->gender    = GENDER_MALE;
		ci->skincolor = SKINCOLOR_BLACK;
		break;
	}


	if (clientNum == cg.clientNum) {
        // need to check/reload in case we were using default skins
        // for only one team
		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_RED));
		CG_LoadSkinOnDemand(CG_WhichSkin(TEAM_BLUE));
    }

	/* Just make sure we're loaded right */
	CG_SetupPlayerGFX(clientNum, ci);
	trap_Cvar_Set("ui_gender",
		ci->gender == GENDER_FEMALE ? "female" : "male");
	CG_ReloadHandSkins(clientNum); /*set the handskins */

	/*
	 * Register the funstuff model
	 * Annoying limitation, we can't just enum the folder,
	 * but there are a finite number of allowed combinations so
	 * we'll just run with that.
	 */
	for (i = 0; i < 3; i++) {
		if (strlen(ci->funstuff[i]) == 0) {
			ci->funmodel[i]    = 0;
			ci->funmodeltag[i] = 0;
		}
	}

	Com_sprintf(path, sizeof(path), "models/players/funstuff/");

	if (ci->team == 2) {
		Com_sprintf(colors, sizeof(colors), "b\0");
	} else {
		Com_sprintf(colors, sizeof(colors), "r\0");
	}

	for (q = 0; q < 3; q++) {
		for (r = 0; r < 3; r++) {
			Com_sprintf(t, sizeof(t), "%s%s_%s_%i_%i.md3",
				path, ci->funstuff[q], colors,
				ci->race, r);
			if ((ci->funmodel[q] = trap_R_RegisterModel(t))) {
				// no helmet
				ci->funmodeltag[q] = r;
				break;
			}
		}
	}

	// arm bands
	for (i = 0; i < 3; i++) {
		Com_sprintf(t, sizeof(t), "a%d", i);
		v = Info_ValueForKey(configstring, t);
		ci->armbandRGB[i] = atof(v);
	}

	if (clientNum == cg.clientNum) {
		// we are the one switching team
		// -> force update everyone else gfx accordingly
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cgs.clientinfo[i].infoValid
			&& clientNum != i)
				CG_SetupPlayerGFX(i, &cgs.clientinfo[i]);
		}
	}
}

/**
 * CG_PlayerAngles
 *
 * Handles separate torso motion
 *
 * legs pivot based on direction of movement
 *
 * head always looks exactly at cent->lerpAnge
 *
 * if motion < 20 degrees, show in head only
 * if < 45 degrees, also how n torso
 */
static void CG_PlayerAngles(centity_t *cent, vec3_t legs[3],
	vec3_t torso[3], vec3_t head[3])
{
	int  pryFlags;

	/* D8: calculate angles with this terrible function */
	BG_PlayerAngles(cent->lerpAngles,
			cent->currentState.pos.trDelta,
			cent->currentState.legsAnim,
			cent->currentState.torsoAnim,
			cg.frametime,
			&cent->pe.adata,
			&cent->pe.legs.pitchAngle,
			&cent->pe.legs.yawAngle,
			&cent->pe.torso.pitchAngle,
			&cent->pe.torso.yawAngle,
			&pryFlags);

	/* CG_Printf("cg.frametime: %d\n", cg.frametime ); */

	AnglesToAxis(cent->pe.adata.legAngles, legs);
	AnglesToAxis(cent->pe.adata.torsoAngles, torso);
	AnglesToAxis(cent->pe.adata.headAngles, head);

	/*
	Com_Printf("CG adata: {%f %f %f} {%f %f %f} {%f %f %f}\n",
			cent->pe.adata.legAngles[0],
			cent->pe.adata.legAngles[1],
			cent->pe.adata.legAngles[2],
			cent->pe.adata.torsoAngles[0],
			cent->pe.adata.torsoAngles[1],
			cent->pe.adata.torsoAngles[2],
			cent->pe.adata.headAngles[0],
			cent->pe.adata.headAngles[1],
			cent->pe.adata.headAngles[2]);
	*/

	/* store the stuff that cgame needs */
	cent->pe.legs.yawing	= pryFlags & 4;
	cent->pe.torso.yawing	= pryFlags & 2;
	cent->pe.torso.pitching = pryFlags & 1;
}

/**
 * CG_BreathPuffs
 */
static void CG_BreathPuffs(centity_t *cent, refEntity_t *head)
{
	clientInfo_t  *ci;
	vec3_t	      up, origin;
	int	      contents;
	int	      breathLevel;

	ci = &cgs.clientinfo[cent->currentState.number];

	/* Check to see if breathing is off */
	if (!cg_sfxBreathing.integer) {
		return;
	}

	if (cent->currentState.eFlags & EF_DEAD) {
		return;
	}

	/* Dont break until we have to */
	if (cg.time < ci->breathPuffTime) {
		return;
	}

	//@Barbatos: no breathing when paused
	if (cg.pauseState & UT_PAUSE_ON)
	{
		return;
	}

	/* No breathing underwater. */
	contents = trap_CM_PointContents(head->origin, 0);

	if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
		return;
	}

	breathLevel = (cent->currentState.time2 >> 24) / 5;

	if (breathLevel > 15) {
		breathLevel = 15;
	}

	/* TEMP */
	if (breathLevel < 11) {
		breathLevel = 0;
	} else {
		breathLevel = 1;
	}

	if (!ci->breathPuffType) {
		/* Make the sound */
		if (breathLevel > 0) {
			trap_S_StartSound(NULL, cent->currentState.clientNum,
				  CHAN_AUTO,
				  CG_CustomSound(cent->currentState.clientNum,
						 "*breathin1.wav"));
		ci->breathPuffType = 1;
		} else {
			ci->breathPuffType = 2;
		}

		ci->breathPuffTime = cg.time + 500 + 1000 * (100 -
				     (cent->currentState.time2 >> 24)) / 100;
	} else {
		if (ci->breathPuffType == 1) {
			trap_S_StartSound(NULL,
				  cent->currentState.clientNum, CHAN_AUTO,
				  CG_CustomSound(cent->currentState.clientNum,
						 "*breathout1.wav"));
		}

		ci->breathPuffTime = cg.time + 500 + 500 * (100 -
				     (cent->currentState.time2 >> 24)) / 100;
		ci->breathPuffType = 0;

		if (!cg_enableBreath.integer) {
			return;
		}

		if ((cent->currentState.number == cg.snap->ps.clientNum) &&
				!cg.renderingThirdPerson) {
			return;
		}

		VectorSet(up, 0, 0, 8);
		VectorMA(head->origin, 8, head->axis[0], origin);
		VectorMA(origin, -4, head->axis[2], origin);
		CG_SmokePuff(origin, up, 8, 1, 1, 1, 0.66f, 1500, cg.time,
			     cg.time + 400, LEF_PUFF_DONT_SCALE,
			     cgs.media.shotgunSmokePuffShader);
	}
}

/**
 * CG_DustTrail
 */
static void CG_DustTrail(centity_t *cent)
{
	int	 anim;
	vec3_t	 end, vel;
	trace_t  tr;

	if (!cg_enableDust.integer) {
		return;
	}

	if (cent->miscTime > cg.time) {
		return;
	}

	anim = cent->pe.legs.animationNumber & ~ANIM_TOGGLEBIT;

	if ((anim != LEGS_BACKLAND) && (anim != LEGS_LAND)) {
		return;
	}

	cent->miscTime += 40;

	if (cent->miscTime < cg.time) {
		cent->miscTime = cg.time;
	}

	VectorCopy(cent->currentState.pos.trBase, end);
	end[2] -= 64;
	CG_Trace(&tr, cent->currentState.pos.trBase, NULL, NULL, end,
		 cent->currentState.number, MASK_PLAYERSOLID);

	if (!(tr.surfaceFlags & SURF_DUST)) {
		return;
	}

	VectorCopy(cent->currentState.pos.trBase, end);
	end[2] -= 16;

	VectorSet(vel, 0, 0, -30);

	CG_SmokePuff(end, vel,
		     24,
		     .8f, .8f, 0.7f, 0.33f,
		     500,
		     cg.time,
		     0,
		     0,
		     cgs.media.shotgunSmokePuffShader);
}


#if 0
/**
 * CG_TrailItem
 */
static void CG_TrailItem(centity_t *cent, qhandle_t hModel)
{
	refEntity_t  ent;
	vec3_t	     angles;
	vec3_t	     axis[3];

	VectorCopy(cent->lerpAngles, angles);
	angles[PITCH] = 0;
	angles[ROLL]  = 0;
	AnglesToAxis(angles, axis);

	memset(&ent, 0, sizeof(ent));
	VectorMA(cent->lerpOrigin, -16, axis[0], ent.origin);
	ent.origin[2] += 16;
	angles[YAW]   += 90;
	AnglesToAxis(angles, ent.axis);

	ent.hModel = hModel;
	trap_R_AddRefEntityToSceneX(&ent);
}
#endif


/**
 * CG_PlayerFloatSprite
 *
 * Float a sprite over the player's head
 */
static void CG_PlayerFloatSprite(centity_t *cent, qhandle_t shader)
{
	int	     rf;
	refEntity_t  ent;
	float	     myfloat;

	/* only show in mirrors */
	if ((cent->currentState.number == cg.snap->ps.clientNum) &&
			!cg.renderingThirdPerson) {
		rf = RF_THIRD_PERSON;
	} else {
		rf = 0;
	}

	memset(&ent, 0, sizeof(ent));
	VectorCopy(cent->lerpOrigin, ent.origin);
	ent.origin[2]	 += 48;
	ent.reType	  = RT_SPRITE;
	ent.customShader  = shader;
	ent.radius	  = 10;
	ent.renderfx	  = rf;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;

	/* medic icon */
	if ((ent.customShader == cgs.media.medicshader) ||
			(ent.customShader == cgs.media.plantingShader) ||
			(ent.customShader == cgs.media.defusingShader)) {
		/*
		 * offset the heart icon to one side of the player relative
		 * to current view
		 * VectorMA(ent.origin, -8, cg.refdef.viewaxis[1], ent.origin);
		 */
		myfloat    = cg.time;
		ent.radius = 7 - (sin(myfloat * 0.01) * 0.5);
	}

	trap_R_AddRefEntityToSceneX(&ent);
}

/**
 * CG_PlayerSprites
 *
 * Float sprites over the player's head
 */
static void CG_PlayerSprites(centity_t *cent)
{
	int  team;

	team = cgs.clientinfo[cg.predictedPlayerState.clientNum].team;

	/*
	 * medic icon: player wants to be healed
	 * should only be displayed after a player has issued a "medic!" call
	 * on the radio and should only be visible to their team-mates
	 * uses the "Impressive" flag of entity flags
	 * EF_UT_MEDIC gets set when the radio command arrives at the server
	 * and gets cleared on the server when a medic heals them
	 */
	if (cent->currentState.eFlags & EF_UT_MEDIC) {
		/* is the player on the same team as the current client? */
		if ((team == TEAM_SPECTATOR) ||
			(cgs.clientinfo[cent->currentState.clientNum]
			 .team == team) ||
			(cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) ||
			(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] &
			 UT_PMF_GHOST)) {
			CG_PlayerFloatSprite(cent, cgs.media.medicshader);
		}

		return;
	}

	/* If planting flag is set. */
	if (cent->currentState.eFlags & EF_UT_PLANTING) {
		/* If we're on this dudes team. */
		if ((team == TEAM_SPECTATOR) ||
			(cgs.clientinfo[cent->currentState.clientNum]
			 .team == team) ||
			(cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) ||
			(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] &
			 UT_PMF_GHOST)) {
			CG_PlayerFloatSprite(cent, cgs.media.plantingShader);
		}

		return;
	}

	/* If defusing flag is set. */
	if (cent->currentState.eFlags & EF_UT_DEFUSING) {
		/* If we're on this dudes team. */
		if ((team == TEAM_SPECTATOR) ||
			(cgs.clientinfo[cent->currentState.clientNum]
			 .team == team) ||
			(cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) ||
			(cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] &
			 UT_PMF_GHOST)) {
			CG_PlayerFloatSprite(cent, cgs.media.defusingShader);
		}

		return;
	}
}

/**
 * CG_PlayerBleeding
 *
 * Draw player bleeding
 */
static void CG_PlayerBleeding(centity_t *cent)
{
	clientInfo_t   *ci = &cgs.clientinfo[cent->currentState.clientNum];
	int	       bleed;
	localEntity_t  *le;

	/* Bomb if we dont want to draw blood */
	if (cg_blood.integer <= 0) {
		return;
	}

	/* dead men dont bleed. */
	if (cent->currentState.eFlags & EF_DEAD) {
		return;
	}

	for (bleed = 0; bleed < ci->bleedCount; bleed++) {
		vec3_t	point;
		vec3_t	from   = { 0, 0, 10};
		vec3_t	normal = { 0, 0, 1};

		if (cg.time > ci->bleedTime[bleed]) {
			le = CG_ParticleCreate();

			VectorCopy(cent->lerpOrigin, point);
			CG_ParticleSetAttributes(le, PART_ATTR_BLOODTRICKLE,
				 cgs.media.bloodExplosionShader,
				 cgs.media.bloodMarkShader[rand() % 4], 0);

			CG_ParticleExplode(point, from, normal,
					   1 + (rand() % 1), 40.0f, le);

			if (!(cent->currentState.powerups & (1 << PW_BLEEDING)))
				ci->bleedCount--;

			while (cg.time > ci->bleedTime[bleed]) {
				ci->bleedTime[bleed] += (150 + rand() % 150);
			}
		}
	}
}

#define SHADOW_DISTANCE 128

/**
 * CG_PlayerShadow
 *
 * Returns the Z component of the surface being shadowed.
 * Should it return a full plane instead of Z?
 */
static qboolean CG_PlayerShadow(centity_t *cent, float *shadowPlane)
{
	vec3_t	 end;
	trace_t  trace;
	float	 alpha;

	*shadowPlane = 0;

	/* send a trace down from the player to the ground */
	VectorCopy(cent->lerpOrigin, end);
	end[2] -= SHADOW_DISTANCE;

	trap_CM_BoxTrace(&trace, cent->lerpOrigin, end, NULL, NULL, 0,
			MASK_PLAYERSOLID);

	/* no shadow if too high */
	if ((trace.fraction == 1.0) || trace.startsolid || trace.allsolid) {
		return qfalse;
	}

	*shadowPlane = trace.endpos[2] + 1;

	/* fade the shadow out with height */
	alpha = 1.0 - trace.fraction;

	/* add the mark as a temporary, so it goes directly to the renderer */
	/* without taking a spot in the cg_marks array */
	CG_ImpactMark(cgs.media.shadowMarkShader, trace.endpos,
		trace.plane.normal,
		cent->pe.legs.yawAngle, alpha, alpha, alpha, 1,
		qfalse, 18, qtrue, NULL);

	return qtrue;
}

/**
 * CG_PlayerSplash
 *
 * Draw a mark at the water surface
 */
static void CG_PlayerSplash(centity_t *cent)
{
	vec3_t	    start, end;
	trace_t     trace;
	int	    contents;
	polyVert_t  verts[4];

	if (!cg_shadows.integer) {
		return;
	}

	VectorCopy(cent->lerpOrigin, end);
	end[2] -= 24;

	/*
	 * if the feet aren't in liquid, don't make a mark
	 * this won't handle moving water brushes, but they wouldn't draw
	 * right anyway...
	 */
	contents = trap_CM_PointContents(end, 0);

	if (!(contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))) {
		return;
	}

	VectorCopy(cent->lerpOrigin, start);
	start[2] += 32;

	/* if the head isn't out of liquid, don't make a mark */
	contents = trap_CM_PointContents(start, 0);

	if (contents & (CONTENTS_SOLID | CONTENTS_WATER |
			CONTENTS_SLIME | CONTENTS_LAVA)) {
		/*
		 * Provides a convenient opportunity to play the
		 * underwater sound tho
		 */
		if ((cent->currentState.clientNum ==
				cg.predictedPlayerState.clientNum) &&
				(cg.playingWaterSFX == qfalse)) {
			cg.playingWaterSFX = qtrue;
			trap_S_AddRealLoopingSound(
				cg.predictedPlayerState.clientNum,
				cent->lerpOrigin, vec3_origin,
				cgs.media.sndUnderWater);
		}

		return;
	}

	/*
	 * Provides a convenient opportunity to stop playing
	 * the underwater sound
	 */
	if ((cent->currentState.clientNum ==
		cg.predictedPlayerState.clientNum) &&
			(cg.playingWaterSFX == qtrue)) {
		/* stop underwater sound */
		cg.playingWaterSFX = qfalse;
		trap_S_StopLoopingSound(cg.predictedPlayerState.clientNum);
	}

	/* trace down to find the surface */
	trap_CM_BoxTrace(&trace, start, end, NULL, NULL, 0,
			(CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA));

	if (trace.fraction == 1.0) {
		return;
	}

	/* create a mark polygon */
	VectorCopy(trace.endpos, verts[0].xyz);
	verts[0].xyz[0]     -= 32;
	verts[0].xyz[1]     -= 32;
	verts[0].st[0]	     = 0;
	verts[0].st[1]	     = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorCopy(trace.endpos, verts[1].xyz);
	verts[1].xyz[0]     -= 32;
	verts[1].xyz[1]     += 32;
	verts[1].st[0]	     = 0;
	verts[1].st[1]	     = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorCopy(trace.endpos, verts[2].xyz);
	verts[2].xyz[0]     += 32;
	verts[2].xyz[1]     += 32;
	verts[2].st[0]	     = 1;
	verts[2].st[1]	     = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorCopy(trace.endpos, verts[3].xyz);
	verts[3].xyz[0]     += 32;
	verts[3].xyz[1]     -= 32;
	verts[3].st[0]	     = 1;
	verts[3].st[1]	     = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene(cgs.media.wakeMarkShader, 4, verts);
}

/**
 * CG_AddRefEntityWithNVGGlow
 *
 * Adds a piece with modifications or duplications for powerups
 * Also called by CG_Missile for quad rockets, but nobody can tell...
 */
void CG_AddRefEntityWithNVGGlow(refEntity_t *ent, entityState_t *state)
{
	float  speed;

	trap_R_AddRefEntityToSceneX(ent);

	/* Add the NVG glow if this client has NVGs */
	if (CG_ENTITIES(cg.snap->ps.clientNum)->currentState.powerups &
			POWERUP(PW_NVG) && !cg_thirdPerson.integer &&
			!(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST) &&
			!(cg.snap->ps.pm_flags & PMF_FOLLOW) &&
			!(cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) &&
			!(cg.snap->ps.pm_flags & PMF_FOLLOW)) {

		if (!state || ((state->eType == ET_PLAYER) &&
				!(state->eFlags & EF_DEAD) &&
				cgs.clientinfo[CG_ENTITIES(state->number)->
					        currentState.clientNum].nvgVisible)) {
			speed = 800.0f;
		} else {
			speed = VectorLength(state->pos.trDelta);
		}

		if (speed > 800.0f) {
			speed = 800.0f;
		}

		if (speed < 50.0f) {
			speed = 50.0f;
		}

		if ((cg_nvg.integer == 1) || (cg_nvg.integer == 2)) {
			ent->customShader = cgs.media.nvgBrightB;
		} else {
			ent->customShader = cgs.media.nvgBrightA;
		}

		ent->shaderRGBA[0] = (unsigned char)(255.0f * (1.0f - (speed / 800.0f)));
		ent->shaderRGBA[2] = ent->shaderRGBA[1] = ent->shaderRGBA[0];

		trap_R_AddRefEntityToSceneX(ent);
	}
}

/*For debugging the antilag */
void CG_PlayerBB(centity_t *cent)
{
	vec3_t	mins = { -10, -10, -10};
	vec3_t	maxs = { 10, 10, 10};

	CG_DrawBB(cent->lerpOrigin, mins, maxs, 1, 255, 0, 0);
}

/**
 * $(function)
 */
void CG_Corpse(centity_t *cent)
{
	refEntity_t   legs;
	refEntity_t   torso;
	refEntity_t   head;
	vec3_t	      blank = { 0, 0, 0};
	vec3_t	      vel;
	clientInfo_t  *host;
	qboolean      InitializedThisFrame;
	int	      j;
	float	      angle, dist, radius;
	vec3_t	      dir;
	vec4_t	      playercol;
	trace_t       trace;
	vec3_t	      start, end;
	vec3_t	      mins = { -20, -20, -5};
	vec3_t	      maxs = { 20, 20, 5};
	int	      team;
	utSkins_t     sidx;

	/* If bomb exploded and corpse wasn't gibbed yet. */
	if (cg.bombExploded && !cgs.clientinfo[cent->currentState.clientNum].gibbed) {
		/* Calculate distance. */
		VectorSubtract(cent->lerpOrigin, cg.bombExplosionOrigin, vel);
		VectorNormalize(vel,dist);

		/* Calculate radius. */
		radius = (cg.time - cg.bombExplodeTime) * 1.0f;

		/* If shockwave has reached the corpse and corpse is within gib range. */
		if ((dist <= radius) && (dist < 1500) && ((radius - dist) < 500)) {
			/* Calculate velocity. */
			VectorScale(vel, 250.0f, vel);

			/* Gib it. */
			CG_GibPlayer(cent->lerpOrigin, vel);

			/* Set flag. */
			cgs.clientinfo[cent->currentState.clientNum].gibbed = qtrue;
		}
	}

	/* If bomb exploded and corpse was gibbed. */
	if (cg.bombExploded && cgs.clientinfo[cent->currentState.clientNum].gibbed) {
		/* No skeletons yet so don't draw the player model. */
		return;
	}

	memset(&legs, 0, sizeof(legs));
	memset(&torso, 0, sizeof(torso));
	memset(&head, 0, sizeof(head));

	/*team + race are kept in modelindex and modelindex2 */
	/*sanity check */
	if ((cent->currentState.modelindex2 < 0) || (cent->currentState.modelindex2 > 3)) {
		cent->currentState.modelindex2 = 0;
	}

	team = cent->currentState.modelindex;
	sidx = CG_WhichSkin(team);
	switch (sidx) {
	case UT_SKIN_GREEN:
		host = cgs.media.Free_Skins;
		break;
	case UT_SKIN_BLUE:
		host = cgs.media.Blue_Skins;
		break;
	case UT_SKIN_RED:
		host = cgs.media.Red_Skins;
		break;
	case UT_SKIN_PURPLE:
		host = cgs.media.Jump_Skins;
		break;
	case UT_SKIN_ORANGE:
		host = cgs.media.Orange_Skins;
		break;
	case UT_SKIN_OLIVE:
		host = cgs.media.Olive_Skins;
		break;
	case UT_SKIN_WHITE:
		host = cgs.media.White_Skins;
		break;
	case UT_SKIN_BLACK:
		host = cgs.media.Black_Skins;
		break;
	case UT_SKIN_DESERT:
		host = cgs.media.Desert_Skins;
		break;
	case UT_SKIN_COWBOY:
		host = cgs.media.Cowboy_Skins;
		break;
	case UT_SKIN_CAVALRY:
		host = cgs.media.Cavalry_Skins;
		break;
	case UT_SKIN_DROOGS:
		host = cgs.media.Droogs_Skins;
		break;
	}
	host = &host[cent->currentState.modelindex2];

	playercol[0] =
		cgs.clientinfo[cent->currentState.clientNum].armbandRGB[0];
	playercol[1] =
		cgs.clientinfo[cent->currentState.clientNum].armbandRGB[1];
	playercol[2] =
		cgs.clientinfo[cent->currentState.clientNum].armbandRGB[2];
	playercol[3] = 128;

	if ( cg.g_armbands == 1 ) {
		playercol[0] = skinInfos[sidx].defaultArmBandColor[0];
		playercol[1] = skinInfos[sidx].defaultArmBandColor[1];
		playercol[2] = skinInfos[sidx].defaultArmBandColor[2];
	}

	/* Set the model/skins */
	legs.hModel	    = host->legsModel;
	legs.customSkin     = host->legsSkin;
	torso.hModel	    = host->torsoModel;
	torso.customSkin    = host->torsoSkin;

	torso.shaderRGBA[0] = playercol[0];
	torso.shaderRGBA[1] = playercol[1];
	torso.shaderRGBA[2] = playercol[2];
	torso.shaderRGBA[3] = playercol[3];
	legs.shaderRGBA[0]  = playercol[0];
	legs.shaderRGBA[1]  = playercol[1];
	legs.shaderRGBA[2]  = playercol[2];
	legs.shaderRGBA[3]  = playercol[3];

	if (cent->currentState.powerups & POWERUP(PW_VEST)) {
		torso.hModel	 = host->vesttorsoModel;
		torso.customSkin = host->vesttorsoSkin;
	}

	head.hModel	= host->headModel;
	head.customSkin = host->headSkin;

	/* Positions */
	VectorCopy(cent->lerpOrigin, legs.origin);

	VectorCopy(cent->lerpOrigin, legs.lightingOrigin);
	VectorCopy(cent->lerpOrigin, torso.origin);
	VectorCopy(cent->lerpOrigin, torso.lightingOrigin);
	VectorCopy(cent->lerpOrigin, head.origin);
	VectorCopy(cent->lerpOrigin, head.lightingOrigin);

	/* Do the orientate thing */
	VectorCopy(cent->lerpOrigin, start);
	VectorCopy(cent->lerpOrigin, end);
	start[2] += 4;
	end[2]	 -= 40;
	trap_CM_BoxTrace(&trace, start, end, mins, maxs, 0, CONTENTS_SOLID);

	if ((trace.fraction < 1) && (trace.fraction > 0)) {
		AxisFromNormal(trace.plane.normal,
			legs.axis, cent->lerpAngles[1]);
		/* sink a bit if you're on a ramp */
		legs.origin[2] -= 1 - trace.plane.normal[2];
	} else {
		/* Set orientation. */
		AnglesToAxis(cent->lerpAngles, legs.axis);
	}

	AnglesToAxis(blank, torso.axis);
	AnglesToAxis(blank, head.axis);

	legs.renderfx  = RF_LIGHTING_ORIGIN | RF_MINLIGHT;
	torso.renderfx = RF_LIGHTING_ORIGIN | RF_MINLIGHT;
	head.renderfx  = RF_LIGHTING_ORIGIN | RF_MINLIGHT;

	/*Scale booster */
	/*VectorScale(legs.axis[0], 1.15, legs.axis[0]); */
	/*VectorScale(legs.axis[1], 1.15, legs.axis[1]); */
	/*VectorScale(legs.axis[2], 1.15, legs.axis[2]); */
	/*legs.origin[2] += 4; */

	/* Animate em */
	InitializedThisFrame = qfalse;

	if (cent->pe.view.frame != cent->currentState.generic1) {
		/*
		 * This is the initialisation
		 * We have two possible initialisation scenarios
		 * Either the cg.time matches cent->currentState.origin2[0]
		 * or it doesn't. If it DOES match the corpse died in our PVS,
		 * if it doesn't match corpse died outside it and we should
		 * set it straight to dead.
		 */

		/* Copy the players current info */
		if (cent->currentState.clientNum != cg.clientNum) {
			/* We have a valid cg_entity to steal anim data from */
			memcpy(&cent->pe,
				&CG_ENTITIES(cent->currentState.clientNum)->pe,
				sizeof(playerEntity_t));
		} else {
			/* Else we copy from the predicted player ent */
			memcpy(&cent->pe, &cg.predictedPlayerEntity.pe,
				sizeof(playerEntity_t));
		}

		/*
		 * Generic1 is set to a random key so we can tell when we
		 * need to initialize..
		 * The problem is that we wanna store flags in pe.view,
		 * which is unused, BUT, its also uninitialized from the
		 * previous owner... Hackish, but was the cleanest way.
		 * Note: even if by some freak that the numbers matched and
		 * shouldn't, the anim will just freak out but wont crash...
		 */
		cent->pe.view.frame = cent->currentState.generic1;

		if (cent->currentState.origin2[0] == cg.snap->serverTime) {
			InitializedThisFrame = qtrue;
		} else {
			/* Set us to dead */
			cent->pe.legs.animationTime  += 5000;
			cent->pe.torso.animationTime += 5000;
		}
	}

	C_RunLerpFrame(host->animations, &cent->pe.torso,
		       cent->currentState.torsoAnim, 1, cg.time);
	torso.oldframe = cent->pe.torso.oldFrame;
	torso.frame    = cent->pe.torso.frame;
	torso.backlerp = cent->pe.torso.backlerp;

	C_RunLerpFrame(host->animations, &cent->pe.legs,
		       cent->currentState.legsAnim, 1, cg.time);
	legs.oldframe = cent->pe.legs.oldFrame;
	legs.frame    = cent->pe.legs.frame;
	legs.backlerp = cent->pe.legs.backlerp;

	/* Set their final world translations and render */
	CG_PositionRotatedEntityOnTag(&torso, &legs, legs.hModel, "tag_torso");
	CG_PositionRotatedEntityOnTag(&head, &torso, torso.hModel, "tag_head");

	CG_AddRefEntityWithNVGGlow(&legs, &cent->currentState);

	CG_AddRefEntityWithNVGGlow(&torso, &cent->currentState);

	/* Deal with legs and torso damage skins */
	if (cg_blood.value != 0) {
		if (cg_sfxShowDamage.integer &&
				(((cent->currentState.time >> 8) & 0xFF) & 2)) {
			if ((cent->currentState.time & 0xFF) < 30) {
				legs.customSkin = host->legsDamageSkin[2];
				trap_R_AddRefEntityToSceneX(&legs);

				torso.customSkin = host->torsoDamageSkin[2];
				trap_R_AddRefEntityToSceneX(&torso);
			} else if ((cent->currentState.time & 0xFF) < 60) {
				legs.customSkin = host->legsDamageSkin[1];
				trap_R_AddRefEntityToSceneX(&legs);

				torso.customSkin = host->torsoDamageSkin[1];
				trap_R_AddRefEntityToSceneX(&torso);
			} else if ((cent->currentState.time & 0xFF) < 100) {
				legs.customSkin = host->legsDamageSkin[0];
				trap_R_AddRefEntityToSceneX(&legs);

				torso.customSkin = host->torsoDamageSkin[0];
				trap_R_AddRefEntityToSceneX(&torso);
			}
		}
	}

	/* If A Medic show medic bands */
	if (((int)cent->currentState.powerups) & POWERUP(PW_MEDKIT)) {
		torso.customSkin = host->torsoDamageSkin[4];
		trap_R_AddRefEntityToSceneX(&torso);
	}

	/* Decal for beheading */
	if (((int)cent->currentState.powerups) & POWERUP(PW_HASNOHEAD)) {
		torso.customSkin = host->torsoDamageSkin[3];
		CG_AddRefEntityWithNVGGlow(&torso, &cent->currentState);
	}

	/* If we still have a head, draw it in */
	if (!(((int)cent->currentState.powerups) & POWERUP(PW_HASNOHEAD))) {
		CG_AddRefEntityWithNVGGlow(&head, &cent->currentState);

		/* if ( cent->currentState.powerups & POWERUP(PW_HELMET) )
			   {
				   head.hModel = host->helmetModel;
				   head.customSkin = host->helmetSkin;
				   CG_AddWeaponShiney( &head );
			   } */
		if ((cg_blood.value != 0) && (cg_sfxShowDamage.integer)) {
			if (cent->currentState.powerups &
					POWERUP(PW_HASBULLETHOLEINHEAD)) {
				head.hModel	= host->headModel;
				head.customSkin = host->headDamageSkin;
				CG_AddWeapon(&head);
			}
		}
	} else {
		if (InitializedThisFrame == qtrue) {
			/*
			 * if we're making the head explode..
			 * This will get called the first snapshot the
			 * corpse is drawn
			 */
			CG_Bleed(head.origin, -1);

			for (j = 0; j < 20; j++) {
				angle  = rand() * 1000;
				dir[0] = sin(angle) * 5;
				dir[1] = cos(angle) * 5;
				dir[2] = (rand() % 1000) / 200;

				CG_BleedPlayer(head.origin, dir, 20 +
					       rand() % 70);
			}
		}
	}
}

/**
 * CG_Player
 */
void CG_Player(centity_t *cent)
{
	clientInfo_t  *ci;
	refEntity_t   legs;
	refEntity_t   torso;
	refEntity_t   head;
	refEntity_t   funstuff;
	refEntity_t   weapon;
	int	      clientNum;
	int	      renderfx;
	int	      primary, secondary, sidearm;
	float	      shadowPlane = 0.0f;
	vec4_t	      playercol;
	vec3_t	      head_origin, torso_origin;
	//vec_t	      head_axis[3][3];
	int	      j;
	int	      team;
	qboolean      HaveHeadGearOn = qfalse;
	utSkins_t     sidx;

	/* the client number is stored in clientNum.  It can't be derived */
	/* from the entity number, because a single client may have */
	/* multiple corpses on the level using the same clientinfo */
	clientNum = cent->currentState.clientNum;

	if ((clientNum < 0) || (clientNum >= MAX_CLIENTS)) {
		CG_Error("Bad clientNum on player entity");
	}

	/* If this dude is dead. */
	if (cent->currentState.eFlags & EF_DEAD) {
		return;
	}

	ci = &cgs.clientinfo[clientNum];

	/* it is possible to see corpses from disconnected players that may */
	/* not have valid clientinfo */
	if (!ci->infoValid) {
		return;
	}

	/* Clear stuff. */
	ci->gibbed = qfalse;

	/* get the player model information */
	renderfx = RF_LIGHTING_ORIGIN | RF_MINLIGHT;

	if (cent->currentState.number == cg.snap->ps.clientNum) {
		if (!cg.renderingThirdPerson) {
			renderfx |= RF_THIRD_PERSON; /* only draw in mirrors */
		} else {
			if (cg_cameraMode.integer) {
				return;
			}
		}
	}

	memset(&legs, 0, sizeof(legs));
	memset(&torso, 0, sizeof(torso));
	memset(&head, 0, sizeof(head));
	memset(&weapon, 0, sizeof(weapon));

	playercol[0] = ci->armbandRGB[0];
	playercol[1] = ci->armbandRGB[1];
	playercol[2] = ci->armbandRGB[2];
	playercol[3] = 128;
	team	     = ci->team;

	sidx = CG_WhichSkin(team);
	if (cg.g_armbands == 1) {
		playercol[0] = skinInfos[sidx].defaultArmBandColor[0];
		playercol[1] = skinInfos[sidx].defaultArmBandColor[1];
		playercol[2] = skinInfos[sidx].defaultArmBandColor[2];
	}

	CG_PlayerAngles(cent, legs.axis, torso.axis, head.axis);

	/* get the animation state (after rotation, to allow feet shuffle) */
	C_PlayerAnimation(&cent->currentState, ci->animations,
			  &cent->pe.legs,	&cent->pe.torso, &legs.oldframe,
			  &legs.frame, &legs.backlerp, &torso.oldframe,
			  &torso.frame, &torso.backlerp, cg.time);

	if (r_debugAries.integer) {
		int modelType = (ci->gender == GENDER_FEMALE) ?
				ARIES_ATHENA : ARIES_ORION;

		/*
		Com_Printf("C (%d): %d %f %d %f\n", clientNum, legs.frame, legs.backlerp,
				torso.frame, torso.backlerp);
		*/
		CG_DrawAries(modelType, cent->lerpOrigin, &legs, &torso, &head);
		return;
	}

	/* add the talk baloon or disconnect icon */
	CG_PlayerSprites(cent);

	/* add a water splash if partially in and out of water */
	CG_PlayerSplash(cent);

	/* add the bleed */
	CG_PlayerBleeding(cent);

	/* add the legs */
	legs.hModel	   = ci->legsModel;
	legs.customSkin    = ci->legsSkin;
	legs.shaderRGBA[0] = playercol[0];
	legs.shaderRGBA[1] = playercol[1];
	legs.shaderRGBA[2] = playercol[2];
	legs.shaderRGBA[3] = playercol[3];

	VectorCopy(cent->lerpOrigin, legs.origin);
	VectorCopy(cent->lerpOrigin, legs.lightingOrigin);
	legs.shadowPlane = shadowPlane;
	legs.renderfx = renderfx;

	CG_AddRefEntityWithNVGGlow(&legs, &cent->currentState);

	/* If show damage is on, then show it */
	if (cg_blood.value != 0 &&
	    cg_sfxShowDamage.integer &&
	    (((cent->currentState.time >> 8) & 0xFF) & 2)) {
		if ((cent->currentState.time & 0xFF) < 30) {
			legs.customSkin = ci->legsDamageSkin[2];
			trap_R_AddRefEntityToSceneX(&legs);
		} else if ((cent->currentState.time & 0xFF) < 60) {
			legs.customSkin = ci->legsDamageSkin[1];
			trap_R_AddRefEntityToSceneX(&legs);
		} else if ((cent->currentState.time & 0xFF) < 100) {
			legs.customSkin = ci->legsDamageSkin[0];
			trap_R_AddRefEntityToSceneX(&legs);
		}
	}

	/* if the model failed, allow the default nullmodel to be displayed */
	if (!legs.hModel) {
		return;
	}

	/* add the torso */
	torso.hModel	 = ci->torsoModel;
	torso.customSkin = ci->torsoSkin;

	if (cent->currentState.powerups & POWERUP(PW_VEST)) {
		torso.hModel	 = ci->vesttorsoModel;
		torso.customSkin = ci->vesttorsoSkin;
	}

	torso.shaderRGBA[0] = playercol[0];
	torso.shaderRGBA[1] = playercol[1];
	torso.shaderRGBA[2] = playercol[2];
	torso.shaderRGBA[3] = playercol[3];

	if (!torso.hModel) {
		return;
	}

	VectorCopy(cent->lerpOrigin, torso.lightingOrigin);

	CG_PositionRotatedEntityOnTag(&torso, &legs,
		ci->legsModel, "tag_torso");
	VectorCopy(torso.origin, torso_origin);
	torso.shadowPlane = shadowPlane;
	torso.renderfx = renderfx;

	CG_AddRefEntityWithNVGGlow(&torso, &cent->currentState);

	/* If show damage is on, then show it */
	if (cg_blood.value != 0 &&
	    cg_sfxShowDamage.integer &&
	    ((cent->currentState.time >> 8) & 0xFF) & 1) {
		if ((cent->currentState.time & 0xFF) < 30) {
			torso.customSkin = ci->torsoDamageSkin[2];
			trap_R_AddRefEntityToSceneX(&torso);
		} else if ((cent->currentState.time & 0xFF) < 60) {
			torso.customSkin = ci->torsoDamageSkin[1];
			trap_R_AddRefEntityToSceneX(&torso);
		} else if ((cent->currentState.time & 0xFF) < 100) {
			torso.customSkin = ci->torsoDamageSkin[0];
			trap_R_AddRefEntityToSceneX(&torso);
		}
	}

	/*If A Medic show medic bands */
	if (((int)cent->currentState.powerups) & POWERUP(PW_MEDKIT)) {
		torso.customSkin = ci->torsoDamageSkin[4];
		trap_R_AddRefEntityToSceneX(&torso);
	}

	/* if A bomber show bomber bands */
	if (((int)cent->currentState.powerups) & POWERUP(PW_CARRYINGBOMB)) {
		torso.customSkin = ci->torsoDamageSkin[5];
		trap_R_AddRefEntityToSceneX(&torso);
	}

	/* reset torso model for weapons */
	torso.hModel = ci->torsoModel;

	VectorCopy(cent->lerpOrigin, weapon.lightingOrigin);
	weapon.shadowPlane = shadowPlane;
	weapon.renderfx = renderfx & ~RF_MINLIGHT ;

	/* If visible items is turned on then add the sidearm, primary and */
	/* secondary weapons if the player has them */
	if (cg_sfxVisibleItems.integer) {
		/* Urban Terror: add the attached weapons */
		sidearm   = cent->currentState.time2 & 0xFF;
		secondary = (cent->currentState.time2 >> 8) & 0xFF;
		primary   = (cent->currentState.time2 >> 16) & 0xFF;

		if (sidearm) {
			weapon.hModel = cg_weapons[sidearm].weaponModel;
			CG_PositionEntityOnTag(&weapon, &legs,
				ci->legsModel, "tag_sidearm");
			CG_AddWeaponShiney(&weapon);
		}

		if (secondary) {
			weapon.hModel = cg_weapons[secondary].weaponModel;
			/*
			 * not an error: tag_secondary got truncated in the
			 * conversion to MD3
			 */
			CG_PositionEntityOnTag(&weapon, &torso,
				ci->torsoModel, "tag_second");
			CG_AddWeaponShiney(&weapon);
		}

		if (primary) {
			weapon.hModel = cg_weapons[primary].weaponModel;
			CG_PositionEntityOnTag(&weapon, &torso,
				ci->torsoModel, "tag_primary");
			CG_AddWeaponShiney(&weapon);
		}
	}

	if (cent->currentState.powerups & POWERUP(PW_NEUTRALFLAG)) {
		weapon.hModel	  = cgs.media.UT_Backpack;
		weapon.customSkin = cgs.media.UT_BackpackExplosives;
		CG_PositionEntityOnTag(&weapon, &torso,
			ci->torsoModel, "tag_back");
		CG_AddWeapon(&weapon);
	} else if (cent->currentState.powerups & POWERUP(PW_MEDKIT)) {
		weapon.hModel	  = cgs.media.UT_Backpack;
		weapon.customSkin = cgs.media.UT_BackpackMedic;
		CG_PositionEntityOnTag(&weapon, &torso,
			ci->torsoModel, "tag_back");
		CG_AddWeapon(&weapon);
	}

	if (cg_funstuff.integer && cgs.g_funstuff) {
        memset(&funstuff, 0, sizeof(funstuff));
    	VectorCopy(cent->lerpOrigin, funstuff.lightingOrigin);
    	funstuff.shadowPlane = shadowPlane;
    	funstuff.renderfx = renderfx;
    	CG_PositionRotatedEntityOnTag(&funstuff, &torso, ci->torsoModel, "tag_back");
    	VectorScale(torso.axis[0], 1.155f, funstuff.axis[0]);
    	VectorScale(torso.axis[1], 1.155f, funstuff.axis[1]);
    	VectorScale(torso.axis[2], 1.155f, funstuff.axis[2]);

    	for (j = 0; j < 3; j++) {
	    	if (ci->funmodel[j]) {
	    		/* back */
	    		if (ci->funmodeltag[j] == 2) {
	    			funstuff.hModel	  = ci->funmodel[j];
	    			funstuff.customSkin = 0;
	    			CG_AddWeapon(&funstuff);
	    		}
	    	}
	    }
    }

	/* add the head */
	head.hModel = ci->headModel;

	if (!head.hModel) {
		return;
	}
	head.customSkin = ci->headSkin;

	CG_PositionRotatedEntityOnTag(&head, &torso, ci->torsoModel, "tag_head");
	VectorCopy(head.origin, head_origin);
	VectorCopy(cent->lerpOrigin, head.lightingOrigin);
	head.shadowPlane = shadowPlane;
	head.renderfx = renderfx;

	CG_AddRefEntityWithNVGGlow(&head, &cent->currentState);

	/* If show damage is on, then show it */
	if (cg_blood.integer != 0) {
		if (cg_sfxShowDamage.integer) {
			if (((cent->currentState.time >> 8) & 0xFF)) {
				if ((((cent->currentState.time >> 8) & 0xFF) & 0x4)) {
					head.customSkin = ci->headDamageSkin;
					trap_R_AddRefEntityToSceneX(&head);
				}
			}
		}
	}

	if (cg_funstuff.integer && cgs.g_funstuff) {
        memset(&funstuff, 0, sizeof(funstuff));
	    VectorCopy(cent->lerpOrigin, funstuff.lightingOrigin);
	    funstuff.shadowPlane = shadowPlane;
	    funstuff.renderfx = renderfx;
	    CG_PositionRotatedEntityOnTag(&funstuff, &head, ci->headModel, "tag_funstuff");
	    VectorScale(head.axis[0], 1.155f, funstuff.axis[0]);
	    VectorScale(head.axis[1], 1.155f, funstuff.axis[1]);
	    VectorScale(head.axis[2], 1.155f, funstuff.axis[2]);

	    for (j = 0; j < 3; j++) {
	    	if (ci->funmodel[j]) {
	    		/* head */
	    		if (ci->funmodeltag[j] == 0) {
	    			if (!(cent->currentState.powerups &
	    				POWERUP(PW_HELMET))) {
	    				funstuff.hModel	= ci->funmodel[j];
	    				funstuff.customSkin = 0;
					
	    				CG_AddWeaponShiney(&funstuff);
	    				HaveHeadGearOn = qtrue;
	    			}
	    		}

	    		/* head with helmet is ok */
	    		if (ci->funmodeltag[j] == 1) {
	    			funstuff.hModel	= ci->funmodel[j];
	    			funstuff.customSkin = 0;

	    			CG_AddWeaponShiney(&funstuff);
	    		}
	    	}
	    }
    }

	/* If visible items are on then add the helmet nad goggles if the client */
	/* has them. */

	/* If the player has a helmet then add it */
	if (cent->currentState.powerups & POWERUP(PW_HELMET)) {
		/* if (HaveHeadGearOn==qfalse) */
		{
			head.hModel	= ci->helmetModel;
			head.customSkin = ci->helmetSkin;

			if ((cgs.gametype == GT_UT_ASSASIN) && ci->teamLeader) {
				head.customSkin = ci->helmetLeaderSkins[CG_WhichSkin(ci->team)];
			}

			CG_AddWeaponShiney(&head);
		}
	}

	if (cg_sfxVisibleItems.integer) {
		/* If the player has goggles then add them */
		if (cent->currentState.powerups & POWERUP(PW_NVG)) {
			head.hModel	= ci->nvgModel;
			head.customSkin = ci->nvgSkin;
			CG_AddWeapon(&head);
		}
	}

	CG_BreathPuffs(cent, &head);
	CG_DustTrail(cent);
	CG_PlayerShadow(cent, &shadowPlane);

	if (!(cent->currentState.powerups & POWERUP(PW_NOWEAPON))) {
		CG_AddPlayerWeapon(&torso, NULL, cent, ci->team);
	}

	if (cent->currentState.powerups & POWERUP(PW_REDFLAG)) {
		if (cgs.gametype == GT_JUMP) {
			weapon.hModel	  = cgs.media.flagModels[UT_SKIN_RED];
		} else {
			weapon.hModel	  = cgs.media.flagModels[CG_WhichSkin(TEAM_RED)];
		}
		weapon.customSkin = 0;
		CG_PositionEntityOnTag(&weapon, &torso, ci->torsoModel,
				       "tag_flag");
		VectorScale(weapon.axis[0], 0.5f, weapon.axis[0]);
		VectorScale(weapon.axis[1], 0.5f, weapon.axis[1]);
		VectorScale(weapon.axis[2], 0.5f, weapon.axis[2]);
		weapon.nonNormalizedAxes = qtrue;
		CG_AddWeapon(&weapon);
	}

	if (cent->currentState.powerups & POWERUP(PW_BLUEFLAG)) {
		if (cgs.gametype == GT_JUMP) {
			weapon.hModel	  = cgs.media.flagModels[UT_SKIN_BLUE];
		} else {
			weapon.hModel	  = cgs.media.flagModels[CG_WhichSkin(TEAM_BLUE)];
		}
		weapon.customSkin = 0;
		CG_PositionEntityOnTag(&weapon, &torso, ci->torsoModel,
				       "tag_flag");
		VectorScale(weapon.axis[0], 0.5f, weapon.axis[0]);
		VectorScale(weapon.axis[1], 0.5f, weapon.axis[1]);
		VectorScale(weapon.axis[2], 0.5f, weapon.axis[2]);
		weapon.nonNormalizedAxes = qtrue;
		CG_AddWeapon(&weapon);
	}

	CG_DrawTrail(cent, head_origin, torso_origin);

	if (cent->currentState.eFlags & EF_POWERSLIDE) {
		vec3_t	dir    = { 0, 0, 20};
		vec3_t	org;
		float	radius = 50;

		VectorCopy(cent->lerpOrigin, org);
		org[2] -= 20;
		org[0] += (rand() % 60) - 30;
		org[1] += (rand() % 60) - 30;
		CG_SmokePuff(org, dir, radius, 1, 1, 1, 0.9f, 300, cg.time,
			     0, 0, cgs.media.shotgunSmokePuffShader);
	}
}

/**
 * CG_ResetPlayerEntity
 *
 * A player just came into view or teleported, so reset all animation info
 */
void CG_ResetPlayerEntity(centity_t *cent)
{
	int  j;

	C_ClearLerpFrame(cgs.clientinfo[cent->currentState.clientNum]
			 .animations, &cent->pe.legs,
			 cent->currentState.legsAnim,
			 cg.time);
	C_ClearLerpFrame(cgs.clientinfo[cent->currentState.clientNum]
			 .animations, &cent->pe.torso,
			 cent->currentState.torsoAnim,
			 cg.time);

	BG_EvaluateTrajectory(&cent->currentState.pos, cg.time,
			      cent->lerpOrigin);
	BG_EvaluateTrajectory(&cent->currentState.apos, cg.time,
			      cent->lerpAngles);

	memset(&cent->pe.legs, 0, sizeof(cent->pe.legs));
	cent->pe.legs.yawAngle	 = cent->lerpAngles[YAW];
	cent->pe.legs.yawing	 = qfalse;
	cent->pe.legs.pitchAngle = 0;
	cent->pe.legs.pitching	 = qfalse;

	memset(&cent->pe.torso, 0, sizeof(cent->pe.legs));
	cent->pe.torso.yawAngle   = cent->lerpAngles[YAW];
	cent->pe.torso.yawing	  = qfalse;
	cent->pe.torso.pitchAngle = cent->lerpAngles[PITCH];
	cent->pe.torso.pitching   = qfalse;

	if (cg_debugPosition.integer) {
		CG_Printf("%i ResetPlayerEntity yaw=%i\n",
			  cent->currentState.number, cent->pe.torso.yawAngle);
	}

	/* reset trail info */
	cent->pe.trail.trailing = 0;

	for (j = 0; j < MAX_TRAIL; j++) {
		cent->pe.trail.trails[j].link = -1;
	}
}

/**
 * CG_BuildPlayerSpectatorList
 *
 * Basically if we are currently a spectator, then we print out the text of
 * the other players depending on their visibility.
 * This builds a list of players in the current frame
 */
void CG_BuildPlayerSpectatorList(centity_t *cent)
{
	int		  clientNum;
	clientInfo_t	  *ci;
	specplayerinfo_t  *p;

	if (!cent) {
		return;
	}

	if (cent->currentState.number == cg.snap->ps.clientNum) {
		return;
	}

	if (cg.NumPlayersSnap >= MAX_CLIENTS) {
		return; /* Soz too much on screen */
	}

	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		return; /* if we aren't a spectator then no go! */
	}
	clientNum = cent->currentState.clientNum;

	if ((clientNum < 0) || (clientNum >= MAX_CLIENTS)) {
		CG_Error("Bad clientNum on player entity");
	}
	ci = &cgs.clientinfo[clientNum];

	if (!ci->infoValid) {
		return;
	}

	if (ci->team == TEAM_SPECTATOR) {  /* they are a spectator too */
		return;
	}

	/* check, if dead then no stats */
	if ((cent->currentState.time & 0xFF) <= 0) {
		return;
	}
	p	   = &PlayerInfo[cg.NumPlayersSnap];
	p->Name    = ci->name;
	p->Health  = (cent->currentState.time & 0xFF);
	p->Armor   = ci->armor; /* NULL */
	p->Team    = ci->team;
	VectorCopy(cent->lerpOrigin, p->XYZ);
	p->XYZ[2] += 64; /* reposition the Z to above the player's head */
	p->num	   = cent->currentState.number; /* if its ever required! */
	p->Weapon  = cent->currentState.weapon;
	cg.NumPlayersSnap++;
}

/**
 * CG_DrawTrail
 */
void CG_DrawTrail(centity_t *cent, vec3_t top, vec3_t bot)
{
	int	    j;
	trailp_t    *t;
	trailp_t    *p;
	polyVert_t  pv[4];
	int	    i;
	int	    trailtime  = 700;
	int	    sampletime = 50;
	float	    tscale;
	float	    v;
	vec3_t	    vel;
	qboolean    isPlayer = qfalse;
	utSkins_t   sidx;

	if (cg.snap->ps.clientNum == cent->currentState.clientNum) {
		isPlayer = qtrue;
	}

	bot[2] -= 10;

	VectorCopy(cent->currentState.pos.trDelta, vel);
	v = VectorLength(vel);

	if (v > 600) {
		cent->pe.trail.trailing = cg.time + 600;
		v		       -= 600;
	}

	if (cg.time - cent->pe.trail.lasttime > sampletime) {
		if (cent->pe.trail.trailing > cg.time) {
			/* Add a new trail */
			for (j = 0; j < MAX_TRAIL; j++) {
				t = &cent->pe.trail.trails[j];

				if (t->time < cg.time) {
					break;
				}
			}

			/* found a freebie */
			if (j < MAX_TRAIL) {
				t->top[0]	    = top[0];
				t->top[1]	    = top[1];
				t->top[2]	    = top[2];
				t->bottom[0]	    = bot[0];
				t->bottom[1]	    = bot[1];
				t->bottom[2]	    = bot[2];
				t->time 	    = cg.time + trailtime;
				t->isolated	    = 0;
				t->colour[0]	    = v;
				t->link 	    = cent->pe.trail.head;
				cent->pe.trail.head = j;

				/*
				 * Remove all links to this newly assigned one
				 * this lets us reuse without a wait
				 */
				for (i = 0; i < MAX_TRAIL; i++) {
					p = &cent->pe.trail.trails[i];

					if (p->link == j) {
						p->link = -1;
					}
				}
			}
			cent->pe.trail.lasttime = cg.time;
		}
	} else {
		/* just update head to us */
		t = &cent->pe.trail.trails[cent->pe.trail.head];

		if (t->time > cg.time) {
			t->colour[0] = v;
			t->top[0]    = top[0];
			t->top[1]    = top[1];
			t->top[2]    = top[2];
			t->bottom[0] = bot[0];
			t->bottom[1] = bot[1];
			t->bottom[2] = bot[2];
			t->isolated  = 0;
		}
	}

	/* update pos/colour */
	for (j = 0; j < MAX_TRAIL; j++) {
		t = &cent->pe.trail.trails[j];

		if (t->time > cg.time) {
			tscale = (float)(t->time - cg.time) / trailtime;
			t->colour[3] = 32 * tscale;
		}
	}

	/* draw it */
	for (j = 0; j < MAX_TRAIL; j++) {
		t = &cent->pe.trail.trails[j];
		p = &cent->pe.trail.trails[t->link];

		if ((t->time > cg.time) && (t->link != -1)) {
			if (p->time <= cg.time) {
				/* oh oh, links broken */
				t->link = -1;
				continue;
			}
			/* draw ribbon piece between T and P */

			VectorCopy(t->top, pv[0].xyz);
			VectorCopy(p->top, pv[1].xyz);
			VectorCopy(t->bottom, pv[3].xyz);
			VectorCopy(p->bottom, pv[2].xyz);

			pv[0].modulate[3] = t->colour[3] * 1.5f;
			pv[3].modulate[3] = t->colour[3] * 0.5f;
			pv[1].modulate[3] = p->colour[3] * 1.5f;
			pv[2].modulate[3] = p->colour[3] * 0.5f;

			for (i = 0; i < 4; i++) {
				sidx = CG_WhichSkin(cgs.clientinfo[cent->currentState.clientNum].team);
				pv[i].modulate[0] = (int) skinInfos[sidx].trail_color[0];
				pv[i].modulate[1] = (int) skinInfos[sidx].trail_color[1];
				pv[i].modulate[2] = (int) skinInfos[sidx].trail_color[2];
			}

			/* Remove the glitchy 1st person poly */
			if (isPlayer && !cg.renderingThirdPerson) {
				if (t->time - cg.time > 100) {
					continue;
				}
			}

			trap_R_AddPolyToScene(cgs.media.trailShader, 4, pv);
		}
	}
}
