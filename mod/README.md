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
| 2 | ✅ | Shared headers — `bg_public.h`, `q_shared.h`, `g_local.h`, `cg_local.h` |
| 3 | ✅ | Tier-1 common files — hash-identical utility/math/lib files |
| 4 | ✅ | Tier-1 bot/AI files — all hash-identical bot AI |
| 5 | ✅ | Tier-1 game infrastructure — mostly-unchanged game files |
| 6 | 🔄 | Core game logic (Tier 2) — modified functions |
| 7 | ⬜ | New game modes — Freeze Tag, Gun Game, Mr. Sentry |
| 8 | ⬜ | cgame reconstruction |
| 9 | ⬜ | UI reconstruction |
| 10 | ⬜ | Build verification |
