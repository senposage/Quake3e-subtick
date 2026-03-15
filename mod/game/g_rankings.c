// g_rankings.c -- global rankings system stubs
// The Q3 global rankings system was never used in Urban Terror.
// All functions are no-ops to satisfy any potential linker references.

#include "g_local.h"
#include "g_rankings.h"

void G_RankRunFrame( void ) {}
void G_RankKillPlayer( gentity_t *self, gentity_t *attacker ) {
(void)self; (void)attacker;
}
void G_RankSessionBegin( int clientNum ) { (void)clientNum; }
void G_RankSessionEnd( int clientNum, grank_status_t status ) {
(void)clientNum; (void)status;
}
void G_RankDisconnect( gentity_t *self ) { (void)self; }
