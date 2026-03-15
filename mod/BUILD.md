# Build Guide — Urban Terror 4.3.4 QVM

This document covers **everything** needed to compile the three QVM files
(`qagame.qvm`, `cgame.qvm`, `ui.qvm`) from source on Linux or Windows.

---

## Quick start

```bash
cd mod
make          # builds all three QVMs into mod/build/
```

No extra setup needed — the required tools are already in `tools/` and `tools2/`.

---

## Tools

### `mod/tools/` — compiler (q3lcc)

Contains **ec-/q3lcc** (https://github.com/ec-/q3lcc), an LCC-based C→QVM compiler.
Also includes the pre-built Linux 32-bit binaries from the UrT 4.2 GPL source zip for
reference (`q3lcc`, `q3asm`, `q3cpp`, `q3rcc`).

The **active** compiler is the ec- build at `tools/build--/q3lcc` (64-bit, built from
source). The 4.2 32-bit binaries are kept for reference and work on Linux if `lib32-*`
packages are installed.

| File | Description |
|------|-------------|
| `tools/build--/q3lcc` | **Active compiler** (ec- build, 64-bit) |
| `tools/q3lcc` | 4.2 pre-built Linux 32-bit binary (reference) |
| `tools/q3lcc.exe` | 4.2 pre-built Windows binary |
| `tools/q3cpp` / `tools/q3rcc` | preprocessor / code-generator helpers |

### `mod/tools2/` — Windows compiler suite

Contains the Windows `.exe` tools from the UrT 4.2 GPL source, used by the
original `make.bat` on Windows: `lcc.exe`, `q3asm.exe`, `q3cpp.exe`, `q3rcc.exe`.

| File | Description |
|------|-------------|
| `tools2/lcc.exe` | Windows LCC QVM compiler |
| `tools2/q3asm.exe` | Windows QVM assembler |

### q3asm — assembler

The assembler binary used by default is `tools/q3asm` (the 4.2 pre-built Linux binary).
The `$(Q3ASM)` Makefile variable points to it.

The ec-/q3asm source (https://github.com/ec-/q3asm) is bundled in `tools2/` for
easy rebuild if the 4.2 binary doesn't run (`make -C tools2`).

---

## Makefile variables

| Variable | Default | Description |
|----------|---------|-------------|
| `Q3LCC` | `tools/build--/q3lcc` | C→QVM compiler |
| `Q3ASM` | `tools/q3asm` | QVM assembler |

Override example:

```bash
make Q3LCC=/usr/local/bin/q3lcc Q3ASM=/usr/local/bin/q3asm
```

---

## Build targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build all three QVMs |
| `make game` | Build `build/qagame.qvm` only |
| `make cgame` | Build `build/cgame.qvm` only |
| `make ui` | Build `build/ui.qvm` only |
| `make tools` | (Re)build ec-/q3lcc and ec-/q3asm from source |
| `make clean` | Remove `build/` |
| `make clean-tools` | Remove compiled tool binaries |

---

## Source layout

```
mod/
  Makefile          top-level build
  tools/            q3lcc compiler (ec- source + 4.2 pre-built binaries)
  tools2/           q3asm and Windows .exe tools (4.2 pre-built + ec- q3asm source)
  build/            generated: qagame.qvm, cgame.qvm, ui.qvm  (not committed)
  game/             server QVM sources
    g_syscalls.asm  trap table for qagame  ← from 4.2 source, patched for 4.3.4
    g_syscalls.c    C wrappers for syscalls
    bg_public.h     shared public header (GT_, EV_, CS_, MOD_ enums)
    bg_local.h      shared internal header
    inv.h           bot inventory slot constants
    ...
  cgame/            client QVM sources
    cg_syscalls.asm trap table for cgame   ← from 4.3.4 decompiled
    cg_syscalls.c   C wrappers for syscalls
    ...
  ui/               UI QVM sources
    ui_syscalls.asm trap table for ui      ← from 4.3.4 decompiled
    ui_syscalls.c   C wrappers for syscalls
    ui_shared.c/h   shared menu widget library (from 4.2 source)
    ...
  common/           shared between all three QVMs
    c_weapons.c/h   weapon definitions and damage tables
    c_players.c     player profile / .characterfile parser
    c_syscalls.h    shared syscall declarations
    ...
```

---

## Syscall files — 4.2 vs 4.3.4 differences

The syscall `.asm` files define the QVM trap numbers (negative integers) for every
engine API call. We started from the 4.2 GPL source and patched against the 4.3.4
decompiled trap tables (`zUrT43_qvm.zip/asm/*.asm`).

### `game/g_syscalls.asm`

| Change | Detail |
|--------|--------|
| Renamed | `trap_PC_LoadSource/FreeSource/ReadToken/SourceFileAndLine` → `trap_BotLib*` (4.3.4 naming). Aliases kept for shared code. |
| Removed | `trap_NET_StringToAdr` (-601), `trap_NET_SendPacket` (-602), `trap_Sys_StartProcess` (-603), `trap_Auth_DropClient` (-604) — gone in 4.3 |

### `cgame/cg_syscalls.asm`

| Change | Detail |
|--------|--------|
| Removed | `trap_GetDemoState` (-91) — gone in 4.3 |

### `ui/ui_syscalls.asm`

| Change | Detail |
|--------|--------|
| Renamed | `trap_Set_PBClStatus` (-88) → `trap_SetPbClStatus` (capitalisation fixed in 4.3) |
| Removed | `trap_NET_StringToAdr` (-89), `trap_Q_vsnprintf` (-90), `trap_NET_SendPacket` (-91), `trap_Sys_StartProcess` (-93), `trap_NET_CompareBaseAdr` (-94), `printf` (-110) |

---

## New weapons (4.3.x additions)

Weapons that were stubbed out with `#if 0` in 4.2 and are active in 4.3.4:

| ID | Name | Notes |
|----|------|-------|
| 23 | P90 | SMG, enabled in 4.3 |
| 24 | Benelli M3 | Shotgun, enabled in 4.3 |
| 25 | Magnum | Revolver, enabled in 4.3 |
| 26 | FRF1 | Sniper rifle. Damage: vest 76%, arms 50%, upper-leg 50%, lower-leg 40%, butt/groin 76%/31% |

`TOD50` was present in older pre-release code but is **not** in 4.3.4 retail — the slot
(27) is reserved but zero-initialised.

---

## Damage model

UrT uses per-hitzone multipliers. The multipliers are stored in `c_weapons.c` as
`bg_weaponlist[WT_*].damage` tables indexed by `HL_*` hitlocation enum values
(defined in `bg_public.h`).

FRF1 values confirmed via in-game `cg_showbullethits 2` testing:

| Hitzone | Multiplier |
|---------|-----------|
| Vest (torso with vest) | 76% |
| Arms | 50% |
| Upper leg | 50% |
| Lower leg | 40% |
| Butt | 76% |
| Groin | 31% |
| Head / helmet | ~100% / ~130% (estimated, tune with `cg_showbullethits 2`) |

---

## New game modes

### GT_FREEZE (Freeze Tag)

Implemented in `game/g_freezetag.c` and `cgame/cg_freezetag.c`.

- Players are frozen (not killed) when health reaches 0
- Teammates thaw frozen players by standing near them
- `g_freezeThawTime` cvar controls thaw duration (default 3000 ms)
- `g_freezeMeltTime` controls auto-melt time
- Frozen players display a blue overlay HUD (`cg_freezetag.c`)

### GT_GUNGAME (Gun Game)

Implemented in `game/g_gungame.c` and `cgame/cg_gungame.c`.

- Each kill advances the player to the next weapon in the progression
- First player to reach the final weapon (knife) wins
- `g_gunGameWeapons` cvar configures the weapon progression list

---

## Known issues / TODO

- FRF1 head/helmet damage values are estimated — confirm with `cg_showbullethits 2`
- Animation delta for new weapons (P90/Benelli/Magnum) may need tuning against
  4.3.4 model files
- `g_rankings.c` is a minimal no-op stub; the full UrT stats/rankings system was
  proprietary and cannot be reconstructed from the QVM alone
- `trap_Auth_DropClient` and `trap_NET_*` wrappers in `g_syscalls.c` are still
  compiled but the `.asm` trap definitions are removed (they will link as dead code)
