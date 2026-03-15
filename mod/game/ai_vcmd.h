// Copyright (C) 1999-2000 Id Software, Inc.
//

/*****************************************************************************
 * name:		ai_vcmd.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_vcmd.c $
 * $Author: density $
 * $Revision: 1.2 $
 * $Modtime: 11/10/99 3:30p $
 * $Date: 2003/01/22 05:35:32 $
 *
 *****************************************************************************/

int  BotVoiceChatCommand(bot_state_t *bs, int mode, char *voicechat);
void BotVoiceChat_Defend(bot_state_t *bs, int client, int mode);
