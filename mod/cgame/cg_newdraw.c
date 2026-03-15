/**
 * Filename: $(filename)
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 */

#include "cg_local.h"
#include "../ui/ui_shared.h"

extern displayContextDef_t  cgDC;
extern vec3_t		    bg_colors[20];

int			    sortedTeamPlayers[TEAM_NUM_TEAMS][MAX_CLIENTS];
int			    sortedTeamCounts[TEAM_NUM_TEAMS];
int			    sortedTeamOffsets[TEAM_NUM_TEAMS];

int			    drawTeamOverlayModificationCount = -1;

/**
 * $(function)
 */
void CG_DrawPlayerAmmoValue(rectDef_t *rect, vec4_t color )
{
	char	       *num;
	playerState_t  *ps;
	weaponInfo_t   *pWeaponInfo;
	int	       weaponID;
	float	       scale = 0.50f;
	int	       x;
	int	       bullets;
	vec4_t	       whiteColour = { 1, 1, 1, 1};

	ps = &cg.snap->ps;

	if ((ps->stats[STAT_HEALTH] <= 0) || (ps->pm_type != PM_NORMAL))
	{
		return;
	}

	if (0 == (weaponID = utPSGetWeaponID ( ps )))
	{
		return;
	}

	// If this is a knife or a bomb.
	//if (weaponID == UT_WP_KNIFE || weaponID == UT_WP_BOMB)
	if (weaponID == UT_WP_BOMB)
	{
		return;
	}

	pWeaponInfo = &cg_weapons[weaponID];
	bullets     = utPSGetWeaponBullets(ps);

	if (cg_hudWeaponInfo.integer == 0)
	{
		// Draw the bullets
		num = va("%i", bullets);
		CG_Text_Paint(rect->x + 20 - CG_Text_Width(num, scale, 0), rect->y + rect->h - 14 + CG_Text_Height(num, scale, 0) / 2, scale, color, num, 0, 0, 4);

		// Draw the clip type
		CG_DrawPic ( rect->x + 20, rect->y, 32, 32, pWeaponInfo->ammoIcon );

		// Draw the clip count
		num = va("x%i", utPSGetWeaponClips ( ps ));
		CG_Text_Paint(rect->x + 52, rect->y + rect->h - 24 + CG_Text_Height(num, 0.2f, 0), 0.25f, color, num, 0, 0, 4);
	}
	else if (cg_hudWeaponInfo.integer == 1)
	{
		// Draw the clip count
		num = va("x%i", utPSGetWeaponClips ( ps ));
		CG_Text_Paint(rect->x + 52, rect->y + rect->h - 24 + CG_Text_Height(num, 0.2f, 0), 0.25f, color, num, 0, 0, 4);

		// Draw bullets count.
		num = va("%i", bullets);
		CG_Text_Paint(rect->x + (x = 50 - CG_Text_Width(num, scale, 0)), rect->y + rect->h - 14 + CG_Text_Height(num, scale, 0) / 2, scale, color, num, 0, 0, 4);

		// Draw weapon name.
		num = va("%s", bg_weaponlist[weaponID].name);
		CG_Text_Paint(rect->x + x - CG_Text_Width(num, 0.25f, 0) - 4, rect->y + rect->h - 14 + CG_Text_Height(num, 0.25f, 0), 0.25f, whiteColour, num, 0, 0, 4);
	}
	else
	{
		// Draw the bullets
		num = va("%i", bullets);
		CG_Text_Paint(rect->x + (x = 20 - CG_Text_Width(num, scale, 0)), rect->y + rect->h - 14 + CG_Text_Height(num, scale, 0) / 2, scale, color, num, 0, 0, 4);

		// Draw weapon name.
		num = va("%s", bg_weaponlist[weaponID].name);
		CG_Text_Paint(rect->x + x - CG_Text_Width(num, 0.25f, 0) - 6, rect->y + rect->h - 14 + CG_Text_Height(num, 0.25f, 0), 0.25f, whiteColour, num, 0, 0, 4);

		// Draw the clip type
		CG_DrawPic ( rect->x + 20, rect->y, 32, 32, pWeaponInfo->ammoIcon );

		// Draw the clip count
		num = va("x%i", utPSGetWeaponClips ( ps ));
		CG_Text_Paint(rect->x + 52, rect->y + rect->h - 24 + CG_Text_Height(num, 0.2f, 0), 0.25f, color, num, 0, 0, 4);
	}
}

/**
 * $(function)
 */
#if 0
static void CG_DrawPlayerHead(rectDef_t *rect, qboolean draw2D)
{
	vec3_t	angles;
	float	size, stretch;
	float	frac;
	float	x = rect->x;

	VectorClear( angles );

	if (cg.damageTime && (cg.time - cg.damageTime < DAMAGE_TIME))
	{
		frac		 = (float)(cg.time - cg.damageTime ) / DAMAGE_TIME;
		size		 = rect->w * 1.25 * (1.5 - frac * 0.5);

		stretch 	 = size - rect->w * 1.25;
		// kick in the direction of damage
		x		-= stretch * 0.5 + cg.damageX * stretch * 0.5;

		cg.headStartYaw  = 180 + cg.damageX * 45;

		cg.headEndYaw	 = 180 + 20 * cos( crandom() * M_PI );
		cg.headEndPitch  = 5 * cos( crandom() * M_PI );

		cg.headStartTime = cg.time;
		cg.headEndTime	 = cg.time + 100 + random() * 2000;
	}
	else
	{
		if (cg.time >= cg.headEndTime)
		{
			// select a new head angle
			cg.headStartYaw   = cg.headEndYaw;
			cg.headStartPitch = cg.headEndPitch;
			cg.headStartTime  = cg.headEndTime;
			cg.headEndTime	  = cg.time + 100 + random() * 2000;

			cg.headEndYaw	  = 180 + 20 * cos( crandom() * M_PI );
			cg.headEndPitch   = 5 * cos( crandom() * M_PI );
		}

		size = rect->w * 1.25;
	}

	// if the server was frozen for a while we may have a bad head start time
	if (cg.headStartTime > cg.time)
	{
		cg.headStartTime = cg.time;
	}

	frac	      = (cg.time - cg.headStartTime) / (float)( cg.headEndTime - cg.headStartTime );
	frac	      = frac * frac * (3 - 2 * frac);
	angles[YAW]   = cg.headStartYaw + (cg.headEndYaw - cg.headStartYaw) * frac;
	angles[PITCH] = cg.headStartPitch + (cg.headEndPitch - cg.headStartPitch) * frac;

	CG_DrawHead( x, rect->y, rect->w, rect->h, cg.snap->ps.clientNum, angles );
}
#endif

#if 0
static void CG_DrawSelectedPlayerHealth( rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	clientInfo_t *ci;
	int value;
	char num[16];

  ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
  if (ci) {
		if (shader) {
			trap_R_SetColor( color );
			CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
			trap_R_SetColor( NULL );
		} else {
			Com_sprintf (num, sizeof(num), "%i", ci->health);
		  value = CG_Text_Width(num, scale, 0);
		  CG_Text_Paint(rect->x + (rect->w - value) / 2, rect->y + rect->h, scale, color, num, 0, 0, textStyle);
		}
	}
}
#endif

#if 0
static void CG_DrawSelectedPlayerArmor( rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	clientInfo_t *ci;
	int value;
	char num[16];
  ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
  if (ci) {
    if (ci->armor > 0) {
			if (shader) {
				trap_R_SetColor( color );
				CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
				trap_R_SetColor( NULL );
			} else {
				Com_sprintf (num, sizeof(num), "%i", ci->armor);
				value = CG_Text_Width(num, scale, 0);
				CG_Text_Paint(rect->x + (rect->w - value) / 2, rect->y + rect->h, scale, color, num, 0, 0, textStyle);
			}
		}
	}
}
#endif

qhandle_t CG_StatusHandle(int task)
{
/* URBAN TERROR - These shaders are not used.
	qhandle_t h = cgs.media.assaultShader;
	switch (task) {
		case TEAMTASK_OFFENSE :
			h = cgs.media.assaultShader;
			break;
		case TEAMTASK_DEFENSE :
			h = cgs.media.defendShader;
			break;
		case TEAMTASK_PATROL :
			h = cgs.media.patrolShader;
			break;
		case TEAMTASK_FOLLOW :
			h = cgs.media.followShader;
			break;
		case TEAMTASK_CAMP :
			h = cgs.media.campShader;
			break;
		case TEAMTASK_RETRIEVE :
			h = cgs.media.retrieveShader;
			break;
		case TEAMTASK_ESCORT :
			h = cgs.media.escortShader;
			break;
		default :
			h = cgs.media.assaultShader;
			break;
	}
	return h;
*/

	return 0;
}

/*
static void CG_DrawSelectedPlayerStatus( rectDef_t *rect ) {
	clientInfo_t *ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
	if (ci) {
		qhandle_t h;
		if (cgs.orderPending) {
			// blink the icon
			if ( cg.time > cgs.orderTime - 2500 && (cg.time >> 9 ) & 1 ) {
				return;
			}
			h = CG_StatusHandle(cgs.currentOrder);
		}	else {
			h = CG_StatusHandle(ci->teamTask);
		}
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, h );
	}
}
*/

static void CG_DrawPlayerStatus( rectDef_t *rect )
{
	clientInfo_t  *ci = &cgs.clientinfo[cg.snap->ps.clientNum];

	if (ci)
	{
		qhandle_t  h = CG_StatusHandle(ci->teamTask);
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, h);
	}
}

/*
static void CG_DrawSelectedPlayerName( rectDef_t *rect, float scale, vec4_t color, qboolean voice, int textStyle) {
	clientInfo_t *ci;
  ci = cgs.clientinfo + ((voice) ? cgs.currentVoiceClient : sortedTeamPlayers[CG_GetSelectedPlayer()]);
  if (ci) {
    CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, ci->name, 0, 0, textStyle);
  }
}

static void CG_DrawSelectedPlayerLocation( rectDef_t *rect, float scale, vec4_t color, int textStyle ) {
	clientInfo_t *ci;
  ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
  if (ci) {
		const char *p = CG_ConfigString(CS_LOCATIONS + ci->location);
		if (!p || !*p) {
			p = "unknown";
		}
    CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, p, 0, 0, textStyle);
  }
}
*/

static void CG_DrawPlayerLocation( rectDef_t *rect, float scale, vec4_t color, int textStyle  )
{
	clientInfo_t  *ci = &cgs.clientinfo[cg.snap->ps.clientNum];

	if (ci)
	{
		const char  *p = CG_ConfigString(CS_LOCATIONS + ci->location);

		if (!p || !*p)
		{
			p = "unknown";
		}
		CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, p, 0, 0, textStyle);
	}
}

/*
static void CG_DrawSelectedPlayerWeapon( rectDef_t *rect ) {
	clientInfo_t *ci;

  ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
  if (ci) {
	  if ( cg_weapons[ci->curWeapon].weaponIcon ) {
	    CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg_weapons[ci->curWeapon].weaponIcon );
		} else {
	  CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.deferShader);
    }
  }
}
*/

static void CG_DrawSurvivorRoundWinnerName ( rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	char  name[MAX_QPATH];
	int   value;

	Com_sprintf ( name, sizeof(name), "%s", cg.survivorRoundWinner );
	value = CG_Text_Width(name, scale, 0);
	CG_Text_Paint(rect->x + (rect->w - value) / 2, rect->y + rect->h, scale, color, name, 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawPlayerScore( rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	char  num[16];
	int   value = 0;

	value = cg.snap->ps.persistant[PERS_SCORE];

	if (shader)
	{
		trap_R_SetColor( color );
		CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
		trap_R_SetColor( NULL );
	}
	else
	{
		Com_sprintf (num, sizeof(num), "%i", value);
		value = CG_Text_Width(num, scale, 0);
		CG_Text_Paint(rect->x + (rect->w - value) / 2, rect->y + rect->h, scale, color, num, 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
void CG_DrawPlayerWeaponMode(rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	playerState_t  *ps;
	char	       *name;
	int	       value;
	int	       speed;
	char	       buf[64];

	if (cg_speedo.integer == 1)
	{
		speed = sqrt( cg.predictedPlayerState.velocity[0] * cg.predictedPlayerState.velocity[0] + cg.predictedPlayerState.velocity[1] * cg.predictedPlayerState.velocity[1] );

		if (speed > cg.speedxy)
		{
			cg.speedxy = speed;
		}
		Com_sprintf(buf, sizeof(buf), "XY: %i (%i)", speed, cg.speedxy);
		value = CG_Text_Width(buf, 0.2f, 0);
		CG_Text_Paint(rect->x + (rect->w - value) - 80, rect->y + rect->h, scale, color, buf, 0, 0, textStyle);

		speed = sqrt( cg.predictedPlayerState.velocity[0] * cg.predictedPlayerState.velocity[0] + cg.predictedPlayerState.velocity[1] * cg.predictedPlayerState.velocity[1] + cg.predictedPlayerState.velocity[2] * cg.predictedPlayerState.velocity[2]);

		if (speed > cg.speedxyz)
		{
			cg.speedxyz = speed;
		}
		Com_sprintf(buf, sizeof(buf), "XYZ: %i (%i)", speed, cg.speedxyz);
		value = CG_Text_Width(buf, 0.2f, 0);
		CG_Text_Paint(rect->x + (rect->w - value) - 80, rect->y + rect->h + 12, scale, color, buf, 0, 0, textStyle);
	}
	else
	{
		cg.speedxy  = 0;
		cg.speedxyz = 0;
	}

	ps = &cg.snap->ps;

	if ((ps->stats[STAT_HEALTH] <= 0) || (ps->pm_type != PM_NORMAL))
	{
		return;
	}

	// Dont bother if the weapon only has one mode
	if (!bg_weaponlist[utPSGetWeaponID(ps)].modes[1].name)
	{
		return;
	}

	name  = bg_weaponlist[utPSGetWeaponID(ps)].modes[utPSGetWeaponMode(ps)].name;
	value = CG_Text_Width(name, 0.2f, 0);
	CG_Text_Paint(rect->x + (rect->w - value), rect->y + rect->h, scale, color, name, 0, 0, textStyle);
}

/**
 * $(function)
 */
void CG_GetPercentageColor ( vec4_t out, vec4_t col1, vec4_t col2, float per )
{
	out[0] = (col2[0] - col1[0]) * per + col1[0];
	out[1] = (col2[1] - col1[1]) * per + col1[1];
	out[2] = (col2[2] - col1[2]) * per + col1[2];
	out[3] = (col2[3] - col1[3]) * per + col1[3];
}

/**
 * $(function)
 */
static void CG_DrawPlayerDamage(rectDef_t *rect, vec4_t color ) {

	int  i;

	trap_R_SetColor( color );

	for (i = 0; i < HL_MAX; i++) {

		if (cg.bleedingFrom[i]) {
			CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.UT_DamageIcon[i]);
		}
	}

	if (cg.predictedPlayerState.stats[STAT_AIRMOVE_FLAGS] & UT_PMF_CRASHED) {
		CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.UT_DamageIcon[HL_LEGLL]);
		CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.UT_DamageIcon[HL_LEGLR]);
		CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.UT_DamageIcon[HL_FOOTL]);
		CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.UT_DamageIcon[HL_FOOTR]);
	}

	trap_R_SetColor( NULL );
}

/**
 * $(function)
 */
void CG_DrawPlayerHealth(rectDef_t *rect, vec4_t color )
{
	vec4_t	staminaColor	 = { 1, .8f, 0, 1 };
	vec4_t	bleedingColor	 = { 1, .1f, .1f, 1 };
	vec4_t	usedStaminaColor = { .6f, .6f, .6f, 1 };
	vec4_t	lostStaminaColor = { .17f, 0, 0, 1};
	utSkins_t sidx;

	int	i;
	float	stamina    = cg.snap->ps.stats[STAT_STAMINA];
	float	maxstamina = cg.snap->ps.stats[STAT_HEALTH];

	memcpy ( &staminaColor[0], &color[0], sizeof(vec4_t));

	sidx = CG_WhichSkin(cg.predictedPlayerState.persistant[PERS_TEAM]);
	memcpy ( &usedStaminaColor[0], &skinInfos[sidx].ui_used_stamina_color[0], sizeof(vec4_t));
	memcpy ( &lostStaminaColor[0], &skinInfos[sidx].ui_lost_stamina_color[0], sizeof(vec4_t));

	if ((maxstamina <= 0) || (cg.snap->ps.pm_type != PM_NORMAL))
	{
		return;
	}

	stamina    = 9 * (stamina / (float)UT_MAX_HEALTH) / (float)UT_STAM_MUL;
	maxstamina = 9 * (maxstamina / (float)UT_MAX_HEALTH);

	if(cg.snap->ps.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_BLEEDING)
	{
		float  f = (float)(cg.time % 1000);

		if(f < 500)
		{
			f = (500.0f - f) / 500.0f;
		}
		else
		{
			f = (f - 500.0f) / 500.0f;
		}

		CG_GetPercentageColor ( staminaColor, staminaColor, bleedingColor, f );
	}

	trap_R_SetColor ( NULL );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.UT_PlayerStatusOutline );

	for(i = 0; i < 9; i++)
	{
		float  fp;
		float  bp;

		fp = stamina - (float)(8 - i);
		bp = maxstamina - (float)(8 - i);

		if(fp < 0)
		{
			fp = 0;
		}

		if(bp < 0)
		{
			bp = 0;
		}

		if(fp >= 1.0f)
		{
			trap_R_SetColor ( staminaColor );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.UT_PlayerStatusStamina[i] );
		}
		else if (fp <= 0.0f)
		{
			vec4_t	colBack;
			CG_GetPercentageColor ( colBack, lostStaminaColor, usedStaminaColor, bp );
			trap_R_SetColor ( colBack );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.UT_PlayerStatusStamina[i] );
		}
		else
		{
			vec4_t	colBack;
			CG_GetPercentageColor ( colBack, lostStaminaColor, usedStaminaColor, bp );
			colBack[3] = 1.0f - fp;
			trap_R_SetColor ( colBack );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.UT_PlayerStatusStamina[i] );

			staminaColor[3] = fp;
			trap_R_SetColor ( staminaColor );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.UT_PlayerStatusStamina[i] );
			staminaColor[3] = 1.0f;
		}

		stamina    -= fp;
		maxstamina -= bp;
	}

	CG_DrawPlayerDamage ( rect, lostStaminaColor );
	trap_R_SetColor ( NULL );
}

/**
 * $(function)
 */
static void CG_DrawRedScore(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	int   value;
	char  num[16];

	if (cgs.scores1 == SCORE_NOT_PRESENT)
	{
		Com_sprintf (num, sizeof(num), "-");
	}
	else
	{
		Com_sprintf (num, sizeof(num), "%i", cgs.scores1);
	}
	value = CG_Text_Width(num, scale, 0);
	CG_Text_Paint(rect->x + rect->w - value, rect->y + rect->h, scale, color, num, 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawBlueScore(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	int   value;
	char  num[16];

	if (cgs.scores2 == SCORE_NOT_PRESENT)
	{
		Com_sprintf (num, sizeof(num), "-");
	}
	else
	{
		Com_sprintf (num, sizeof(num), "%i", cgs.scores2);
	}
	value = CG_Text_Width(num, scale, 0);
	CG_Text_Paint(rect->x + rect->w - value, rect->y + rect->h, scale, color, num, 0, 0, textStyle);
}

// FIXME: team name support
static void CG_DrawRedName(rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, "RED", 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawBlueName(rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, "BLUE", 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawBlueFlagName(rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	int  i;

	for (i = 0 ; i < cgs.maxclients ; i++)
	{
		if (cgs.clientinfo[i].infoValid && (cgs.clientinfo[i].team == TEAM_RED) && (cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)))
		{
			CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
			return;
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawBlueFlagStatus(rectDef_t *rect, qhandle_t shader)
{
//	if(cgs.gametype >= GT_UT_BTB )
//		return;

	if (shader)
	{
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
	}
	else
	{
		gitem_t  *item = BG_FindItemForPowerup( PW_BLUEFLAG );

		if (item)
		{
			vec4_t	color = { 0, 0, 1, 1};
			trap_R_SetColor(color);

			if((cgs.blueflag >= 0) && (cgs.blueflag <= 2))
			{
				CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[cgs.blueflag] );
			}
			else
			{
				CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[0] );
			}
			trap_R_SetColor(NULL);
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawBlueFlagHead(rectDef_t *rect)
{
	int  i;

	for (i = 0 ; i < cgs.maxclients ; i++)
	{
		if (cgs.clientinfo[i].infoValid && (cgs.clientinfo[i].team == TEAM_RED) && (cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)))
		{
			vec3_t	angles;
			VectorClear( angles );
			angles[YAW] = 180 + 20 * sin( cg.time / 650.0 );
			CG_DrawHead( rect->x, rect->y, rect->w, rect->h, 0, angles );
			return;
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawRedFlagName(rectDef_t *rect, float scale, vec4_t color, int textStyle )
{
	int  i;

	for (i = 0 ; i < cgs.maxclients ; i++)
	{
		if (cgs.clientinfo[i].infoValid && (cgs.clientinfo[i].team == TEAM_BLUE) && (cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)))
		{
			CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
			return;
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawRedFlagStatus(rectDef_t *rect, qhandle_t shader)
{
//	if(cgs.gametype >= GT_UT_BTB )
//		return;

	if (shader)
	{
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
	}
	else
	{
		gitem_t  *item = BG_FindItemForPowerup( PW_REDFLAG );

		if (item)
		{
			vec4_t	color = { 1, 0, 0, 1};
			trap_R_SetColor(color);

			if((cgs.redflag >= 0) && (cgs.redflag <= 2))
			{
				CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[cgs.redflag] );
			}
			else
			{
				CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[0] );
			}
			trap_R_SetColor(NULL);
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawRedFlagHead(rectDef_t *rect)
{
	int  i;

	for (i = 0 ; i < cgs.maxclients ; i++)
	{
		if (cgs.clientinfo[i].infoValid && (cgs.clientinfo[i].team == TEAM_BLUE) && (cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)))
		{
			vec3_t	angles;
			VectorClear( angles );
			angles[YAW] = 180 + 20 * sin( cg.time / 650.0 );
			CG_DrawHead( rect->x, rect->y, rect->w, rect->h, 0, angles );
			return;
		}
	}
}

/**
 * $(function)
 */
static void CG_OneFlagStatus(rectDef_t *rect)
{
//	if(cgs.gametype >= GT_UT_BTB )
//		return;

//	if (cgs.gametype != GT_1FCTF && cgs.gametype != GT_UT_BTB) {
//		return;
//	} else
	{
		gitem_t  *item = BG_FindItemForPowerup( PW_NEUTRALFLAG );

		if (item)
		{
			if((cgs.flagStatus >= 0) && (cgs.flagStatus <= 4))
			{
				vec4_t	color = { 1, 1, 1, 1};
				int	index = 0;

				if (cgs.flagStatus == FLAG_TAKEN_RED)
				{
					color[1] = color[2] = 0;
					index	 = 1;
				}
				else if (cgs.flagStatus == FLAG_TAKEN_BLUE)
				{
					color[0] = color[1] = 0;
					index	 = 1;
				}
				else if (cgs.flagStatus == FLAG_DROPPED)
				{
					index = 2;
				}
				trap_R_SetColor(color);
				CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[index] );
			}
		}
	}
}

/**
 * $(function)
 */
static void CG_DrawCTFPowerUp(rectDef_t *rect)
{
//	int		value;
/*	value = cg.snap->ps.stats[STAT_PERSISTANT_POWERUP];
	if ( value ) {
		CG_RegisterItemVisuals( value );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg_items[ value ].icon );
	} */
}

/**
 * $(function)
 */
static void CG_DrawAreaPowerUp(rectDef_t *rect, int align, float special, float scale, vec4_t color)
{
/* URBAN TERROR
	char num[16];
	int		sorted[MAX_POWERUPS];
	int		sortedTime[MAX_POWERUPS];
	int		i, j, k;
	int		active;
	playerState_t	*ps;
	int		t;
	gitem_t *item;
	float	f;
	rectDef_t r2;
	float *inc;
	r2.x = rect->x;
	r2.y = rect->y;
	r2.w = rect->w;
	r2.h = rect->h;

	inc = (align == HUD_VERTICAL) ? &r2.y : &r2.x;

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	// sort the list by time remaining
	active = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( !ps->powerups[ i ] ) {
			continue;
		}
		t = ps->powerups[ i ] - cg.time;
		// ZOID--don't draw if the power up has unlimited time (999 seconds)
		// This is true of the CTF flags
		if ( t <= 0 || t >= 999000) {
			continue;
		}

		// insert into the list
		for ( j = 0 ; j < active ; j++ ) {
			if ( sortedTime[j] >= t ) {
				for ( k = active - 1 ; k >= j ; k-- ) {
					sorted[k+1] = sorted[k];
					sortedTime[k+1] = sortedTime[k];
				}
				break;
			}
		}
		sorted[j] = i;
		sortedTime[j] = t;
		active++;
	}

	// draw the icons and timers
	for ( i = 0 ; i < active ; i++ ) {
		item = BG_FindItemForPowerup( sorted[i] );

		if (item) {
			t = ps->powerups[ sorted[i] ];
			if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
				trap_R_SetColor( NULL );
			} else {
				vec4_t	modulate;

				f = (float)( t - cg.time ) / POWERUP_BLINK_TIME;
				f -= (int)f;
				modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
				trap_R_SetColor( modulate );
			}

			CG_DrawPic( r2.x, r2.y, r2.w * .75, r2.h, trap_R_RegisterShader( item->icon ) );

			Com_sprintf (num, sizeof(num), "%i", sortedTime[i] / 1000);
			CG_Text_Paint(r2.x + (r2.w * .75) + 3 , r2.y + r2.h, scale, color, num, 0, 0, 0);
			*inc += r2.w + special;
		}

	}
	trap_R_SetColor( NULL );
*/
}

/**
 * $(function)
 */
float CG_GetValue(int ownerDraw)
{
	centity_t      *cent;
//	clientInfo_t *ci;
	playerState_t  *ps;

	cent = CG_ENTITIES(cg.snap->ps.clientNum);
	ps   = &cg.snap->ps;

	switch (ownerDraw)
	{
		/*
		  case CG_SELECTEDPLAYER_ARMOR:
		    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
		    return ci->armor;
		    break;
		  case CG_SELECTEDPLAYER_HEALTH:
		    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
		    return ci->health;
		    break;
		*/
		case CG_PLAYER_ARMOR_VALUE:
			return 0;

		case CG_PLAYER_AMMO_VALUE:

			if (cent->currentState.weapon)
			{
				int  weaponSlot = utPSGetWeaponByID ( ps, cent->currentState.weapon );
				return (-1 != weaponSlot) ? UT_WEAPON_GETBULLETS(ps->weaponData[weaponSlot]) : 0;
			}
			break;

		case CG_PLAYER_SCORE:
			return cg.snap->ps.persistant[PERS_SCORE];

			break;

		case CG_PLAYER_HEALTH:
			return ps->stats[STAT_HEALTH];

			break;

		case CG_RED_SCORE:
			return cgs.scores1;

			break;

		case CG_BLUE_SCORE:
			return cgs.scores2;

			break;

		default:
			break;
	}
	return -1;
}

/**
 * $(function)
 */
qboolean CG_OtherTeamHasFlag(void)
{
	if (cgs.gametype == GT_CTF) //cgs.gametype == GT_UT_BTB || cgs.gametype == GT_UT_C4  ) {
	{
		int  team = cg.snap->ps.persistant[PERS_TEAM];
		/*if ( cgs.gametype == GT_UT_BTB) {
			if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_BLUE) {
				return qtrue;
			} else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_RED) {
				return qtrue;
			} else {
				return qfalse;
			}
		} else
		*/
		{
			if ((team == TEAM_RED) && (cgs.redflag == FLAG_TAKEN))
			{
				return qtrue;
			}
			else if (team == TEAM_BLUE && cgs.blueflag == FLAG_TAKEN)
			{
				return qtrue;
			}
			else
			{
				return qfalse;
			}
		}
	}
	return qfalse;
}

/**
 * $(function)
 */
qboolean CG_YourTeamHasFlag(void)
{
	if (cgs.gametype == GT_CTF) //
	{	//cgs.gametype == GT_UT_BTB || cgs.gametype == GT_UT_C4 )
		int  team = cg.snap->ps.persistant[PERS_TEAM];
//		if ( cgs.gametype == GT_UT_BTB)
//		{
//			if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_RED) {
//				return qtrue;
//			} else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_BLUE) {
//				return qtrue;
//			} else {
//				return qfalse;
//			}
//		}else
		{
			if ((team == TEAM_RED) && (cgs.blueflag == FLAG_TAKEN))
			{
				return qtrue;
			}
			else if (team == TEAM_BLUE && cgs.redflag == FLAG_TAKEN)
			{
				return qtrue;
			}
			else
			{
				return qfalse;
			}
		}
	}
	return qfalse;
}

// THINKABOUTME: should these be exclusive or inclusive..
//
qboolean CG_OwnerDrawVisible(int flags)
{
	if (flags & CG_SHOW_SPECTATORS)
	{
		return sortedTeamCounts[TEAM_SPECTATOR] == 0 ? qfalse : qtrue;
	}

/*
	if (flags & CG_SHOW_TEAMINFO) {
		return (cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
	}

	if (flags & CG_SHOW_NOTEAMINFO) {
		return !(cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
	}
*/

	if (flags & CG_SHOW_OTHERTEAMHASFLAG)
	{
		return CG_OtherTeamHasFlag();
	}

	if (flags & CG_SHOW_YOURTEAMHASENEMYFLAG)
	{
		return CG_YourTeamHasFlag();
	}

	if (flags & (CG_SHOW_BLUE_TEAM_HAS_REDFLAG | CG_SHOW_RED_TEAM_HAS_BLUEFLAG))
	{
		if ((flags & CG_SHOW_BLUE_TEAM_HAS_REDFLAG) && ((cgs.redflag == FLAG_TAKEN) || (cgs.flagStatus == FLAG_TAKEN_RED)))
		{
			return qtrue;
		}
		else if ((flags & CG_SHOW_RED_TEAM_HAS_BLUEFLAG) && (cgs.blueflag == FLAG_TAKEN || cgs.flagStatus == FLAG_TAKEN_BLUE))
		{
			return qtrue;
		}
		return qfalse;
	}

	if (flags & CG_SHOW_ANYTEAMGAME)
	{
		if((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
		{
			return qtrue;
		}
	}

	if (flags & CG_SHOW_ANYNONTEAMGAME)
	{
		if((cgs.gametype < GT_TEAM) || (cgs.gametype == GT_JUMP))
		{
			return qtrue;
		}
	}

	if (flags & CG_SHOW_CTF)
	{
		if(cgs.gametype == GT_CTF)
		{
			return qtrue;
		}
	}

	if (flags & CG_SHOW_HEALTHCRITICAL)
	{
		if (cg.snap->ps.stats[STAT_HEALTH] < 25)
		{
			return qtrue;
		}
	}

	if (flags & CG_SHOW_HEALTHOK)
	{
		if (cg.snap->ps.stats[STAT_HEALTH] > 25)
		{
			return qtrue;
		}
	}

	if (flags & CG_SHOW_DURINGINCOMINGVOICE)
	{
	}

	if (flags & CG_SHOW_IF_PLAYER_HAS_FLAG)
	{
		if (utPSHasItem ( &cg.snap->ps, UT_ITEM_REDFLAG ) || utPSHasItem ( &cg.snap->ps, UT_ITEM_BLUEFLAG ) || utPSHasItem ( &cg.snap->ps, UT_ITEM_NEUTRALFLAG ))
		{
			return qtrue;
		}
	}
	return qfalse;
}

/**
 * $(function)
 */
static void CG_DrawPlayerHasFlag(rectDef_t *rect, qboolean force2D)
{
	int  adj = (force2D) ? 0 : 2;

	if(utPSHasItem ( &cg.predictedPlayerState, UT_ITEM_REDFLAG ))
	{
		CG_DrawFlagModel( rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_RED, force2D);
	}
	else if(utPSHasItem ( &cg.predictedPlayerState, UT_ITEM_BLUEFLAG ))
	{
		CG_DrawFlagModel( rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_BLUE, force2D);
	}
	else if(utPSHasItem ( &cg.predictedPlayerState, UT_ITEM_NEUTRALFLAG ))
	{
		CG_DrawFlagModel( rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_FREE, force2D);
	}
}

/**
 * $(function)
 */
const char *CG_GetKillerText(void)
{
	const char  *s = "";

	if (cg.killerName[0])
	{
		s = va("Fragged by %s", cg.killerName );
	}
	return s;
}

/**
 * $(function)
 */
static void CG_DrawKiller(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	// fragged by ... line
	if (cg.killerName[0])
	{
		int  x = rect->x + rect->w / 2;
		CG_Text_Paint(x - CG_Text_Width(CG_GetKillerText(), scale, 0) / 2, rect->y + rect->h, scale, color, CG_GetKillerText(), 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_DrawCapFragLimit(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
	int  limit = (cgs.gametype >= GT_CTF) ? cgs.capturelimit : cgs.fraglimit;

	CG_Text_Paint(rect->x, rect->y, scale, color, va("%2i", limit), 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_Draw1stPlace(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
	if (cgs.scores1 != SCORE_NOT_PRESENT)
	{
		CG_Text_Paint(rect->x, rect->y, scale, color, va("%2i", cgs.scores1), 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_Draw2ndPlace(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
	if (cgs.scores2 != SCORE_NOT_PRESENT)
	{
		CG_Text_Paint(rect->x, rect->y, scale, color, va("%2i", cgs.scores2), 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_ColorFromOwnerdrawParam ( int ownerdrawParam, vec4_t color )
{
	utSkins_t skin;
	switch (ownerdrawParam)
	{
		case UI_TEAM_RED:
			skin = CG_WhichSkin(TEAM_RED);
			break;
		case UI_TEAM_BLUE:
			skin = CG_WhichSkin(TEAM_BLUE);
			break;
		case UI_TEAM_FREE:
			skin = CG_WhichSkin(TEAM_FREE);
			break;
		case UI_TEAM_SPEC:
			color[0] = 0.8f;
			color[1] = 0.8f;
			color[2] = 0.8f;
			color[3] = 1.0f;
			return ;
	}

	color[0] = skinInfos[skin].ui_teamscore_color[0];
	color[1] = skinInfos[skin].ui_teamscore_color[1];
	color[2] = skinInfos[skin].ui_teamscore_color[2];
	color[3] = skinInfos[skin].ui_teamscore_color[3];
}

/**
 * $(function)
 */
void CG_GetRectOffset(int offsetID, rectDef_t *rect )
{
	switch (offsetID)
	{
		case RECT_OFFSET_TEAM_BLUE:
			rect->y += (sortedTeamOffsets[TEAM_RED] * 10);
			break;

		case RECT_OFFSET_TEAM_SPEC:
			rect->y += (sortedTeamOffsets[TEAM_RED] * 10);
			rect->y += (sortedTeamOffsets[TEAM_BLUE] * 10);
			rect->y += (sortedTeamOffsets[TEAM_FREE] * 10);
			break;
	}
}



/**
 * $(function)
 */
static void CG_DrawScoreboardNameList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam ) {
	int	i;
	vec4_t	color;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++)
	{
		CG_Text_Paint(rect->x, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].name, 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_DrawScoreboardStatusList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam )
{
	int	   i;
	vec4_t	   color;
	vec4_t	   col;
	utSkins_t  sidx;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++)
	{
		if (cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].substitute)
		{
			CG_Text_Paint(rect->x + 1, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "SUB", 0, 0, textStyle);
		}
		else if (cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].ghost)
		{
			CG_Text_Paint(rect->x + 1, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "DEAD", 0, 0, textStyle);
		}
		else if (cgs.gametype == GT_UT_ASSASIN && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].team) && cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].teamLeader)
		{
			CG_Text_Paint(rect->x + 1, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "LEADER", 0, 0, textStyle);
		}
		else if (cgs.gametype == GT_CTF && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].team) && cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].hasFlag)
		{
			CG_Text_Paint(rect->x + 1, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "FLAG", 0, 0, textStyle);
		}
		else if (cgs.gametype == GT_UT_BOMB && (cgs.clientinfo[cg.predictedPlayerState.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.predictedPlayerState.clientNum].team == cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].team) && cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].hasBomb)
		{
			CG_Text_Paint(rect->x + 1, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "BOMB", 0, 0, textStyle);
		}
		else if (cgs.gametype == GT_JUMP && cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].jumprun)
		{
			int w = cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].jumpWay; //@r00t:Addded JumpWayColors
			if (!w) {
				CG_Text_Paint(rect->x + 2, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, "RDY", 0, 0, textStyle);
			} else {
				CG_Text_Paint(rect->x + 2, rect->y + rect->h + i * rect->h, scale, g_color_table[(w-1)%8], "RUN", 0, 0, textStyle);
			}
		}

		{
			//int  team = cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].team;
			//col[0]=bg_colors[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client][0]/255;
			//col[1]=bg_colors[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client][1]/255;
			//col[2]=bg_colors[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client][2]/255;
			col[0] = cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].armbandRGB[0] / 255;
			col[1] = cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].armbandRGB[1] / 255;
			col[2] = cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].armbandRGB[2] / 255;
			col[3] = 1;

			if (cg.g_armbands==1) {
				sidx = CG_WhichSkin(cgs.clientinfo[cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client].team);
				col[0] = skinInfos[sidx].defaultArmBandColor[0] / 255;
				col[1] = skinInfos[sidx].defaultArmBandColor[1] / 255;
				col[2] = skinInfos[sidx].defaultArmBandColor[2] / 255;
			}
			CG_FillRect(rect->x - 8, rect->y + 2 + i * rect->h, 8, 8, col);
		}

	}
}

/**
 * $(function)
 */
static void CG_DrawScoreboardTimeList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam )  {

	int	i;
	vec4_t	color;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {
		char  *s = va("%i", cg.scores[sortedTeamPlayers[ownerdrawParam][i]].time);
		int   w  = CG_Text_Width ( s, scale, 0 );
		CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, s, 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_DrawScoreboardKillList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam) {

	int	i;
	vec4_t	color;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {
		char  *s = va("%i", cg.scores[sortedTeamPlayers[ownerdrawParam][i]].score);
		int   w  = CG_Text_Width ( s, scale, 0 );
		CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, s, 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
static void CG_DrawScoreboardJumpBestTimeList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam) {

	int	i, w, msec, seconds, mins, hours, wnone;
	char	*s;
	vec4_t	color;
	//vec4_t	waycolor;

//	  CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );
	wnone = CG_Text_Width ( "---", scale, 0 );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {

		if (cg.scores[sortedTeamPlayers[ownerdrawParam][i]].jumpBestTime > 0) {
			msec = cg.scores[sortedTeamPlayers[ownerdrawParam][i]].jumpBestTime;
			seconds = msec / 1000;
			msec -= seconds * 1000;
			mins = seconds / 60;
			seconds -=mins * 60;
			hours = mins / 60;
			mins -= hours * 60;
			s = va("%01d:%02d:%02d.%03d", hours, mins, seconds, msec);
			w = CG_Text_Width ( s, scale, 0 );  //@r00t:Addded JumpWayColors
			CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, g_color_table[(cg.scores[sortedTeamPlayers[ownerdrawParam][i]].jumpBestWay-1)%8], s, 0, 0, textStyle);
//			  CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, /*cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color*/waycolor, s, 0, 0, textStyle);
		} else {
			CG_Text_Paint(rect->x - wnone, rect->y + rect->h + i * rect->h, scale, color, "---", 0, 0, textStyle);
		}
	}
}


/**
 * $(function)
 */
static void CG_DrawScoreboardDeathList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam ) {

	int	i;
	vec4_t	color;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {
		char  *s = va("%i", cg.scores[sortedTeamPlayers[ownerdrawParam][i]].deaths);
		int   w  = CG_Text_Width ( s, scale, 0 );
		CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, s, 0, 0, textStyle);
	}
}

//@Barbatos //@Fenix
static void CG_DrawScoreboardAccountsList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam) {

	int	i, w;
	vec4_t	color;
	char	*s;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {
		s = va("%s", cg.scores[sortedTeamPlayers[ownerdrawParam][i]].account);
		w  = CG_Text_Width ( s, scale, 0 );
		CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, s, 0, 0, textStyle);
	}

}

/**
 * $(function)
 */
static void CG_DrawScoreboardPingList (rectDef_t *rect, float scale, int textStyle, int ownerdrawParam ) {

	int	i;
	vec4_t	color;

	CG_ColorFromOwnerdrawParam ( ownerdrawParam, color );

	for (i = 0; i < sortedTeamCounts[ownerdrawParam] && i < sortedTeamOffsets[ownerdrawParam]; i++) {
		char  *s = va("%i", cg.scores[sortedTeamPlayers[ownerdrawParam][i]].ping);
		int   w  = CG_Text_Width ( s, scale, 0 );
		CG_Text_Paint(rect->x - w, rect->y + rect->h + i * rect->h, scale, cg.scores[sortedTeamPlayers[ownerdrawParam][i]].client == cg.snap->ps.clientNum ? colorWhite : color, s, 0, 0, textStyle);
	}
}

/**
 * $(function)
 */
const char *CG_GetGameStatusTextOld(void)
{
	const char  *p = "";
	int	    redS, blueS, half;

	p = va("");

	if (strlen(cg.roundstat) > 0)
	{
		sscanf(cg.roundstat, "%i %i %i", &half, &redS, &blueS);

		if (half > 0)
		{
			if (redS == blueS)
			{
				p = va("(First Half Tied at %i)", blueS);
			}
			else
			if (redS > blueS)
			{
				p = va("(First Half to Red %i to %i)", redS, blueS);
			}
			else
			{
				p = va("(First Half to Blue %i to %i)", blueS, redS);
			}
		}
	}

	//p=oldscore;
	return p;
}

/**
 * $(function)
 */
const char *CG_GetGameStatusText(void)
{
	const char  *s = "";
	const char  *info;
	char	    stat[512];
	int	    redS, blueS, half;
	int		msec, seconds, mins, hours;
	//int		j;
	char		*t;

	if ((cgs.gametype < GT_TEAM) || (cgs.gametype == GT_JUMP)) {

		if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
			if (cgs.gametype == GT_JUMP) {

				t = "0:00:00.000"; // -> Initializing empty string.
				msec = cg.snap->ps.persistant[PERS_JUMPBESTTIME];

				if (msec > 0) {
					seconds = msec / 1000;
					msec -= seconds * 1000;
					mins = seconds / 60;
					seconds -=mins * 60;
					hours = mins / 60;
					mins -= hours * 60;
					t = va("%01d:%02d:%02d.%03d", hours, mins, seconds, msec);
				}

				s = va("%s place with %s", CG_PlaceString( (cg.snap->ps.persistant[PERS_RANK]&(RANK_TIED_FLAG-1)) + 1 ), t );

			} else {
				s = va("%s place with %i", CG_PlaceString( (cg.snap->ps.persistant[PERS_RANK]&(RANK_TIED_FLAG-1)) + 1 ), cg.snap->ps.persistant[PERS_SCORE] );
			}
		}

	}
	else
	{
		info = CG_ConfigString( CS_SERVERINFO );
		Com_sprintf(stat, 512, "");

		if (atoi( Info_ValueForKey( info, "g_swaproles" )))
		{
			if (strlen(cg.roundstat) > 0)
			{
				sscanf(cg.roundstat, "%i %i %i", &half, &redS, &blueS);

				if (half == 0)
				{
					Com_sprintf(stat, 512, "First Half,");
				}
				else
				{
					Com_sprintf(stat, 512, "Second Half,");
				}
			}
		}

		if (cg.teamScores[0] == cg.teamScores[1])
		{
			s = va("%s Teams are tied at %i", stat, cg.teamScores[0]);
		}
		else if (cg.teamScores[0] >= cg.teamScores[1])
		{
			s = va("%s %s leads %s, %i to %i",
				stat,
				skinInfos[CG_WhichSkin(TEAM_RED)].defaultTeamColorName,
				skinInfos[CG_WhichSkin(TEAM_BLUE)].defaultTeamColorName,
				cg.teamScores[0], cg.teamScores[1]);
		}
		else
		{
			s = va("%s %s leads %s, %i to %i",
				stat,
				skinInfos[CG_WhichSkin(TEAM_BLUE)].defaultTeamColorName,
				skinInfos[CG_WhichSkin(TEAM_RED)].defaultTeamColorName,
				cg.teamScores[0], cg.teamScores[1]);
		}
	}
	return s;
}

/**
 * $(function)
 */
static void CG_DrawGameStatus(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, CG_GetGameStatusText(), 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawGameStatusOld(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, CG_GetGameStatusTextOld(), 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawTeamScore(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerdrawParam )
{
	static float  colorWhite[4] = { 1.0f,  1.0f,  1.0f,  1.0f };

	if ((cgs.gametype >= GT_TEAM) && (cgs.gametype != GT_JUMP))
	{
		char  *s = va("%i", cg.teamScores[ownerdrawParam - 1]);
		int   w  = CG_Text_Width(s, scale, 0);
		if (ownerdrawParam == TEAM_RED || ownerdrawParam == TEAM_BLUE) {
			CG_Text_Paint(rect->x + rect->w - w, rect->y + rect->h, scale,
					colorWhite, s, 0, 0, textStyle);
		} else {
			CG_Text_Paint(rect->x + rect->w - w, rect->y + rect->h, scale, color, s, 0, 0, textStyle);			
		}
	}
}

/**
 * $(function)
 */
const char *CG_GameTypeString(void)
{
	if (cgs.gametype == GT_FFA)
	{
		return "Free For All";
	}

	if (cgs.gametype == GT_LASTMAN)
	{
		return "Last Man Standing";
	}

	if (cgs.gametype == GT_JUMP)
	{
		return "Jump";
	}

	else if (cgs.gametype == GT_TEAM)
	{
		return "Team Deathmatch";
	}
	else if (cgs.gametype == GT_UT_SURVIVOR)
	{
		return "Team Survivor";
	}
	else if (cgs.gametype == GT_CTF)
	{
		return "Capture the Flag";
	}/* else if ( cgs.gametype == GT_UT_BTB ) {
		return "Search and Destroy";
	} else if ( cgs.gametype == GT_UT_C4 ) {
		return "Infiltrate";
	} */
	else if (cgs.gametype == GT_UT_ASSASIN)
	{
		return "Follow the Leader";
	}/* else if ( cgs.gametype == GT_UT_COLLECT ) {
		return "Collectors";
	} */
	else if (cgs.gametype == GT_UT_CAH)
	{
		return "Capture and Hold";
	}
	else if(cgs.gametype == GT_UT_BOMB)
	{
		return "Bomb";
	}
	return "";
}
/**
 * $(function)
 */
static void CG_DrawGameType(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	CG_Text_Paint(rect->x, rect->y + rect->h, scale, color, CG_GameTypeString(), 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawGameTime(rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle )
{
	char  *s = CG_GetTimerString ( );
	int   w  = CG_Text_Width ( s, scale, 0 );

	CG_Text_Paint(rect->x + rect->w - w, rect->y + rect->h, scale, color, s, 0, 0, textStyle);
}

/**
 * $(function)
 */
static void CG_DrawTeamName(int team, rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
	char  s[MAX_NAME_LENGTH];
	int   w, h;

	if (team == TEAM_RED || team == TEAM_BLUE)
	{
		// get default team name
		strcpy(s, skinInfos[CG_WhichSkin(team)].defaultTeamName);

		// If we have a valid team name.
		if (team == TEAM_RED && strlen(cg.teamNameRed))
		{
			strcpy(s, cg.teamNameRed);
		}
		if (team == TEAM_BLUE && strlen(cg.teamNameBlue))
		{
			strcpy(s, cg.teamNameBlue);
		}
	}
	else
	{
		strcpy(s, "No Name");
	}

	w = CG_Text_Width(s, scale, 0);
	h = CG_Text_Height(s, scale, 0);
	CG_Text_Paint(rect->x + (rect->w / 2) - (w / 2), rect->y + rect->h - (h / 4), scale, skinInfos[CG_WhichSkin(team)].ui_inv_color, s, 0, 0, textStyle);
}

//Iain: new addition
static void CG_DrawPlayerCount(int team, rectDef_t *rect, float scale, vec4_t color, qhandle_t shader, int textStyle ) {

    char    *s;
    int     i, w, t = 0;
    //vec4_t  c;

    for (i = 0; i < cgs.maxclients; i++) {

        if (!cgs.clientinfo[i].infoValid) {
            continue;
        }

        if(cgs.clientinfo[i].team == team) {
            t++;
        }
    }

    s = va("[ %d %s%s ]", t, (team != TEAM_SPECTATOR) ? "player" : "spectator" , (t == 1) ? "" : "s");
    w = CG_Text_Width (s, scale, 0);

    if (team == TEAM_RED || team == TEAM_BLUE || team == TEAM_FREE) {
        CG_Text_Paint(rect->x + rect->w - 10, rect->y + rect->h, scale, skinInfos[CG_WhichSkin(team)].ui_inv_color, s, 0, 0, textStyle);
    } else {
        CG_Text_Paint(rect->x + rect->w - 10, rect->y + rect->h, scale, color, s, 0, 0, textStyle);
    }

}

/**
 * $(function)
 */
#if 0
static void CG_Text_Paint_Limit(float *maxX, float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit)
{
	int	     len, count;
	vec4_t	     newColor;
	glyphInfo_t  *glyph;

	if (text)
	{
		const char  *s	  = text;
		float	    max   = *maxX;
		float	    useScale;
		fontInfo_t  *font = &cgDC.Assets.textFont;

		if (scale <= cg_smallFont.value)
		{
			font = &cgDC.Assets.smallFont;
		}
		else if (scale > cg_bigFont.value)
		{
			font = &cgDC.Assets.bigFont;
		}
		useScale = scale * font->glyphScale;
		trap_R_SetColor( color );
		len	 = strlen(text);

		if ((limit > 0) && (len > limit))
		{
			len = limit;
		}
		count = 0;

		while (s && *s && count < len)
		{
			glyph = &font->glyphs[(int)*s];

			if (Q_IsColorString( s ))
			{
				memcpy( newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s	   += 2;
				continue;
			}
			else
			{
				float  yadj = useScale * glyph->top;

				if (CG_Text_Width(s, useScale, 1) + x > max)
				{
					*maxX = 0;
					break;
				}
				CG_Text_PaintChar(x, y - yadj,
						  glyph->imageWidth,
						  glyph->imageHeight,
						  useScale,
						  glyph->s,
						  glyph->t,
						  glyph->s2,
						  glyph->t2,
						  glyph->glyph);
				x    += (glyph->xSkip * useScale) + adjust;
				*maxX = x;
				count++;
				s++;
			}
		}
		trap_R_SetColor( NULL );
	}
}
#endif

#define PIC_WIDTH 12

/**
 * $(function)
 */
void CG_DrawNewTeamInfo(rectDef_t *rect, float text_x, float text_y, float scale, vec4_t color, qhandle_t shader)
{
/*
	int xx;
	float y;
	int i, j, len, count;
	const char *p;
	vec4_t		hcolor;
	float pwidth, lwidth, maxx, leftOver;
	clientInfo_t *ci;
	gitem_t *item;
	qhandle_t h;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			len = CG_Text_Width( ci->name, scale, 0);
			if (len > pwidth)
				pwidth = len;
		}
	}

	// max location name width
	lwidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_Text_Width(p, scale, 0);
			if (len > lwidth)
				lwidth = len;
		}
	}

	y = rect->y;

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {

			xx = rect->x + 1;
			for (j = 0; j <= PW_NUM_POWERUPS; j++) {
				if (ci->powerups & (1 << j)) {

					item = BG_FindItemForPowerup( j );

					if (item) {
						CG_DrawPic( xx, y, PIC_WIDTH, PIC_WIDTH, trap_R_RegisterShader( item->icon ) );
						xx += PIC_WIDTH;
					}
				}
			}

			// FIXME: max of 3 powerups shown properly
			xx = rect->x + (PIC_WIDTH * 3) + 2;

			CG_GetColorForHealth( ci->health, ci->armor, hcolor );
			trap_R_SetColor(hcolor);
//			CG_DrawPic( xx, y + 1, PIC_WIDTH - 2, PIC_WIDTH - 2, cgs.media.heartShader );

			//Com_sprintf (st, sizeof(st), "%3i %3i", ci->health,	ci->armor);
			//CG_Text_Paint(xx, y + text_y, scale, hcolor, st, 0, 0);

			// draw weapon icon
			xx += PIC_WIDTH + 1;

// weapon used is not that useful, use the space for task
#if 0
			if ( cg_weapons[ci->curWeapon].weaponIcon ) {
				CG_DrawPic( xx, y, PIC_WIDTH, PIC_WIDTH, cg_weapons[ci->curWeapon].weaponIcon );
			} else {
				CG_DrawPic( xx, y, PIC_WIDTH, PIC_WIDTH, cgs.media.deferShader );
			}
#endif

			trap_R_SetColor(NULL);
			if (cgs.orderPending) {
				// blink the icon
				if ( cg.time > cgs.orderTime - 2500 && (cg.time >> 9 ) & 1 ) {
					h = 0;
				} else {
					h = CG_StatusHandle(cgs.currentOrder);
				}
			}	else {
				h = CG_StatusHandle(ci->teamTask);
			}

			if (h) {
				CG_DrawPic( xx, y, PIC_WIDTH, PIC_WIDTH, h);
			}

			xx += PIC_WIDTH + 1;

			leftOver = rect->w - xx;
			maxx = xx + leftOver / 3;



			CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, ci->name, 0, 0);

			p = CG_ConfigString(CS_LOCATIONS + ci->location);
			if (!p || !*p) {
				p = "unknown";
			}

			xx += leftOver / 3 + 2;
			maxx = rect->w - 4;

			CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, p, 0, 0);
			y += text_y + 2;
			if ( y + text_y + 2 > rect->y + rect->h ) {
				break;
			}

		}
	}
*/
}


//
void CG_OwnerDraw( float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int ownerDrawParam, int ownerDrawParam2, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle)
{
			static float  colourGrey[4]  = { 0.5f, 0.5f, 0.5f, 1.00f};
			static float  colourRed[4]   = { 0.5f, 0.0f, 0.0f, 1.00f};
			static float  colourBlue[4]  = { 0.0f, 0.0f, 0.5f, 1.00f};

	rectDef_t  rect;

	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	switch (ownerDraw)
	{
		case CG_PLAYER_AMMO_VALUE:
//			CG_DrawPlayerAmmoValue(&rect, scale, color, shader, textStyle);
			break;

		case CG_PLAYER_SCORE:
			CG_DrawPlayerScore(&rect, scale, color, shader, textStyle);
			break;

		case CG_SURVIVOR_ROUND_WINNER:
			CG_DrawSurvivorRoundWinnerName(&rect, scale, color, shader, textStyle);
			break;

		case CG_PLAYER_HEALTH:
//			CG_DrawPlayerHealth ( &rect, color );
			break;

		case CG_PLAYER_WEAPONMODE:
			CG_DrawPlayerWeaponMode(&rect, scale, color, textStyle);
			break;

		case CG_PLAYER_DAMAGE:
			CG_DrawPlayerDamage(&rect, color);
			break;

		case CG_RED_SCORE:
			CG_DrawRedScore(&rect, scale, color, shader, textStyle);
			break;

		case CG_BLUE_SCORE:
			CG_DrawBlueScore(&rect, scale, color, shader, textStyle);
			break;

		case CG_RED_NAME:
			CG_DrawRedName(&rect, scale, color, textStyle);
			break;

		case CG_BLUE_NAME:
			CG_DrawBlueName(&rect, scale, color, textStyle);
			break;

		case CG_RED_PLAYER_COUNT:
			CG_DrawPlayerCount(TEAM_RED, &rect, scale, color, shader, textStyle);
			break;

		case CG_BLUE_PLAYER_COUNT:
			CG_DrawPlayerCount(TEAM_BLUE, &rect, scale, color, shader, textStyle);
			break;

		case CG_FREE_PLAYER_COUNT:
			CG_DrawPlayerCount(TEAM_FREE, &rect, scale, color, shader, textStyle);
			break;

		case CG_SPECTATOR_PLAYER_COUNT:
            CG_DrawPlayerCount(TEAM_SPECTATOR, &rect, scale, color, shader, textStyle);
            break;

		case CG_BLUE_FLAGHEAD:
			CG_DrawBlueFlagHead(&rect);
			break;

		case CG_BLUE_FLAGSTATUS:
			CG_DrawBlueFlagStatus(&rect, shader);
			break;

		case CG_BLUE_FLAGNAME:
			CG_DrawBlueFlagName(&rect, scale, color, textStyle);
			break;

		case CG_RED_FLAGHEAD:
			CG_DrawRedFlagHead(&rect);
			break;

		case CG_RED_FLAGSTATUS:
			CG_DrawRedFlagStatus(&rect, shader);
			break;

		case CG_RED_FLAGNAME:
			CG_DrawRedFlagName(&rect, scale, color, textStyle);
			break;

		case CG_ONEFLAG_STATUS:
			CG_OneFlagStatus(&rect);
			break;

		case CG_PLAYER_LOCATION:
			CG_DrawPlayerLocation(&rect, scale, color, textStyle);
			break;

		case CG_CTF_POWERUP:
			CG_DrawCTFPowerUp(&rect);
			break;

		case CG_AREA_POWERUP:
			CG_DrawAreaPowerUp(&rect, align, special, scale, color);
			break;

		case CG_PLAYER_STATUS:
			CG_DrawPlayerStatus(&rect);
			break;

		case CG_PLAYER_HASFLAG:
			CG_DrawPlayerHasFlag(&rect, qfalse);
			break;

		case CG_PLAYER_HASFLAG2D:
			CG_DrawPlayerHasFlag(&rect, qtrue);
			break;

		case CG_GAME_TYPE:
			CG_DrawGameType(&rect, scale, color, shader, textStyle);
			break;

		case CG_GAME_TIME:
			if(!cg.intermissionStarted)
			{
				CG_DrawGameTime(&rect, scale, color, shader, textStyle);
			}
			break;

		case CG_GAME_STATUS:
			CG_DrawGameStatus(&rect, scale, color, shader, textStyle);
			break;

		case CG_GAME_STATUS_OLD:
			CG_DrawGameStatusOld(&rect, scale, color, shader, textStyle);
			break;

		case CG_KILLER:
			CG_DrawKiller(&rect, scale, color, shader, textStyle);
			break;

		//case CG_TEAMINFO:
        //    if (cg_currentSelectedPlayer.integer == numSortedTeamPlayers) {
        //        CG_DrawNewTeamInfo(&rect, text_x, text_y, scale, color, shader);
        //    }
		//    break;

		case CG_CAPFRAGLIMIT:
			CG_DrawCapFragLimit(&rect, scale, color, shader, textStyle);
			break;

		case CG_1STPLACE:
			CG_Draw1stPlace(&rect, scale, color, shader, textStyle);
			break;

		case CG_2NDPLACE:
			CG_Draw2ndPlace(&rect, scale, color, shader, textStyle);
			break;

		case CG_SCOREBOARD_NAME_LIST:
			CG_DrawScoreboardNameList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_STATUS_LIST:
			CG_DrawScoreboardStatusList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_TIME_LIST:
			CG_DrawScoreboardTimeList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_KILL_LIST:
			CG_DrawScoreboardKillList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_DEATH_LIST:
			CG_DrawScoreboardDeathList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_PING_LIST:
			CG_DrawScoreboardPingList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		//@Barbatos - auth system
		case CG_SCOREBOARD_ACCOUNTS_LIST:
			CG_DrawScoreboardAccountsList( &rect, scale, textStyle, ownerDrawParam );
			break;

		//@Fenix
		case CG_SCOREBOARD_BESTTIME_LIST:
			CG_DrawScoreboardJumpBestTimeList ( &rect, scale, textStyle, ownerDrawParam );
			break;

		case CG_SCOREBOARD_TEAM_SCORE:
			CG_DrawTeamScore(&rect, scale, color, textStyle, ownerDrawParam);
			break;

		case CG_TEAM_NAME_RED:
			CG_DrawTeamName(TEAM_RED, &rect, scale, color, shader, textStyle);
			break;

		case CG_TEAM_NAME_BLUE:
			CG_DrawTeamName(TEAM_BLUE, &rect, scale, color, shader, textStyle);
			break;

		case CG_TEAM_BAR_RED:
			CG_DrawRect(rect.x,rect.y,rect.w-53,rect.h,1, colourGrey);
			CG_FillRect(rect.x+1,rect.y+1,rect.w-53-2,rect.h-2,skinInfos[CG_WhichSkin(TEAM_RED)].ui_teamscores_teambar_color);
			CG_DrawRect(rect.x+rect.w-50,rect.y,50,rect.h,1, colourGrey);
			CG_FillRect(rect.x+rect.w-50+1,rect.y+1,50-2,rect.h-2,colourRed);
			break;

		case CG_TEAM_BAR_BLUE:
			CG_DrawRect(rect.x,rect.y,rect.w-53,rect.h,1, colourGrey);
			CG_FillRect(rect.x+1,rect.y+1,rect.w-53-2,rect.h-2,skinInfos[CG_WhichSkin(TEAM_BLUE)].ui_teamscores_teambar_color);
			CG_DrawRect(rect.x+rect.w-50,rect.y,50,rect.h,1, colourGrey);
			CG_FillRect(rect.x+rect.w-50+1,rect.y+1,50-2,rect.h-2,colourBlue);
			break;

		case CG_SOLO_BAR:
			CG_FillRect(rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2,
			            skinInfos[CG_WhichSkin(TEAM_FREE)].ui_teamscores_teambar_color);
			break;

		default:
			break;
	}
}

/**
 * $(function)
 */
void CG_MouseEvent(int x, int y)
{
	int  n;

	if (((cg.predictedPlayerState.pm_type == PM_NORMAL) || (cg.predictedPlayerState.stats[STAT_UTPMOVE_FLAGS] & UT_PMF_GHOST)) && (cg.showScores == qfalse))
	{
		trap_Key_SetCatcher(0);
		return;
	}

	cgs.cursorX += x;

	if (cgs.cursorX < 0)
	{
		cgs.cursorX = 0;
	}
	else if (cgs.cursorX > 640)
	{
		cgs.cursorX = 640;
	}

	cgs.cursorY += y;

	if (cgs.cursorY < 0)
	{
		cgs.cursorY = 0;
	}
	else if (cgs.cursorY > 480)
	{
		cgs.cursorY = 480;
	}

	n		 = Display_CursorType(cgs.cursorX, cgs.cursorY);
	cgs.activeCursor = 0;

	if (n == CURSOR_ARROW)
	{
		cgs.activeCursor = cgs.media.selectCursor;
	}
	else if (n == CURSOR_SIZER)
	{
		cgs.activeCursor = cgs.media.sizeCursor;
	}

	if (cgs.capturedItem)
	{
		Display_MouseMove(cgs.capturedItem, x, y);
	}
	else
	{
		Display_MouseMove(NULL, cgs.cursorX, cgs.cursorY);
	}
}

/*
==================
CG_HideTeamMenus
==================

*/
void CG_HideTeamMenu(void)
{
	Menus_CloseByName("teamMenu");
	Menus_CloseByName("getMenu");
}

/*
==================
CG_ShowTeamMenus
==================

*/
void CG_ShowTeamMenu(void)
{
	Menus_OpenByName("teamMenu");
}

/*
==================
CG_EventHandling
==================
 type 0 - no event handling
      1 - team menu
      2 - hud editor

*/
void CG_EventHandling(int type)
{
	cgs.eventHandling = type;

	if (type == CGAME_EVENT_NONE)
	{
		CG_HideTeamMenu();
	}
	else if (type == CGAME_EVENT_TEAMMENU)
	{
		//CG_ShowTeamMenu();
	}
	else if (type == CGAME_EVENT_SCOREBOARD)
	{
	}
}

/**
 * $(function)
 */
void CG_KeyEvent(int key, qboolean down)
{
	if (!down)
	{
		return;
	}

	if ((cg.predictedPlayerState.pm_type == PM_NORMAL) || ((cg.predictedPlayerState.pm_type == PM_SPECTATOR) && (cg.showScores == qfalse)))
	{
		CG_EventHandling(CGAME_EVENT_NONE);
		trap_Key_SetCatcher(0);
		return;
	}

	//if (key == trap_Key_GetKey("teamMenu") || !Display_CaptureItem(cgs.cursorX, cgs.cursorY)) {
	// if we see this then we should always be visible
	//  CG_EventHandling(CGAME_EVENT_NONE);
	//  trap_Key_SetCatcher(0);
	//}

	Display_HandleKey(key, down, cgs.cursorX, cgs.cursorY);

	if (cgs.capturedItem)
	{
		cgs.capturedItem = NULL;
	}
	else
	{
		if ((key == K_MOUSE2) && down)
		{
			cgs.capturedItem = Display_CaptureItem(cgs.cursorX, cgs.cursorY);
		}
	}
}

/**
 * $(function)
 */
int CG_ClientNumFromName(const char *p)
{
	int  i;

	for (i = 0; i < cgs.maxclients; i++)
	{
		if (cgs.clientinfo[i].infoValid && (Q_stricmp(cgs.clientinfo[i].name, p) == 0))
		{
			return i;
		}
	}
	return -1;
}

/**
 * $(function)
 */
void CG_ShowResponseHead(void)
{
	Menus_OpenByName("voiceMenu");
	trap_Cvar_Set("cl_conXOffset", "72");
	cg.voiceTime = cg.time;
}

/**
 * $(function)
 */
void CG_ScoreboardInit ( void )
{
	int  i;

	for (i = TEAM_FREE; i < TEAM_NUM_TEAMS; i++)
	{
		sortedTeamCounts[i] = 0;
	}

	for (i = 0; i < cg.numScores ; i++)
	{
		if (cgs.clientinfo[cg.scores[i].client].infoValid)
		{
			int  team = cgs.clientinfo[cg.scores[i].client].team;
			sortedTeamPlayers[team][sortedTeamCounts[team]] = i;

			sortedTeamCounts[team]				= sortedTeamCounts[team] + 1;
		}
	}

	if ((cgs.gametype < GT_TEAM) || (cgs.gametype == GT_JUMP))
	{
		sortedTeamOffsets[TEAM_FREE]	  = 20;
		sortedTeamOffsets[TEAM_SPECTATOR] = 6;
	}
	else
	{
		sortedTeamOffsets[TEAM_RED]	  = 12;
		sortedTeamOffsets[TEAM_BLUE]	  = 12;
		sortedTeamOffsets[TEAM_SPECTATOR] = 2;

		// Make room for more specs if we need to
		if (sortedTeamCounts[TEAM_SPECTATOR] > sortedTeamOffsets[TEAM_SPECTATOR])
		{
			sortedTeamOffsets[TEAM_RED]	 -= ((sortedTeamCounts[TEAM_SPECTATOR] - sortedTeamOffsets[TEAM_SPECTATOR]) / 2);
			sortedTeamOffsets[TEAM_BLUE]	 -= ((sortedTeamCounts[TEAM_SPECTATOR] - sortedTeamOffsets[TEAM_SPECTATOR]) / 2);
			sortedTeamOffsets[TEAM_SPECTATOR] = sortedTeamCounts[TEAM_SPECTATOR];
		}
		else if (sortedTeamCounts[TEAM_SPECTATOR] == 0)
		{
			sortedTeamOffsets[TEAM_RED]  += 2;
			sortedTeamOffsets[TEAM_BLUE] += 2;
		}
	}
}

/**
 * $(function)
 */
void CG_RunMenuScript(itemDef_t *item, char * *args)
{
}

/**
 * $(function)
 */
void CG_GetTeamColor(vec4_t *color)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED)
	{
		(*color)[0] = 1;
		(*color)[3] = .25f;
		(*color)[1] = (*color)[2] = 0;
	}
	else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE)
	{
		(*color)[0] = (*color)[1] = 0;
		(*color)[2] = 1;
		(*color)[3] = .25f;
	}
	else
	{
		(*color)[0] = (*color)[2] = 0;
		(*color)[1] = .17f;
		(*color)[3] = .25f;
	}
}
