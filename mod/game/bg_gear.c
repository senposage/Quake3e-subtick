/**
 * Filename: bg_gear.c
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2000-2009 FrozenSand
 *
 * This file is part of Urban Terror source code.
 *
 * Definitions shared by both the server game and client game
 * modules because games can change separately from the main system
 * version we need a second version that must match between game and cgame
 */
#include "q_shared.h"
#include "bg_gear.h"
#include "bg_public.h"
#include "../common/c_bmalloc.h"

#define ALPHADIFF ('a' - 'Z' - 1)

/**
 * utGear_Compress
 *
 * Compresses the given gear structure into a string.
 */
void utGear_Compress(utGear_t *gear, char *compressed)
{
	int  i = 0;

	// Weapons are the first three characters
	compressed[i] = 'A' + gear->sidearm;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->primary;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->secondary;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->grenade;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->item1;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->item2;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = 'A' + gear->item3;

	if (compressed[i] > 'Z') {
		compressed[i] += ALPHADIFF;
	}
	compressed[++i] = '\0';
}

#define UPPERCASEBASE 'A'
#define LOWERCASEBASE (UPPERCASEBASE + ALPHADIFF)

/**
 * utGear_Decompress
 *
 * Decompresses the given string into a gear structure
 */
void utGear_Decompress(char *compressed, utGear_t *gear)
{
	// decompress
	if (compressed[0] > 'Z') {
		gear->sidearm = compressed[0] - LOWERCASEBASE;
	} else {
		gear->sidearm = compressed[0] - UPPERCASEBASE;
	}

	if (compressed[1] > 'Z') {
		gear->primary = compressed[1] - LOWERCASEBASE;
	} else {
		gear->primary = compressed[1] - UPPERCASEBASE;
	}

	if (compressed[2] > 'Z') {
		gear->secondary = compressed[2] - LOWERCASEBASE;
	} else {
		gear->secondary = compressed[2] - UPPERCASEBASE;
	}

	if (compressed[3] > 'Z') {
		gear->grenade = compressed[3] - LOWERCASEBASE;
	} else {
		gear->grenade = compressed[3] - UPPERCASEBASE;
	}

	if (compressed[4] > 'Z') {
		gear->item1 = compressed[4] - LOWERCASEBASE;
	} else {
		gear->item1 = compressed[4] - UPPERCASEBASE;
	}

	if (compressed[5] > 'Z') {
		gear->item2 = compressed[5] - LOWERCASEBASE;
	} else {
		gear->item2 = compressed[5] - UPPERCASEBASE;
	}

	if (compressed[6] > 'Z') {
		gear->item3 = compressed[6] - LOWERCASEBASE;
	} else {
		gear->item3 = compressed[6] - UPPERCASEBASE;
	}

	// If not a valid gear setup then use the default
	if (!utGear_Verify(gear)) {
		utGear_Decompress("GLAARWV", gear);
	}
}

/**
 * onOff
 */
char *onOff(int val)
{
	if (!val) {
		return "1";
	} else {
		return "0";
	}
}

/**
 * allowedGear
 */
char *allowedGear(int arg)
{
	char *buf = bmalloc(128);
	//@Fenix - bugfix #748. sizeof(buf) is 4 byte on a 32bit machine and 8 byte on a 64bit machine
	//since it's a pointer to an array of chars. We actually need the size of the allocated memory.
	Com_sprintf(buf, 128,
		    "Nade:%s Sniper:%s Spas:%s Pistol:%s Auto:%s Negev:%s"
#ifdef FLASHNADES
		    "Flash:%s"
#endif
		    ,onOff((arg & GEAR_NADES)),
		    onOff((arg & GEAR_SNIPERS)),
		    onOff((arg & GEAR_SPAS)),
		    onOff((arg & GEAR_PISTOLS)),
		    onOff((arg & GEAR_AUTOS)),
		    onOff((arg & GEAR_NEGEV))
#ifdef FLASHNADES
		    ,onOff((arg & GEAR_FLASH))
#endif
);
	return buf;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Name       : utGear_Verify
// Description: Verifies a gear structure to ensure its validitity
// Author     : Apoxol
///////////////////////////////////////////////////////////////////////////////////////////
qboolean utGear_Verify(utGear_t *gear)
{
	int  items = 1;

	// Verify primary
	switch (gear->primary) {
	case UT_ITEM_LR:
	case UT_ITEM_G36:
	case UT_ITEM_PSG1:
	case UT_ITEM_SR8:
	case UT_ITEM_AK103:
	case UT_ITEM_M4:
	case UT_ITEM_NEGEV:
	case UT_ITEM_HK69:
	case UT_ITEM_SPAS12:
	case UT_ITEM_MP5K:
	case UT_ITEM_UMP45:
	case UT_ITEM_MAC11:
    //case UT_ITEM_BENELLI:
	//case UT_ITEM_P90:
	case UT_ITEM_NONE:
		break;

	default:
		return qfalse;
	}

	// Verify secondary
	switch (gear->secondary) {
	case UT_ITEM_NONE:
		items++;
		break;

	case UT_ITEM_SPAS12:
	case UT_ITEM_MP5K:
	case UT_ITEM_UMP45:
	case UT_ITEM_MAC11:
    //case UT_ITEM_BENELLI:
	//case UT_ITEM_P90:

	/*case UT_ITEM_DUAL_BERETTA:
	case UT_ITEM_DUAL_DEAGLE:
	case UT_ITEM_DUAL_GLOCK:
	case UT_ITEM_DUAL_MAGNUM:*/

		if (gear->primary == UT_ITEM_NEGEV) {
			return qfalse;
		}
		break;

	default:
		return qfalse;
	}

	// Verify sidearm
	switch (gear->sidearm) {
	case UT_ITEM_BERETTA:
	case UT_ITEM_DEAGLE:
	case UT_ITEM_GLOCK:
	case UT_ITEM_COLT1911:
	//case UT_ITEM_MAGNUM:
		// No sidearm allowed when dual pistols are selected for secondary
		/*if ( gear->secondary >= UT_ITEM_DUAL_BERETTA && gear->secondary <= UT_ITEM_DUAL_MAGNUM ) {
			return qfalse;
		}  */
		break;

	default:
		return qfalse;
	}

	// Verify grenade
	switch (gear->grenade) {
	case UT_ITEM_NONE:
		items++;
		break;

	case UT_ITEM_GRENADE_HE:
#ifdef FLASHNADES
	case UT_ITEM_GRENADE_FLASH:
#endif
	case UT_ITEM_GRENADE_SMOKE:
		break;

	default:
		return qfalse;
	}

	// Verify item 1
	switch (gear->item1) {
	case UT_ITEM_VEST:
	case UT_ITEM_NVG:
	case UT_ITEM_MEDKIT:
	case UT_ITEM_SILENCER:
	case UT_ITEM_LASER:
	case UT_ITEM_HELMET:
	case UT_ITEM_AMMO:
		break;

	default:
		return qfalse;
	}

	// Verify item 2
	switch (gear->item2) {
	case UT_ITEM_NONE:
		break;

	case UT_ITEM_VEST:
	case UT_ITEM_NVG:
	case UT_ITEM_MEDKIT:
	case UT_ITEM_SILENCER:
	case UT_ITEM_LASER:
	case UT_ITEM_HELMET:
	case UT_ITEM_AMMO:
		if (items < 2) {
			return qfalse;
		}

		if (gear->item2 == gear->item1) {
			return qfalse;
		}
		break;

	default:
		return qfalse;
	}

	// Verify item 3
	switch (gear->item3) {
	case UT_ITEM_NONE:
		break;

	case UT_ITEM_VEST:
	case UT_ITEM_NVG:
	case UT_ITEM_MEDKIT:
	case UT_ITEM_SILENCER:
	case UT_ITEM_LASER:
	case UT_ITEM_HELMET:
	case UT_ITEM_AMMO:
		if (items < 3) {
			return qfalse;
		}

		if ((gear->item3 == gear->item2) || (gear->item3 == gear->item1)) {
			return qfalse;
		}
		break;

	default:
		return qfalse;
	}

	return qtrue;
}
