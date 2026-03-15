# Urban Terror 4.3.4 QVM Build System
#
# tools/  = q3lcc + q3asm pre-built Linux binaries from UrbanTerror 4.2 source (primary)
#           Also contains ec-/q3lcc source tree as a fallback build system.
# tools2/ = Windows .exe equivalents (for reference / Windows builds)
#
# Override with:   make Q3LCC=/path/to/q3lcc Q3ASM=/path/to/q3asm
#
# Usage:
#   make              - build all three QVMs
#   make game         - build qagame.qvm only
#   make cgame        - build cgame.qvm only
#   make ui           - build ui.qvm only
#   make tools        - (re)build ec-/q3lcc + ec-/q3asm from source in tools/
#   make clean        - remove QVM build artifacts
#   make clean-tools  - remove compiled ec- tool binaries

TOOLSDIR  = $(shell pwd)/tools
TOOLS2DIR = $(shell pwd)/tools2

# Primary compiler: ec-/q3lcc (64-bit, built from source in tools/)
# The 4.2 pre-built q3lcc/q3asm in tools/ are 32-bit Linux binaries kept for reference.
# Primary assembler: 4.2 q3asm (32-bit but works fine here)
Q3LCC   ?= $(TOOLSDIR)/build--/q3lcc
Q3ASM   ?= $(TOOLSDIR)/build--/q3asm

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
	$(GAMEDIR)/bg_pmove.c \
	$(GAMEDIR)/bg_lib.c \
	$(GAMEDIR)/bg_misc.c \
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
	$(GAMEDIR)/bg_misc.c \
	$(GAMEDIR)/bg_lib.c \
	$(GAMEDIR)/bg_gear.c \
	$(GAMEDIR)/q_math.c \
	$(GAMEDIR)/q_shared.c \
	$(COMMONDIR)/zdecrypt.c \
	$(COMMONDIR)/zzz_last.c

GAME_OBJS   = $(GAMEDIR)/g_syscalls.asm \
              $(patsubst %.c,$(BUILDDIR)/game/%.asm,$(notdir $(GAME_SOURCES)))
CGAME_OBJS  = $(CGAMEDIR)/cg_syscalls.asm \
              $(patsubst %.c,$(BUILDDIR)/cgame/%.asm,$(notdir $(CGAME_SOURCES)))
UI_OBJS     = $(UIDIR)/ui_syscalls.asm \
              $(patsubst %.c,$(BUILDDIR)/ui/%.asm,$(notdir $(UI_SOURCES)))

.PHONY: all game cgame ui tools clean clean-tools

# tools/ has pre-built 4.2 binaries; 'make tools' (re)builds the ec- versions from source
tools:
	$(MAKE) -C $(TOOLSDIR)
	$(MAKE) -C $(TOOLS2DIR)

clean-tools:
	$(MAKE) -C $(TOOLSDIR) clean
	$(MAKE) -C $(TOOLS2DIR) clean

all: game cgame ui

game: $(BUILDDIR)/qagame.qvm

cgame: $(BUILDDIR)/cgame.qvm

ui: $(BUILDDIR)/ui.qvm

# 4.2 q3asm link steps: pass all .asm files directly
$(BUILDDIR)/qagame.qvm: $(GAME_OBJS)
	$(Q3ASM) -o $(BUILDDIR)/qagame $(GAME_OBJS)

$(BUILDDIR)/cgame.qvm: $(CGAME_OBJS)
	$(Q3ASM) -o $(BUILDDIR)/cgame $(CGAME_OBJS)

$(BUILDDIR)/ui.qvm: $(UI_OBJS)
	$(Q3ASM) -o $(BUILDDIR)/ui $(UI_OBJS)

$(BUILDDIR)/game/%.asm: $(GAMEDIR)/%.c | $(BUILDDIR)/game
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/game/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/game
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(CGAMEDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(UIDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/cgame/%.asm: $(GAMEDIR)/%.c | $(BUILDDIR)/cgame
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/ui/%.asm: $(UIDIR)/%.c | $(BUILDDIR)/ui
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/ui/%.asm: $(COMMONDIR)/%.c | $(BUILDDIR)/ui
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/ui/%.asm: $(GAMEDIR)/%.c | $(BUILDDIR)/ui
	$(Q3LCC) $(Q3LCCFLAGS) -S -o $@ $<

$(BUILDDIR)/game $(BUILDDIR)/cgame $(BUILDDIR)/ui:
	mkdir -p $@

clean:
	rm -rf $(BUILDDIR)
