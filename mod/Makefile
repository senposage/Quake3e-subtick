# Urban Terror 4.3.4 QVM Build System
#
# Requires:
#   Q3LCC   = path to q3lcc (lcc QVM C compiler)
#   Q3ASM   = path to q3asm (QVM assembler)
#
# Usage:
#   make              - build all three QVMs
#   make game         - build qagame.qvm only
#   make cgame        - build cgame.qvm only
#   make ui           - build ui.qvm only
#   make clean        - remove build artifacts

Q3LCC   ?= q3lcc
Q3ASM   ?= q3asm

BASEDIR  = $(shell pwd)
GAMEDIR  = $(BASEDIR)/game
CGAMEDIR = $(BASEDIR)/cgame
UIDIR    = $(BASEDIR)/ui
COMMONDIR= $(BASEDIR)/common
BUILDDIR = $(BASEDIR)/build

Q3LCCFLAGS = -DQ3_VM \
             -DMISSIONPACK \
             -I$(GAMEDIR) \
             -I$(CGAMEDIR) \
             -I$(UIDIR) \
             -I$(COMMONDIR)

GAME_SOURCES = \
	$(GAMEDIR)/g_main.c \
	$(COMMONDIR)/q_cvars.c \
	$(COMMONDIR)/c_cvars.c \
	$(COMMONDIR)/c_bmalloc.c \
	$(COMMONDIR)/c_bysize.c \
	$(COMMONDIR)/c_md3.c \
	$(COMMONDIR)/c_players.c \
	$(COMMONDIR)/c_arieshl.c \
	$(COMMONDIR)/c_aries.c \
	$(COMMONDIR)/c_weapons.c \
	$(GAMEDIR)/bg_misc.c \
	$(GAMEDIR)/bg_gear.c \
	$(GAMEDIR)/bg_lib.c \
	$(GAMEDIR)/bg_pmove.c \
	$(GAMEDIR)/bg_slidemove.c \
	$(GAMEDIR)/q_math.c \
	$(GAMEDIR)/q_shared.c \
	$(GAMEDIR)/ai_dmnet.c \
	$(GAMEDIR)/ai_dmq3.c \
	$(GAMEDIR)/ai_team.c \
	$(GAMEDIR)/ai_main.c \
	$(GAMEDIR)/ai_chat.c \
	$(GAMEDIR)/ai_cmd.c \
	$(GAMEDIR)/ai_vcmd.c \
	$(GAMEDIR)/g_antilag.c \
	$(GAMEDIR)/g_aries.c \
	$(GAMEDIR)/g_active.c \
	$(GAMEDIR)/g_arenas.c \
	$(GAMEDIR)/g_bot.c \
	$(GAMEDIR)/g_client.c \
	$(GAMEDIR)/g_cmds.c \
	$(GAMEDIR)/g_combat.c \
	$(GAMEDIR)/g_freezetag.c \
	$(GAMEDIR)/g_gungame.c \
	$(GAMEDIR)/g_items.c \
	$(GAMEDIR)/g_misc.c \
	$(GAMEDIR)/g_missile.c \
	$(GAMEDIR)/g_mover.c \
	$(GAMEDIR)/g_rankings.c \
	$(GAMEDIR)/g_session.c \
	$(GAMEDIR)/g_sentry.c \
	$(GAMEDIR)/g_spawn.c \
	$(GAMEDIR)/g_svcmds.c \
	$(GAMEDIR)/g_target.c \
	$(GAMEDIR)/g_team.c \
	$(GAMEDIR)/g_trigger.c \
	$(GAMEDIR)/g_utils.c \
	$(GAMEDIR)/g_weapon.c \
	$(COMMONDIR)/zdecrypt.c \
	$(COMMONDIR)/zzz_last.c

CGAME_SOURCES = \
	$(CGAMEDIR)/cg_main.c \
	$(COMMONDIR)/q_cvars.c \
	$(COMMONDIR)/c_cvars.c \
	$(COMMONDIR)/c_bmalloc.c \
	$(COMMONDIR)/c_bysize.c \
	$(COMMONDIR)/c_md3.c \
	$(COMMONDIR)/c_players.c \
	$(COMMONDIR)/c_arieshl.c \
	$(COMMONDIR)/c_aries.c \
	$(COMMONDIR)/c_weapons.c \
	$(CGAMEDIR)/cg_consolecmds.c \
	$(CGAMEDIR)/cg_draw.c \
	$(CGAMEDIR)/cg_newdraw.c \
	$(CGAMEDIR)/cg_drawaries.c \
	$(CGAMEDIR)/cg_drawtools.c \
	$(CGAMEDIR)/cg_effects.c \
	$(CGAMEDIR)/cg_ents.c \
	$(CGAMEDIR)/cg_event.c \
	$(CGAMEDIR)/cg_freezetag.c \
	$(CGAMEDIR)/cg_gungame.c \
	$(CGAMEDIR)/cg_info.c \
	$(CGAMEDIR)/cg_localents.c \
	$(CGAMEDIR)/cg_marks.c \
	$(CGAMEDIR)/cg_players.c \
	$(CGAMEDIR)/cg_playerstate.c \
	$(CGAMEDIR)/cg_particles.c \
	$(CGAMEDIR)/cg_predict.c \
	$(CGAMEDIR)/cg_scoreboard.c \
	$(CGAMEDIR)/cg_servercmds.c \
	$(CGAMEDIR)/cg_surfaces.c \
	$(CGAMEDIR)/cg_snapshot.c \
	$(CGAMEDIR)/cg_view.c \
	$(CGAMEDIR)/cg_weapons.c \
	$(UIDIR)/ui_shared.c \
	$(GAMEDIR)/bg_slidemove.c \
	$(CGAMEDIR)/bg_pmove.c \
	$(GAMEDIR)/bg_lib.c \
	$(CGAMEDIR)/bg_misc.c \
	$(GAMEDIR)/q_math.c \
	$(GAMEDIR)/q_shared.c \
	$(COMMONDIR)/zdecrypt.c \
	$(COMMONDIR)/zzz_last.c

UI_SOURCES = \
	$(UIDIR)/ui_main.c \
	$(COMMONDIR)/q_cvars.c \
	$(COMMONDIR)/c_cvars.c \
	$(COMMONDIR)/c_bmalloc.c \
	$(COMMONDIR)/c_bysize.c \
	$(COMMONDIR)/c_players.c \
	$(COMMONDIR)/c_weapons.c \
	$(UIDIR)/ui_atoms.c \
	$(UIDIR)/ui_players.c \
	$(UIDIR)/ui_shared.c \
	$(UIDIR)/ui_gameinfo.c \
	$(UIDIR)/bg_misc.c \
	$(GAMEDIR)/bg_lib.c \
	$(UIDIR)/bg_gear.c \
	$(GAMEDIR)/q_math.c \
	$(GAMEDIR)/q_shared.c \
	$(COMMONDIR)/zdecrypt.c \
	$(COMMONDIR)/zzz_last.c

GAME_OBJS   = $(patsubst %.c,$(BUILDDIR)/game/%.asm,$(notdir $(GAME_SOURCES)))
CGAME_OBJS  = $(patsubst %.c,$(BUILDDIR)/cgame/%.asm,$(notdir $(CGAME_SOURCES)))
UI_OBJS     = $(patsubst %.c,$(BUILDDIR)/ui/%.asm,$(notdir $(UI_SOURCES)))

.PHONY: all game cgame ui clean

all: game cgame ui

game: $(BUILDDIR)/qagame.qvm

cgame: $(BUILDDIR)/cgame.qvm

ui: $(BUILDDIR)/ui.qvm

$(BUILDDIR)/qagame.qvm: $(GAME_OBJS)
	$(Q3ASM) -o $@ $(GAMEDIR)/game.q3asm

$(BUILDDIR)/cgame.qvm: $(CGAME_OBJS)
	$(Q3ASM) -o $@ $(CGAMEDIR)/cgame.q3asm

$(BUILDDIR)/ui.qvm: $(UI_OBJS)
	$(Q3ASM) -o $@ $(UIDIR)/ui.q3asm

$(BUILDDIR)/game/%.asm: $(GAMEDIR)/%.c | $(BUILDDIR)/game
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/game/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/game
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(CGAMEDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/ui/%.asm: $(UIDIR)/%.c | $(BUILDDIR)/ui
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/ui/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/ui
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/game $(BUILDDIR)/cgame $(BUILDDIR)/ui:
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)
