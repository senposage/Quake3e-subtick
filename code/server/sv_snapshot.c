/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"


/*
=============================================================================

Per-client position ring buffer for sv_bufferMs / sv_velSmooth

=============================================================================
*/

#define SV_SMOOTH_MAX_SLOTS 32

typedef struct {
	vec3_t      origin;
	vec3_t      velocity;
	int         serverTime;
	qboolean    valid;
} svSmoothPos_t;

typedef struct {
	svSmoothPos_t slots[SV_SMOOTH_MAX_SLOTS];
	int           head;
	int           count;
} svSmoothHistory_t;

static svSmoothHistory_t sv_smoothHistory[MAX_CLIENTS];


void SV_SmoothInit( void ) {
	Com_Memset( sv_smoothHistory, 0, sizeof( sv_smoothHistory ) );
}


static void SV_SmoothRecord( int clientNum ) {
	svSmoothHistory_t *hist;
	svSmoothPos_t *slot;
	int idx;

	if ( clientNum < 0 || clientNum >= sv.maxclients )
		return;

	hist = &sv_smoothHistory[clientNum];
	idx = ( hist->head % SV_SMOOTH_MAX_SLOTS + SV_SMOOTH_MAX_SLOTS ) % SV_SMOOTH_MAX_SLOTS;
	slot = &hist->slots[idx];

	if ( svs.clients[clientNum].netchan.remoteAddress.type == NA_BOT ) {
		sharedEntity_t *gent = SV_GentityNum( clientNum );
		VectorCopy( gent->s.pos.trBase, slot->origin );
		VectorCopy( gent->s.pos.trDelta, slot->velocity );
	} else {
		playerState_t *ps = SV_GameClientNum( clientNum );
		VectorCopy( ps->origin, slot->origin );
		VectorCopy( ps->velocity, slot->velocity );
	}

	slot->serverTime = sv.time;
	slot->valid = qtrue;

	hist->head++;
	if ( hist->count < SV_SMOOTH_MAX_SLOTS )
		hist->count++;
}


void SV_SmoothRecordAll( void ) {
	int i;
	for ( i = 0; i < sv.maxclients; i++ ) {
		if ( svs.clients[i].state == CS_ACTIVE ) {
			SV_SmoothRecord( i );
		}
	}
}


static qboolean SV_SmoothGetPosition( int clientNum, int targetTime,
		vec3_t outOrigin, vec3_t outVelocity ) {
	svSmoothHistory_t *hist;
	svSmoothPos_t *before = NULL, *after = NULL;
	int i, idx;
	float frac;

	if ( clientNum < 0 || clientNum >= sv.maxclients )
		return qfalse;

	hist = &sv_smoothHistory[clientNum];
	if ( hist->count == 0 )
		return qfalse;

	for ( i = 0; i < hist->count; i++ ) {
		idx = ( ( hist->head - 1 - i ) % SV_SMOOTH_MAX_SLOTS + SV_SMOOTH_MAX_SLOTS ) % SV_SMOOTH_MAX_SLOTS;

		if ( !hist->slots[idx].valid )
			continue;

		if ( hist->slots[idx].serverTime <= targetTime ) {
			if ( !before || hist->slots[idx].serverTime > before->serverTime )
				before = &hist->slots[idx];
		} else {
			if ( !after || hist->slots[idx].serverTime < after->serverTime )
				after = &hist->slots[idx];
		}

		// Early exit: iterating newest-first.  Once we have both brackets
		// and have passed below before's timestamp, nothing further can
		// improve either bracket.
		if ( before && after && hist->slots[idx].serverTime < before->serverTime )
			break;
	}

	if ( !before && !after )
		return qfalse;

	if ( !before ) {
		VectorCopy( after->origin, outOrigin );
		VectorCopy( after->velocity, outVelocity );
		return qtrue;
	}
	if ( !after ) {
		VectorCopy( before->origin, outOrigin );
		VectorCopy( before->velocity, outVelocity );
		return qtrue;
	}

	if ( after->serverTime == before->serverTime ) {
		frac = 0.0f;
	} else {
		frac = (float)( targetTime - before->serverTime )
			/ (float)( after->serverTime - before->serverTime );
	}
	if ( frac < 0.0f ) frac = 0.0f;
	if ( frac > 1.0f ) frac = 1.0f;

	outOrigin[0] = before->origin[0] + frac * ( after->origin[0] - before->origin[0] );
	outOrigin[1] = before->origin[1] + frac * ( after->origin[1] - before->origin[1] );
	outOrigin[2] = before->origin[2] + frac * ( after->origin[2] - before->origin[2] );

	outVelocity[0] = before->velocity[0] + frac * ( after->velocity[0] - before->velocity[0] );
	outVelocity[1] = before->velocity[1] + frac * ( after->velocity[1] - before->velocity[1] );
	outVelocity[2] = before->velocity[2] + frac * ( after->velocity[2] - before->velocity[2] );

	return qtrue;
}


static qboolean SV_SmoothGetAverageVelocity( int clientNum, int windowMs,
		vec3_t outVelocity ) {
	svSmoothHistory_t *hist;
	int i, idx;
	int cutoff;
	float weight, totalWeight;
	vec3_t weightedSum;

	if ( clientNum < 0 || clientNum >= sv.maxclients )
		return qfalse;

	hist = &sv_smoothHistory[clientNum];
	if ( hist->count == 0 )
		return qfalse;

	// Exponential weighted average: each step back in time halves the weight.
	// This keeps the most-recent velocity dominant so that consecutive snapshots
	// produce very similar trDelta values (no per-tick 50/50 flip as the uniform
	// sliding window would cause), eliminating the trajectory kink at every
	// snapshot boundary that manifests as blur/micro-stutter for fast-moving players.
	cutoff = sv.time - windowMs;
	VectorClear( weightedSum );
	totalWeight = 0.0f;
	weight = 1.0f;

	for ( i = 0; i < hist->count; i++ ) {
		idx = ( ( hist->head - 1 - i ) % SV_SMOOTH_MAX_SLOTS + SV_SMOOTH_MAX_SLOTS ) % SV_SMOOTH_MAX_SLOTS;

		if ( !hist->slots[idx].valid )
			continue;
		if ( hist->slots[idx].serverTime < cutoff )
			break;

		weightedSum[0] += hist->slots[idx].velocity[0] * weight;
		weightedSum[1] += hist->slots[idx].velocity[1] * weight;
		weightedSum[2] += hist->slots[idx].velocity[2] * weight;
		totalWeight += weight;
		weight *= 0.5f;
	}

	if ( totalWeight < 1e-6f )
		return qfalse;

	outVelocity[0] = weightedSum[0] / totalWeight;
	outVelocity[1] = weightedSum[1] / totalWeight;
	outVelocity[2] = weightedSum[2] / totalWeight;

	return qtrue;
}


/*
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4	sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1	svc_snapshot
4	last client reliable command
4	serverTime
1	lastframe for delta compression
1	snapFlags
1	areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entityState_t list to the message.
=============
*/
static void SV_EmitPacketEntities( const clientSnapshot_t *from, const clientSnapshot_t *to, msg_t *msg ) {
	entityState_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;

	// generate the delta update
	if ( !from ) {
		from_num_entities = 0;
	} else {
		from_num_entities = from->num_entities;
	}

	newent = NULL;
	oldent = NULL;
	newindex = 0;
	oldindex = 0;
	while ( newindex < to->num_entities || oldindex < from_num_entities ) {
		if ( newindex >= to->num_entities ) {
			newnum = MAX_GENTITIES+1;
		} else {
			newent = to->ents[ newindex ];
			newnum = newent->number;
		}

		if ( oldindex >= from_num_entities ) {
			oldnum = MAX_GENTITIES+1;
		} else {
			oldent = from->ents[ oldindex ];
			oldnum = oldent->number;
		}

		if ( newnum == oldnum ) {
			// delta update from old position
			// because the force parm is qfalse, this will not result
			// in any bytes being emitted if the entity has not changed at all
			MSG_WriteDeltaEntity (msg, oldent, newent, qfalse );
			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum ) {
			// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (msg, &sv.svEntities[newnum].baseline, newent, qtrue );
			newindex++;
			continue;
		}

		if ( newnum > oldnum ) {
			// the old entity isn't present in the new message
			MSG_WriteDeltaEntity (msg, oldent, NULL, qtrue );
			oldindex++;
			continue;
		}
	}

	MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );	// end of packetentities
}


/*
==================
SV_WriteSnapshotToClient
==================
*/
static void SV_WriteSnapshotToClient( const client_t *client, msg_t *msg ) {
	const clientSnapshot_t	*oldframe;
	const clientSnapshot_t	*frame;
	int					lastframe;
	int					i;
	int					snapFlags;

	// this is the snapshot we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];

	// try to use a previous frame as the source for delta compressing the snapshot
	if ( /* client->deltaMessage <= 0 || */ client->state != CS_ACTIVE ) {
		// client is asking for a retransmit
		oldframe = NULL;
		lastframe = 0;
	} else if ( client->netchan.outgoingSequence - client->deltaMessage >= (PACKET_BACKUP - 3) ) {
		// client hasn't gotten a good message through in a long time
		if ( com_developer->integer ) {
			if ( client->deltaMessage != client->netchan.outgoingSequence - ( PACKET_BACKUP + 1 ) ) {
				Com_Printf( "%s: Delta request from out of date packet.\n", client->name );
			}
		}
		oldframe = NULL;
		lastframe = 0;
	}
#ifdef USE_SERVER_DEMO
	else if ( client->demo_recording && client->demo_deltas <= 0 ) {
		// if we're recording this client, force full frames every now and then
		client_t *cl = (client_t *)client;
		oldframe = NULL;
		lastframe = 0;
		if ( cl->demo_backoff < 1024 ) {
			cl->demo_backoff *= 2;
		}
		cl->demo_deltas = cl->demo_backoff;
	}
#endif
	else {
#ifdef USE_SERVER_DEMO
		if ( client->demo_recording ) {
			((client_t *)client)->demo_deltas--;
		}
#endif
		// we have a valid snapshot to delta from
		oldframe = &client->frames[ client->deltaMessage & PACKET_MASK ];
		lastframe = client->netchan.outgoingSequence - client->deltaMessage;
		// we may refer on outdated frame
		if ( oldframe->frameNum - svs.lastValidFrame < 0 ) {
			Com_DPrintf( "%s: Delta request from out of date frame.\n", client->name );
			oldframe = NULL;
			lastframe = 0;
		}
	}

#ifdef USE_SERVER_DEMO
	// start recording only once there's a non-delta frame to start with
	if ( !oldframe && client->demo_recording && client->demo_waiting ) {
		((client_t *)client)->demo_waiting = qfalse;
	}
#endif

	MSG_WriteByte( msg, svc_snapshot );

	// NOTE, MRE: now sent at the start of every message from server to client
	// let the client know which reliable clientCommands we have received
	//MSG_WriteLong( msg, client->lastClientCommand );

	// send over the current server time so the client can drift
	// its view of time to try to match
	if ( client->oldServerTime ) {
		// The server has not yet got an acknowledgement of the
		// new gamestate from this client, so continue to send it
		// a time as if the server has not restarted. Note from
		// the client's perspective this time is strictly speaking
		// incorrect, but since it'll be busy loading a map at
		// the time it doesn't really matter.
		MSG_WriteLong( msg, sv.time + client->oldServerTime );
	} else {
		MSG_WriteLong( msg, sv.time );
	}

	// what we are delta'ing from
	MSG_WriteByte( msg, lastframe );

	snapFlags = svs.snapFlagServerBit;
	if ( client->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	if ( client->state != CS_ACTIVE ) {
		snapFlags |= SNAPFLAG_NOT_ACTIVE;
	}

	MSG_WriteByte (msg, snapFlags);

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	MSG_WriteData (msg, frame->areabits, frame->areabytes);

	// don't send any changes to zombies
	if ( client->state <= CS_ZOMBIE ) {
		// playerstate
		MSG_WriteByte( msg, 0 ); // # of changes
		MSG_WriteBits( msg, 0, 1 ); // no array changes
		// packet entities
		MSG_WriteBits( msg, (MAX_GENTITIES-1), GENTITYNUM_BITS );
		return;
	}

	// delta encode the playerstate
	if ( oldframe ) {
		MSG_WriteDeltaPlayerstate( msg, &oldframe->ps, &frame->ps );
	} else {
		MSG_WriteDeltaPlayerstate( msg, NULL, &frame->ps );
	}

	// delta encode the entities
	SV_EmitPacketEntities (oldframe, frame, msg);

	// padding for rate debugging
	if ( sv_padPackets->integer ) {
		for ( i = 0 ; i < sv_padPackets->integer ; i++ ) {
			MSG_WriteByte (msg, svc_nop);
		}
	}
}


/*
==================
SV_UpdateServerCommandsToClient

(re)send all server commands the client hasn't acknowledged yet
==================
*/
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) {
	int i, n;

	// write any unacknowledged serverCommands
	n = client->reliableSequence - client->reliableAcknowledge;

	for ( i = 0; i < n; i++ ) {
		const int index = client->reliableAcknowledge + 1 + i;
		MSG_WriteByte( msg, svc_serverCommand );
		MSG_WriteLong( msg, index );
		MSG_WriteString( msg, client->reliableCommands[ index & (MAX_RELIABLE_COMMANDS-1) ] );
	}
}

/*
=============================================================================

Build a client snapshot structure

=============================================================================
*/


typedef int entityNum_t;
typedef struct {
	int		numSnapshotEntities;
	entityNum_t	snapshotEntities[ MAX_SNAPSHOT_ENTITIES ];
	qboolean unordered;
} snapshotEntityNumbers_t;


/*
=============
SV_SortEntityNumbers

Insertion sort is about 10 times faster than quicksort for our task
=============
*/
static void SV_SortEntityNumbers( entityNum_t *num, const int size ) {
	entityNum_t tmp;
	int i, d;
	for ( i = 1 ; i < size; i++ ) {
		d = i;
		while ( d > 0 && num[d] < num[d-1] ) {
			tmp = num[d];
			num[d] = num[d-1];
			num[d-1] = tmp;
			d--;
		}
	}
#ifdef _DEBUG
	// consistency check for delta encoding
	for ( i = 1 ; i < size; i++ ) {
		if ( num[i-1] >= num[i] ) {
			Com_Error( ERR_DROP, "%s: invalid entity number %i", __func__, num[ i ] );
		}
	}
#endif
}


/*
===============
SV_AddIndexToSnapshot
===============
*/
static void SV_AddIndexToSnapshot( svEntity_t *svEnt, int index, snapshotEntityNumbers_t *eNums ) {

	svEnt->snapshotCounter = sv.snapshotCounter;

	// if we are full, silently discard entities
	if ( eNums->numSnapshotEntities >= MAX_SNAPSHOT_ENTITIES ) {
		return;
	}

	eNums->snapshotEntities[ eNums->numSnapshotEntities ] = index;
	eNums->numSnapshotEntities++;
}


/*
===============
SV_AddEntitiesVisibleFromPoint
===============
*/
static void SV_AddEntitiesVisibleFromPoint( const vec3_t origin, clientSnapshot_t *frame,
									snapshotEntityNumbers_t *eNums, qboolean portal ) {
	int		e, i;
	sharedEntity_t *ent;
	svEntity_t	*svEnt;
	entityState_t  *es;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	byte	*clientpvs;
	byte	*bitvector;

	// during an error shutdown message we may need to transmit
	// the shutdown message after the server has shutdown, so
	// specifically check for it
	if ( sv.state == SS_DEAD ) {
		return;
	}

	leafnum = CM_PointLeafnum (origin);
	clientarea = CM_LeafArea (leafnum);
	clientcluster = CM_LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits( frame->areabits, clientarea );

	clientpvs = CM_ClusterPVS (clientcluster);

	for ( e = 0 ; e < svs.currFrame->count; e++ ) {
		es = svs.currFrame->ents[ e ];
		ent = SV_GentityNum( es->number );

		// entities can be flagged to be sent to only one client
		if ( ent->r.svFlags & SVF_SINGLECLIENT ) {
			if ( ent->r.singleClient != frame->ps.clientNum ) {
				continue;
			}
		}
		// entities can be flagged to be sent to everyone but one client
		if ( ent->r.svFlags & SVF_NOTSINGLECLIENT ) {
			if ( ent->r.singleClient == frame->ps.clientNum ) {
				continue;
			}
		}
		// entities can be flagged to be sent to a given mask of clients
		if ( ent->r.svFlags & SVF_CLIENTMASK ) {
			if (frame->ps.clientNum >= 32)
				Com_Error( ERR_DROP, "SVF_CLIENTMASK: clientNum >= 32" );
			if (~ent->r.singleClient & (1 << frame->ps.clientNum))
				continue;
		}

		svEnt = &sv.svEntities[ es->number ];

		// don't double add an entity through portals
		if ( svEnt->snapshotCounter == sv.snapshotCounter ) {
			continue;
		}

		// broadcast entities are always sent
		if ( ent->r.svFlags & SVF_BROADCAST ) {
			SV_AddIndexToSnapshot( svEnt, e, eNums );
			continue;
		}

		// ignore if not touching a PV leaf
		// check area
		if ( !CM_AreasConnected( clientarea, svEnt->areanum ) ) {
			// doors can legally straddle two areas, so
			// we may need to check another one
			if ( !CM_AreasConnected( clientarea, svEnt->areanum2 ) ) {
				continue;		// blocked by a door
			}
		}

		bitvector = clientpvs;

		// check individual leafs
		if ( !svEnt->numClusters ) {
			continue;
		}
		l = 0;
		for ( i=0 ; i < svEnt->numClusters ; i++ ) {
			l = svEnt->clusternums[i];
			if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
				break;
			}
		}

		// if we haven't found it to be visible,
		// check overflow clusters that couldn't be stored
		if ( i == svEnt->numClusters ) {
			if ( svEnt->lastCluster ) {
				for ( ; l <= svEnt->lastCluster ; l++ ) {
					if ( bitvector[l >> 3] & (1 << (l&7) ) ) {
						break;
					}
				}
				if ( l == svEnt->lastCluster ) {
					continue;	// not visible
				}
			} else {
				continue;
			}
		}

		// add it
		SV_AddIndexToSnapshot( svEnt, e, eNums );

		// if it's a portal entity, add everything visible from its camera position
		if ( ent->r.svFlags & SVF_PORTAL && !portal ) {
			if ( ent->s.generic1 ) {
				vec3_t dir;
				VectorSubtract(ent->s.origin, origin, dir);
				if ( VectorLengthSquared(dir) > (float) ent->s.generic1 * ent->s.generic1 ) {
					continue;
				}
			}
			eNums->unordered = qtrue;
			SV_AddEntitiesVisibleFromPoint( ent->s.origin2, frame, eNums, portal );
		}
	}

	ent = SV_GentityNum( frame->ps.clientNum );
	// extension: merge second PVS at ent->r.s.origin2
	if ( ent->r.svFlags & SVF_SELF_PORTAL2 && !portal ) {
		SV_AddEntitiesVisibleFromPoint( ent->r.s.origin2, frame, eNums, qtrue );
		eNums->unordered = qtrue;
	}
}


/*
===============
SV_InitSnapshotStorage
===============
*/
void SV_InitSnapshotStorage( void ) 
{
	// initialize snapshot storage
	Com_Memset( svs.snapFrames, 0, sizeof( svs.snapFrames ) );
	svs.freeStorageEntities = svs.numSnapshotEntities;
	svs.currentStoragePosition = 0;

	svs.snapshotFrame = 0;
	svs.currentSnapshotFrame = 0;
	svs.lastValidFrame = 0;

	svs.currFrame = NULL;
}


/*
===============
SV_IssueNewSnapshot

This should be called before any new client snaphot built
===============
*/
void SV_IssueNewSnapshot( void ) 
{
	svs.currFrame = NULL;
	
	// value that clients can use even for their empty frames
	// as it will not increment on new snapshot built
	svs.currentSnapshotFrame = svs.snapshotFrame;
}


/*
===============
SV_BuildCommonSnapshot

This always allocates new common snapshot frame
===============
*/
static void SV_BuildCommonSnapshot( void ) 
{
	sharedEntity_t	*list[ MAX_GENTITIES ];
	sharedEntity_t	*ent;
	
	snapshotFrame_t	*tmp;
	snapshotFrame_t	*sf;

	int count;
	int index;
	int	num;
	int i;

	count = 0;

	// gather all linked entities
	if ( sv.state != SS_DEAD ) {
		for ( num = 0 ; num < sv.num_entities ; num++ ) {
			ent = SV_GentityNum( num );

			// never send entities that aren't linked in
			if ( !ent->r.linked ) {
				continue;
			}
	
			if ( ent->s.number != num ) {
				Com_DPrintf( "FIXING ENT->S.NUMBER %i => %i\n", ent->s.number, num );
				ent->s.number = num;
			}

			// entities can be flagged to explicitly not be sent to the client
			if ( ent->r.svFlags & SVF_NOCLIENT ) {
				continue;
			}

			list[ count++ ] = ent;
			sv.svEntities[ num ].snapshotCounter = -1;
		}
	}

	sv.snapshotCounter = -1;

	sf = &svs.snapFrames[ svs.snapshotFrame % NUM_SNAPSHOT_FRAMES ];
	
	// track last valid frame
	if ( svs.snapshotFrame - svs.lastValidFrame > (NUM_SNAPSHOT_FRAMES-1) ) {
		svs.lastValidFrame = svs.snapshotFrame - (NUM_SNAPSHOT_FRAMES-1);
		// release storage
		svs.freeStorageEntities += sf->count;
		sf->count = 0;
	}

	// release more frames if needed
	while ( svs.freeStorageEntities < count && svs.lastValidFrame != svs.snapshotFrame ) {
		tmp = &svs.snapFrames[ svs.lastValidFrame % NUM_SNAPSHOT_FRAMES ];
		svs.lastValidFrame++;
		// release storage
		svs.freeStorageEntities += tmp->count;
		tmp->count = 0;
	}

	// should never happen but anyway
	if ( svs.freeStorageEntities < count ) {
		Com_Error( ERR_DROP, "Not enough snapshot storage: %i < %i", svs.freeStorageEntities, count );
	}

	// allocate storage
	sf->count = count;
	svs.freeStorageEntities -= count;

	sf->start = svs.currentStoragePosition; 
	svs.currentStoragePosition = ( svs.currentStoragePosition + count ) % svs.numSnapshotEntities;

	sf->frameNum = svs.snapshotFrame;
	svs.snapshotFrame++;

	svs.currFrame = sf; // clients can refer to this

	// setup start index
	index = sf->start;
	{
		int bufMs = 0;
		if ( sv_bufferMs && sv_bufferMs->integer != 0 ) {
			bufMs = sv_bufferMs->integer;
			if ( bufMs < 0 ) {
				// Auto mode: one snapshot interval
				bufMs = 1000 / sv_fps->integer;
			}
			if ( bufMs > 100 ) bufMs = 100;
		}

		for ( i = 0 ; i < count ; i++, index = (index+1) % svs.numSnapshotEntities ) {
			svs.snapshotEntities[ index ] = list[ i ]->s;

			// Fix up client entity positions between game frames
			if ( ( sv_smoothClients && sv_smoothClients->integer ) ||
				( sv_extrapolate && sv_extrapolate->integer ) ) {
				entityState_t *es = &svs.snapshotEntities[ index ];
				if ( es->number < sv.maxclients && es->pos.trType == TR_INTERPOLATE ) {

					qboolean usedBuffer = qfalse;
					qboolean isBot = ( svs.clients[ es->number ].netchan.remoteAddress.type == NA_BOT );
					vec3_t origin, velocity;

					// Phase 1: resolve position source
					if ( bufMs > 0 && !isBot ) {
						vec3_t delayedOrigin, delayedVelocity;
						if ( SV_SmoothGetPosition( es->number, sv.time - bufMs, delayedOrigin, delayedVelocity ) ) {
							VectorCopy( delayedOrigin, origin );
							VectorCopy( delayedVelocity, velocity );
							usedBuffer = qtrue;
						}
					}
					if ( !usedBuffer ) {
						if ( isBot ) {
							// sv_gamehz > 0: extrapolate bot position forward from the last
							// game frame using ps->velocity (not trDelta -- ps->velocity is
							// the Pmove velocity at the game-frame boundary and is more
							// accurate for straight-line bot movement between AI decisions).
							// Gate: sv.time > sv.gameTime is only true when sv_gamehz creates
							// a real gap.  At sv_gamehz 0 these are always equal so the else
							// branch fires, preserving the existing trBase/trDelta path.
							if ( sv.time > sv.gameTime ) {
								playerState_t *ps = SV_GameClientNum( es->number );
								float dt = (float)( sv.time - sv.gameTime ) * 0.001f;
								origin[0] = es->pos.trBase[0] + ps->velocity[0] * dt;
								origin[1] = es->pos.trBase[1] + ps->velocity[1] * dt;
								origin[2] = es->pos.trBase[2] + ps->velocity[2] * dt;
								VectorCopy( ps->velocity, velocity );
							} else {
								VectorCopy( es->pos.trBase, origin );
								VectorCopy( es->pos.trDelta, velocity );
							}
						} else {
							playerState_t *ps = SV_GameClientNum( es->number );
							// sv_gamehz > 0: ps->origin is only updated when a usercmd is
							// processed (GAME_CLIENT_THINK) or at the game-frame boundary
							// (G_RunClient inside GAME_RUN_FRAME).  Between game frames, if
							// the observed player's packets haven't arrived this tick,
							// ps->origin lags sv.time by up to one packet interval.
							// This causes sv_smoothClients to stamp identical trBase into
							// 2-3 consecutive snapshots -- the observer sees freeze-then-jump.
							//
							// Fix: extrapolate ps->origin forward from the last Pmove time
							// (ps->commandTime) to sv.time using ps->velocity.  The maximum
							// dt is capped to the game-frame gap (sv.time - sv.gameTime) so
							// we never extrapolate beyond what the game frame itself would.
							//
							// Gate: sv.time > sv.gameTime is only true when sv_gamehz creates
							// a gap.  At sv_gamehz 0, G_RunClient keeps ps->commandTime ==
							// sv.time every tick, so dt == 0 and this is a no-op.
							if ( sv.time > sv.gameTime && ps->commandTime < sv.time ) {
								float dt = (float)( sv.time - ps->commandTime ) * 0.001f;
								float maxDt = (float)( sv.time - sv.gameTime ) * 0.001f;
								if ( dt > maxDt ) dt = maxDt;
								origin[0] = ps->origin[0] + ps->velocity[0] * dt;
								origin[1] = ps->origin[1] + ps->velocity[1] * dt;
								origin[2] = ps->origin[2] + ps->velocity[2] * dt;
							} else {
								VectorCopy( ps->origin, origin );
							}
							VectorCopy( ps->velocity, velocity );
						}
					}

					// Phase 2: resolve trajectory type
					if ( isBot ) {
						// Only update the snapshot trBase when sv_gamehz creates a gap
						// between game frames.  Keeps TR_INTERPOLATE so the client
						// interpolates between extrapolated positions -- avoids the
						// visual/server mismatch that TR_LINEAR would cause when bots
						// change direction at a game-frame boundary.
						// At sv_gamehz 0: sv.time == sv.gameTime so the gate is false
						// and trBase is already fresh every tick -- no change needed.
						if ( sv.time > sv.gameTime ) {
							VectorCopy( origin, es->pos.trBase );
						}
					} else if ( sv_smoothClients && sv_smoothClients->integer ) {
						// TR_LINEAR mode
						int velSmoothMs = ( sv_velSmooth && sv_velSmooth->integer > 0 ) ? sv_velSmooth->integer : 0;
						vec3_t finalVel;
						if ( velSmoothMs > 0 ) {
							vec3_t avgVel;
							if ( SV_SmoothGetAverageVelocity( es->number, velSmoothMs, avgVel ) ) {
								VectorCopy( avgVel, finalVel );
							} else {
								VectorCopy( velocity, finalVel );
							}
						} else {
							VectorCopy( velocity, finalVel );
						}
						if ( DotProduct( finalVel, finalVel ) > 100.0f ) {
							VectorCopy( origin, es->pos.trBase );
							VectorCopy( finalVel, es->pos.trDelta );
							es->pos.trType = TR_LINEAR;
							es->pos.trTime = sv.time;
						} else {
							// Idle: keep TR_INTERPOLATE but anchor position
							VectorCopy( origin, es->pos.trBase );
							VectorCopy( finalVel, es->pos.trDelta );
						}
					} else {
						// TR_INTERPOLATE mode (real players, sv_extrapolate path)
						if ( usedBuffer ) {
							if ( DotProduct( velocity, velocity ) > 100.0f ) {
								VectorCopy( origin, es->pos.trBase );
								VectorCopy( velocity, es->pos.trDelta );
							}
						} else {
							VectorCopy( origin, es->pos.trBase );
							VectorCopy( velocity, es->pos.trDelta );
						}
					}

					/* Anchor trTime to the snapshot build time for all entities that
					   remain TR_INTERPOLATE after Phase 2.
					   BG_PlayerStateToEntityState (game QVM) never writes pos.trTime for
					   TR_INTERPOLATE entities -- only TR_LINEAR/TR_GRAVITY etc. set it --
					   so it stays at 0 or a stale game time.  The TR_LINEAR formula used
					   by Patch 1 computes:
					     result = trBase + trDelta * (evalTime - trTime) / 1000
					   With trTime == 0 that dt is enormous, teleporting entities every
					   frame regardless of network conditions.  With trTime anchored here:
					     - cgame evaluates snap at snap.serverTime  -> dt = 0 -> trBase
					       (normal interpolation preserved, identical to before)
					     - cgame evaluates snap at cg.time during extrapolation
					       -> dt = cg.time - serverTime -> correct forward extrapolation */
					if ( es->pos.trType == TR_INTERPOLATE ) {
						es->pos.trTime = usedBuffer ? sv.time - bufMs : sv.time;
					}
				}
			}

			sf->ents[ i ] = &svs.snapshotEntities[ index ];
		}
	}
}


/*
=============
SV_BuildClientSnapshot

Decides which entities are going to be visible to the client, and
copies off the playerstate and areabits.

This properly handles multiple recursive portals, but the render
currently doesn't.

For viewing through other player's eyes, clent can be something other than client->gentity
=============
*/
static void SV_BuildClientSnapshot( client_t *client ) {
	vec3_t						org;
	clientSnapshot_t			*frame;
	snapshotEntityNumbers_t		entityNumbers;
	int							i, cl;
	svEntity_t					*svEnt;
	int							clientNum;
	playerState_t				*ps;

	// this is the frame we are creating
	frame = &client->frames[ client->netchan.outgoingSequence & PACKET_MASK ];
	cl = client - svs.clients;

	// clear everything in this snapshot
	Com_Memset( frame->areabits, 0, sizeof( frame->areabits ) );
	frame->areabytes = 0;

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=62
	frame->num_entities = 0;
	frame->frameNum = svs.currentSnapshotFrame;
	
	if ( client->state == CS_ZOMBIE )
		return;

	// grab the current playerState_t
	ps = SV_GameClientNum( cl );
	frame->ps = *ps;

	clientNum = frame->ps.clientNum;
	if ( clientNum < 0 || clientNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "SV_SvEntityForGentity: bad gEnt" );
	}

	// we set client->gentity only after sending gamestate
	// so don't send any packetentities changes until CS_PRIMED
	// because new gamestate will invalidate them anyway
	if ( !client->gentity ) {
		return;
	}

	if ( svs.currFrame == NULL ) {
		// this will always success and setup current frame
		SV_BuildCommonSnapshot();
	}

	// bump the counter used to prevent double adding
	sv.snapshotCounter++;

	// empty entities before visibility check
	entityNumbers.numSnapshotEntities = 0;

	frame->frameNum = svs.currFrame->frameNum;

	// never send client's own entity, because it can
	// be regenerated from the playerstate
	svEnt = &sv.svEntities[ clientNum ];
	svEnt->snapshotCounter = sv.snapshotCounter;

	// find the client's viewpoint
	VectorCopy( ps->origin, org );
	org[2] += ps->viewheight;

	// add all the entities directly visible to the eye, which
	// may include portal entities that merge other viewpoints
	entityNumbers.unordered = qfalse;
	SV_AddEntitiesVisibleFromPoint( org, frame, &entityNumbers, qfalse );

	// if there were portals visible, there may be out of order entities
	// in the list which will need to be resorted for the delta compression
	// to work correctly.  This also catches the error condition
	// of an entity being included twice.
	if ( entityNumbers.unordered ) {
		SV_SortEntityNumbers( &entityNumbers.snapshotEntities[0], 
			entityNumbers.numSnapshotEntities );
	}

	// now that all viewpoint's areabits have been OR'd together, invert
	// all of them to make it a mask vector, which is what the renderer wants
	for ( i = 0; i < MAX_MAP_AREA_BYTES/sizeof(int); i++ ) {
		((int *)frame->areabits)[i] = ((int *)frame->areabits)[i] ^ -1;
	}

	frame->num_entities = entityNumbers.numSnapshotEntities;
	// get pointers from common snapshot
	for ( i = 0 ; i < entityNumbers.numSnapshotEntities ; i++ )	{
		frame->ents[ i ] = svs.currFrame->ents[ entityNumbers.snapshotEntities[ i ] ];
	}
}


/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
void SV_SendMessageToClient( msg_t *msg, client_t *client )
{
#ifdef USE_SERVER_DEMO
	if ( client->demo_recording && !client->demo_waiting ) {
		SVD_WriteDemoFile( client, msg );
	}
#endif

	// record information about the message
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = svs.msgTime;
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = 0;

	// send the datagram
	SV_Netchan_Transmit( client, msg );
}


/*
=======================
SV_SendClientSnapshot

Also called by SV_FinalMessage

=======================
*/
void SV_SendClientSnapshot( client_t *client ) {
	byte		msg_buf[ MAX_MSGLEN_BUF ];
	msg_t		msg;

	// build the snapshot
	SV_BuildClientSnapshot( client );

	// bots need to have their snapshots build, but
	// the query them directly without needing to be sent
	if ( client->netchan.remoteAddress.type == NA_BOT ) {
		return;
	}

	MSG_Init( &msg, msg_buf, MAX_MSGLEN );
	msg.allowoverflow = qtrue;

	// NOTE, MRE: all server->client messages now acknowledge
	// let the client know which reliable clientCommands we have received
	MSG_WriteLong( &msg, client->lastClientCommand );

	// (re)send any reliable server commands
	SV_UpdateServerCommandsToClient( client, &msg );

	// send over all the relevant entityState_t
	// and the playerState_t
	SV_WriteSnapshotToClient( client, &msg );

	// check for overflow
	if ( msg.overflowed ) {
		Com_Printf( "WARNING: msg overflowed for %s\n", client->name );
		MSG_Clear( &msg );
	}

	SV_SendMessageToClient( &msg, client );
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages( void )
{
	int		i;
	client_t	*c;

	svs.msgTime = Sys_Milliseconds();

	// send a message to each connected client
	for ( i = 0; i < sv.maxclients; i++ )
	{
		c = &svs.clients[ i ];

		if ( c->state == CS_FREE )
			continue;		// not connected

		//if ( *c->downloadName )
		//	continue;		// Client is downloading, don't send snapshots

		if ( c->state == CS_CONNECTED )
			continue;		// Client is downloading, don't send snapshots

		//if ( !c->gamestateAcked )
		//	continue;		// waiting usercmd/downloading

		// 1. Local clients get snapshots every server frame
		// 2. Remote clients get snapshots depending from rate and requested number of updates

		if ( svs.time - c->lastSnapshotTime < c->snapshotMsec * com_timescale->value )
			continue;		// It's not time yet

		if ( c->netchan.unsentFragments || c->netchan_start_queue )
		{
			c->rateDelayed = qtrue;
			continue;		// Drop this snapshot if the packet queue is still full or delta compression will break
		}

		if ( SV_RateMsec( c ) > 0 )
		{
			// Not enough time since last packet passed through the line
			c->rateDelayed = qtrue;
			continue;
		}

		// generate and send a new message
		SV_SendClientSnapshot( c );
		c->lastSnapshotTime = svs.time;
		c->rateDelayed = qfalse;
	}
}
