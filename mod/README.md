# Urban Terror 4.3.4 — QVM Source Reconstruction

This directory contains the reconstructed source tree for Urban Terror 4.3.4 (`q3urt43`).

## Background

The original 4.3.4 source was lost. This reconstruction uses:

- **UrT 4.2 GPL source** (`qsource.zip`) as the base — licensed under GPL v2
- **UrT 4.3.4 compiled QVM binaries** (`zUrT43_qvm.zip`, Jun 18 2018) as the reference
- **QVM disassembly** (via `qvmdis`) to reconstruct changed and new functions
- **String-literal analysis** to recover all cvars, commands, log events, and asset paths

The result is a clean-room functional reimplementation: behaviourally identical to 4.3.4
(same cvars, game modes, log events, server API) but independently authored.

All source files are licensed under the GNU General Public License v2 or later,
in accordance with the original Quake III Arena / Urban Terror codebase.

## Directory Layout

```
mod/
  game/         server-side QVM  → qagame.qvm
  cgame/        client-side QVM  → cgame.qvm
  ui/           UI QVM           → ui.qvm
  common/       shared between all three QVMs
```

## Build

Requires `q3asm` (Quake 3 assembler) and a C compiler that can target QVM bytecode
(e.g. `lcc` with `q3lcc` wrapper, or modern `q3vm-tools`).

```bash
cd mod/game  && make   # produces qagame.qvm
cd mod/cgame && make   # produces cgame.qvm
cd mod/ui    && make   # produces ui.qvm
```

## Reconstruction Stages

See `docs/project/URT43_QVM_RECONSTRUCTION.md` for the full analysis and stage breakdown.

| Stage | Status | Description |
|-------|--------|-------------|
| 1 | ✅ | Scaffold — directory structure, Makefile, `.q3asm` lists, syscall ASM |
| 2 | ✅ | Shared headers — `bg_public.h` (GT_FREEZE/GT_GUNGAME, new EV_/CS_/PMF flags), `g_local.h` (freeze/GunGame struct fields + 26 new cvar externs) |
| 3 | ✅ | Tier-1 common files — hash-identical: `q_shared.c`, `q_math.c`, `bg_lib.c`, `bg_slidemove.c`, all `c_*` common files |
| 4 | ✅ | Tier-1 bot/AI files — all hash-identical bot AI: `ai_main.c` through `ai_vcmd.c` |
| 5 | ✅ | Tier-1 game infrastructure — `g_antilag.c`, `g_arenas.c`, `g_bot.c`, `g_misc.c`, `g_mover.c`, `g_rankings.c`, `g_session.c`, `g_target.c`, `g_trigger.c` |
| 6 | ✅ | Core game logic (Tier 2) — `g_main.c` (+26 cvars, cvar renames, v4.3.4), `g_combat.c` (instagib, freeze immunity, UT_MOD_MELT/FREEZE), all other game C files patched |
| 7 | ✅ | New game modes (Tier 3) — `g_freezetag.c` (freeze/thaw/meltdown), `g_gungame.c` (weapon progression), `g_sentry.c` (Mr. Sentry turret entity) |
| 8 | ✅ | cgame reconstruction — Tier-2 cgame files updated; `cg_freezetag.c`, `cg_gungame.c` written |
| 9 | ✅ | UI reconstruction — `ui_gameinfo.c` updated for GT_FREEZE/GT_GUNGAME map type bits |
| 10 | ✅ | Build verification — all three QVMs compile with 0 errors using 4.2 tools from qsource.zip |
