# QVM — Quake Virtual Machine

> Covers `code/qcommon/vm.c`, `vm_local.h`, `vm_interpreted.c` (bytecode interpreter), `vm_x86.c` (x86-64 JIT), `vm_aarch64.c` (ARM64 JIT), `vm_armv7l.c` (ARMv7 JIT), and the QVM file format.

---

## Table of Contents

1. [Purpose and Design](#purpose-and-design)
2. [QVM Modules](#qvm-modules)
3. [QVM File Format](#qvm-file-format)
4. [Execution Modes](#execution-modes)
5. [vm.c — Core VM Functions](#vmc--core-vm-functions)
6. [Bytecode Interpreter](#bytecode-interpreter)
7. [JIT Compilers](#jit-compilers)
8. [Syscall Interface](#syscall-interface)
9. [JTS — Jump Target Sidecar Files](#jts--jump-target-sidecar-files)
10. [VM_ReplaceInstructions — Binary Patching](#vm_replaceinstructions--binary-patching)
11. [Runtime Checks](#runtime-checks)
12. [Key Data Structures](#key-data-structures)
13. [Opcode Reference](#opcode-reference)

---

## Purpose and Design

The QVM (Quake Virtual Machine) is a sandboxed bytecode VM that runs untrusted game/UI code. It:
- Provides a portable, platform-independent bytecode format
- Enforces memory bounds (can't access engine memory outside `dataBase`)
- Allows hot-reload (reload game without engine restart)
- Supports JIT compilation for near-native speed on supported platforms

The VM is stack-based with a separate operand stack. All arithmetic and logic uses 32-bit integers or 32-bit IEEE floats.

---

## QVM Modules

There are three VM slots in the engine:

| Index | `vmIndex_t` | Name | File | Loaded By | Syscall Handler |
|---|---|---|---|---|---|
| 0 | `VM_GAME` | `qagame` | `qagame.qvm` | `SV_InitGameProgs()` | `SV_GameSystemCalls()` |
| 1 | `VM_CGAME` | `cgame` | `cgame.qvm` | `CL_InitCGame()` | `CL_CgameSystemCalls()` |
| 2 | `VM_UI` | `ui` | `ui.qvm` | `CL_InitUI()` | `CL_UISystemCalls()` |

Each VM runs in its own sandboxed memory space. They can call back into the engine only through their registered syscall handler.

---

## QVM File Format

A `.qvm` file has two sections:

```
vmHeader_t {
    int32_t vmMagic;         // VM_MAGIC = 0x12721444 or VM_MAGIC_VER2 = 0x12721445
    int32_t instructionCount;
    int32_t codeOffset;
    int32_t codeLength;
    int32_t dataOffset;
    int32_t dataLength;
    int32_t litLength;       // ASCII strings (read-only, part of data)
    int32_t bssLength;       // zero-initialized (part of data)
}
```

- **Code segment:** Bytecode instructions (variable-length: opcode byte + optional 32-bit operand)
- **Data segment:** Initialized data (global variables) + literals (string constants)
- **BSS segment:** Zero-initialized globals (appended after data in memory)
- **Stack:** Not in the file. The engine allocates `PROGRAM_STACK_SIZE` (64KB) + `PROGRAM_STACK_EXTRA` (32KB extra) at the top of `dataBase`

Total VM address space: `dataLength + litLength + bssLength + PROGRAM_STACK_SIZE + guard`

All addresses are masked with `dataMask` to prevent out-of-bounds writes. The mask is computed as the next power-of-two minus one for the total allocation size.

---

## Execution Modes

```c
typedef enum {
    VMI_NATIVE,     // Load as shared library (.so / .dll) — no sandboxing
    VMI_COMPILED,   // JIT compile bytecode to native code
    VMI_BYTECODE    // Interpret bytecode (slowest, most portable)
} vmInterpret_t;
```

Selection logic in `VM_Create`:
1. Try `VMI_NATIVE` first if requested (loads `.so`/`.dll`)
2. If native fails or `fs_restrict` is set: try `VMI_COMPILED` (JIT)
3. If no JIT available for this arch (`NO_VM_COMPILED`): fall back to `VMI_BYTECODE`

On dedicated servers and most modern systems: JIT (`VMI_COMPILED`) is used.

---

## vm.c — Core VM Functions

**File:** `code/qcommon/vm.c`  
**Size:** ~2158 lines

### VM_Init()

Called once at engine startup. Registers `vmprofile`, `vminfo`, `vm_rtChecks` cvars.

### VM_Create(index, systemCalls, dllSyscalls, interpret)

```
VM_Create:
  if already loaded: return cached
  set vm->name = vmName[index], vm->systemCall = systemCalls
  vm->privateFlag = CVAR_PRIVATE  (prevents QVM from reading private cvars)

  if VMI_NATIVE:
    try VM_LoadDll(name) → if success: vm->entryPoint set, return
    fall through to VMI_COMPILED

  header = VM_LoadQVM(vm, qtrue)   ← load .qvm from filesystem
    validates header magic, sets up vm->dataBase (Hunk_Alloc)
    copies data+lit+bss into dataBase
    computes dataMask

  if VMI_COMPILED:
    VM_Compile(vm, header)           ← JIT compile (vm_x86.c / vm_aarch64.c etc.)
    vm->compiled = true

  else:
    VM_PrepareInterpreter2(vm, header) ← set up interpreter dispatch table

  VM_LoadSymbols(vm)               ← optional .map file for debug symbol names
  return vm
```

### VM_Restart(vm)

Reloads the QVM from disk without restarting the engine. Used for live mod reloading.

### VM_Call(vm, nargs, callnum, ...)

```
VM_Call:
  ++vm->callLevel                  (reentrant — QVM can call back into engine which calls QVM again)
  pack args into int32_t array
  if vm->entryPoint (native dll):
    r = vm->entryPoint(callnum, args...)
  elif vm->compiled (JIT):
    r = VM_CallCompiled(vm, nargs, args)    ← JIT entry
  else:
    r = VM_CallInterpreted2(vm, nargs, args) ← bytecode entry
  --vm->callLevel
  return r
```

**Reentrant note:** `vm->callLevel` tracks nesting. The engine can call the QVM (e.g. `GAME_RUN_FRAME`), which calls back into the engine (e.g. `G_TRACE`), which may be intercepted by `SV_Antilag_InterceptTrace`, which calls `SV_Trace`, which does NOT call back into the QVM. The callLevel counter ensures the VM state is properly saved/restored for each nesting level.

### VM_Free(vm)

Frees all JIT-compiled code and data. Called on map change or engine shutdown.

### VM_LoadSymbols(vm)

Reads optional `.map` file (same name as `.qvm`) for human-readable symbol names. Used by `VM_ValueToSymbol()` in stack traces.

---

## Bytecode Interpreter

**File:** `code/qcommon/vm_interpreted.c`

A simple switch-dispatch loop over the instruction stream. Each instruction is one `instruction_t`:
```c
typedef struct {
    int32_t value;     // operand (for OP_CONST, OP_LOCAL, etc.)
    byte    op;        // opcode_t
    byte    opStack;   // stack depth (for safety checks)
} instruction_t;
```

The interpreter maintains:
- `programStack` — pointer into QVM's data segment (stack grows downward from top of dataBase)
- `opStack` — operand stack for expression evaluation (separate from program stack)

Syscalls: when `OP_CALL` is executed with a negative call number, it's a syscall. The interpreter calls `vm->systemCall(args)` and pushes the result.

**Performance:** Approximately 3-5× slower than JIT. Used as fallback only.

---

## JIT Compilers

The JIT transforms QVM bytecode into native machine code at load time. `vm->codeBase.ptr` points to the mmap'd executable region.

### vm_x86.c — x86-64 JIT

**File:** `code/qcommon/vm_x86.c`  
**Size:** ~4886 lines — largest source file in the project

Key design:
- Single-pass code generation into a fixed buffer
- Operand stack is kept in x86 registers (eax, ecx, edx, etc.) — no stack spills for short sequences
- `instructionPointers[]` table maps QVM instruction index → native code offset (for branch targets)
- Syscall dispatch: negative call → call into `vm->systemCall`

`VM_Compile(vm, header)` entry point:
1. First pass: `VM_LoadInstructions` — parse bytecode into `instruction_t` array
2. `VM_CheckInstructions` — safety validation (no jumps to invalid targets, no stack corruption)
3. `VM_Fixup` — optimize: fold sequential loads, identify near-jumps, mark FPU instructions
4. Second pass: emit x86-64 code for each instruction

### vm_aarch64.c — ARM64 JIT

**File:** `code/qcommon/vm_aarch64.c`  
**Size:** ~3537 lines

Same design as x86 but targeting AArch64 (used on ARM servers, Apple Silicon Macs, Raspberry Pi 4, etc.).

### vm_armv7l.c — ARMv7 JIT

**File:** `code/qcommon/vm_armv7l.c`  
**Size:** ~3258 lines

32-bit ARM (Raspberry Pi 3 and older). Uses Thumb-2 instruction encoding for code density.

---

## Syscall Interface

When QVM code calls a negative function number, it's a syscall. The engine registered a `syscall_t` function pointer at `VM_Create` time.

### Engine → QVM (entry points)

The engine calls `VM_Call(vm, nargs, callnum, ...)` to invoke QVM functions:

| Module | Callnum | Engine calls |
|---|---|---|
| qagame | `GAME_INIT` | `SV_InitGameProgs()` |
| qagame | `GAME_RUN_FRAME` | Every QVM game frame in `SV_Frame()` |
| qagame | `GAME_CLIENT_THINK` | `SV_ClientThink()` per usercmd |
| qagame | `GAME_CLIENT_CONNECT` | Client connecting |
| qagame | `GAME_CLIENT_DISCONNECT` | Client disconnecting |
| qagame | `GAME_CONSOLE_COMMAND` | Console command to game |
| cgame | `CG_INIT` | `CL_InitCGame()` |
| cgame | `CG_DRAW_ACTIVE_FRAME` | `CL_CGameRendering()` per frame |
| cgame | `CG_CONSOLE_COMMAND` | Command routed to cgame |
| ui | `UI_INIT` | `CL_InitUI()` |
| ui | `UI_DRAW_CONNECT_SCREEN` | During connection |
| ui | `UI_REFRESH` | Every frame when UI active |

### QVM → Engine (syscalls)

Full tables in `GAME_INTERFACES.md`. Key principle: the engine validates all pointer arguments from the QVM before using them (`VM_CHECKBOUNDS` macro) to prevent QVM code from reading/writing outside its data segment.

### CVAR_PROTECTED — Preventing QVM Overrides

`Cvar_SetSafe(name, value)` — used for all QVM cvar sets:
- If cvar has `CVAR_PROTECTED` flag: print warning and **silently discard** the set
- Prevents QVM from overriding server-controlled settings

This is how `sv_fps`, `sv_gameHz`, and other custom cvars are protected from being overwritten by the UT4.3 QVM.

---

## JTS — Jump Target Sidecar Files

**A Quake3e extension.** `.jts` files contain a precomputed jump target table for a specific QVM binary (verified by CRC32). They allow the JIT to skip the `VM_CheckInstructions` validation pass on subsequent loads.

`Load_JTS(vm, crc32, data, vmPakIndex)`:
- Looks for `vm/qagame.jts` (etc.) in the same pak as the QVM
- Verifies `crc32` against the stored value in the JTS header
- Verifies `fs_lastPakIndex` matches (prevents cross-pak JTS spoofing)
- If valid: fills `vm->jumpTableTargets` array, skips slow validation

---

## VM_ReplaceInstructions — Binary Patching

`VM_ReplaceInstructions(vm, buf)`:
- Replaces specific bytecode instructions in a loaded QVM
- `buf` is an `instruction_t` array of the same length as the QVM's instruction stream
- Only replaces instructions where `buf[i].op != OP_UNDEF` — effectively a selective patch

This is the engine-level support for the Ghidra binary patches described in `archive/docs/ghidra-cgame-patches.md`. Rather than editing the `.qvm` file directly, a patch can be applied in memory after load.

### UrbanTerror 4.3 cgame patches

The three patches from the Ghidra analysis are applied automatically at runtime when the official UrbanTerror 4.3 `cgame.qvm` is loaded (CRC32 `0x1289DB6B`, `instructionCount` 258563, `exactDataLength` 38055548).

#### Server-type gating — vanilla vs. custom server

**Patch 1** (bit 2, TR_LINEAR) requires the server to anchor `pos.trTime = sv.time` for every `TR_INTERPOLATE` snapshot entity.  Our custom server does this unconditionally; vanilla URT servers never do, so the patch produces enormous `dt` values that teleport all entities every frame on vanilla servers.

The engine detects server type in `CL_ParseServerInfo()` (the same detection used by `cl_adaptiveTiming`) and automatically selects the appropriate patch bitmask:

| Situation | Cvar used | Default value |
|---|---|---|
| Custom server (`sv_snapshotFps` present in serverinfo) | `cl_urt43cgPatches` | `7` (all three patches) |
| Vanilla server (`sv_snapshotFps` absent) | `cl_qvmPatchVanilla` | `0` (no patches — safe baseline for A/B testing) |

The bridge is a `CVAR_TEMP` cvar **`cl_urt43serverIsVanilla`** set to `1` or `0` by `CL_ParseServerInfo()` immediately after `cl.vanillaServer` is determined.  `VM_URT43_CgamePatches()` reads it at VM load time to choose which user cvar to apply.  The TEMP flag ensures it never persists to the config file.

#### Bitmask layout (shared by both cvars)

| Bit | Name | What it fixes | Custom server | Vanilla server |
|---|---|---|---|---|
| 0 (1) | **Patch 2 — frameInterpolation clamp** | The existing QVM clamp is wrong: lower threshold is `0.1f` (should be `0.0f`) and upper clamped value is `~0.99f` (should be `1.0f`). Fixes two constants at instruction indices `0xa688` and `0xa692`. Safe on any server — no server-side dependency. | ✅ enabled | ⬜ off by default (`cl_qvmPatchVanilla 0`) |
| 1 (2) | **Patch 3 — nextSnap NULL crash fix** | `CG_InterpolateEntityPosition` calls `CG_Error()` (fatal crash) when `cg.nextSnap == NULL` during lag spikes. Replaces the 5-instruction error path with a `CONST`+`JUMP` early return to the function's `PUSH`+`LEAVE` (instr `0x1594d`). Safe on any server — no server-side dependency. | ✅ enabled | ⬜ off by default (`cl_qvmPatchVanilla 0`) |
| 2 (4) | **Patch 1 — TR_INTERPOLATE velocity extrapolation** | The `BG_EvaluateTrajectory` switch-table entry for `trType == TR_INTERPOLATE` (case 1, data addr `0xdbb4`) points to the `TR_STATIONARY` handler (VectorCopy only). Redirects it to the `TR_LINEAR` handler (`instr 0x3ab15`) so velocity-based forward extrapolation is used when the next snapshot is unavailable. **Requires server-side `trTime` anchor** — vanilla servers leave `trTime = 0`, causing `(evalTime − 0) / 1000` → enormous `dt` → entity teleportation every frame. Auto-suppressed on vanilla servers via `cl_qvmPatchVanilla`. | ✅ enabled | ❌ unsafe — leave unset |

The function `VM_URT43_CgamePatches()` in `vm.c` prints verbose diagnostic output for every patch attempt (instruction opcodes and values before patching, applied/skipped result) to aid debugging if a future QVM version changes the binary layout.

**Debug output example** — custom server (all patches, `cl_urt43cgPatches 7`):
```
UrT43 cgame patch: CRC=1289DB6B ic=258563 dl=38055548 flags=0x7 (custom server → cl_urt43cgPatches)
  [Patch2] frameInterpolation clamp:
    [0xa688] op=0x08 val=0x3dcccccd (expect op=0x08 val=0x3dcccccd)
    [0xa692] op=0x08 val=0x3f7d70a4 (expect op=0x08 val=0x3f7d70a4)
    [Patch2] APPLIED: lower=0.0f upper=1.0f
  [Patch3] CG_InterpolateEntityPosition null crash:
    ...
    [Patch3] APPLIED: CG_Error->early return at instr 0x1594d
  [Patch1] BG_EvaluateTrajectory TR_INTERPOLATE->TR_LINEAR:
    ...
    [Patch1] APPLIED: TR_INTERPOLATE now uses TR_LINEAR extrapolation
UrT43 cgame patch: applied=0x7 skipped=0x0
```

**Debug output example** — vanilla server (no patches by default, `cl_qvmPatchVanilla 0`):
```
UrT43 cgame patch: CRC=1289DB6B ic=258563 dl=38055548 flags=0x0 (vanilla server → cl_qvmPatchVanilla)
  [Patch2] DISABLED by cvar (bit 0 not set)
  [Patch3] DISABLED by cvar (bit 1 not set)
  [Patch1] DISABLED by cvar (bit 2 not set)
UrT43 cgame patch: applied=0x0 skipped=0x0
```

To enable Patches 2+3 on vanilla servers for testing: `cl_qvmPatchVanilla 3`

If the loaded QVM does not match the expected CRC/size, a `DPrintf` warning identifies the actual fingerprint so a new patch entry can be written.

---

## Runtime Checks

`vm_rtChecks` cvar (bitmask):

| Bit | Name | Effect |
|---|---|---|
| 1 | `VM_RTCHECK_PSTACK` | Check program stack bounds on every ENTER/LEAVE |
| 2 | `VM_RTCHECK_OPSTACK` | Check operand stack bounds on every instruction |
| 4 | `VM_RTCHECK_JUMP` | Validate all jump targets at runtime |
| 8 | `VM_RTCHECK_DATA` | Check all memory load/store bounds |

Default: `7` (pstack + opstack + jumps). Data check (`8`) adds significant overhead.

---

## Key Data Structures

### vm_t — Virtual Machine Instance

```c
struct vm_s {
    syscall_t    systemCall;       // engine function called by QVM
    byte         *dataBase;        // QVM memory space (Hunk allocated)
    int32_t      *opStack;         // operand stack pointer
    int32_t      *opStackTop;      // top of operand stack

    int          programStack;     // current stack pointer in dataBase
    int          stackBottom;      // lowest valid programStack value

    const char   *name;            // "qagame", "cgame", "ui"
    vmIndex_t    index;            // VM_GAME, VM_CGAME, VM_UI

    void         *dllHandle;       // if native mode: OS library handle
    dllSyscall_t entryPoint;       // native DLL entry point

    qboolean     compiled;         // true if JIT compiled
    vmFunc_t     codeBase;         // JIT code base pointer
    unsigned int codeSize;         // JIT code mmap size
    unsigned int codeLength;       // bytecode length

    int          instructionCount;
    intptr_t     *instructionPointers; // JIT: instruction index → native offset

    unsigned int dataMask;         // address mask (power-of-2 minus 1)
    unsigned int dataLength;
    unsigned int dataAlloc;

    int          numSymbols;
    vmSymbol_t   *symbols;         // debug symbol list

    int          callLevel;        // reentrant call nesting depth
    int          privateFlag;      // CVAR_PRIVATE — blocks private cvar reads
    int          *jumpTableTargets; // JTS jump table
    int          numJumpTableTargets;
};
```

### instruction_t — Decoded Bytecode Instruction

```c
typedef struct {
    int32_t  value;     // immediate operand
    byte     op;        // opcode_t
    byte     opStack;   // operand stack depth before this instruction
    unsigned jused:1;   // is a jump target
    unsigned swtch:1;   // indirect jump (switch)
    unsigned safe:1;    // STORE with bounds-checked address
    unsigned fpu:1;     // value should load into FPU register
    unsigned njump:1;   // near jump (fits in 8-bit offset)
} instruction_t;
```

---

## Opcode Reference

| Opcode | Stack Effect | Description |
|---|---|---|
| `OP_ENTER` | — | Function prologue; subtracts locals from programStack |
| `OP_LEAVE` | — | Function epilogue; restores programStack |
| `OP_CALL` | pop addr → push result | Call function at address; negative = syscall |
| `OP_PUSH` | → push 0 | Push zero |
| `OP_POP` | pop x → | Discard top |
| `OP_CONST` | → push imm | Push 32-bit immediate |
| `OP_LOCAL` | → push (sp+offset) | Push address of local variable |
| `OP_JUMP` | pop addr → | Unconditional jump |
| `OP_EQ/NE/LTI/LEI/GTI/GEI` | pop a, pop b → | Signed int compare + conditional jump |
| `OP_LTU/LEU/GTU/GEU` | pop a, pop b → | Unsigned int compare + conditional jump |
| `OP_EQF/NEF/LTF/LEF/GTF/GEF` | pop a, pop b → | Float compare + conditional jump |
| `OP_LOAD1/2/4` | pop addr → push val | Load 1/2/4 bytes from address |
| `OP_STORE1/2/4` | pop addr, pop val → | Store 1/2/4 bytes to address |
| `OP_ARG` | pop val → | Store as function argument (at sp+offset) |
| `OP_BLOCK_COPY` | pop dst, pop src → | memcpy N bytes |
| `OP_SEX8/16` | pop x → push sign_ext(x) | Sign-extend 8 or 16 bits to 32 bits |
| `OP_NEGI/ADD/SUB/DIVI/DIVU/MODI/MODU/MULI/MULU` | arithmetic ops | Integer arithmetic |
| `OP_BAND/BOR/BXOR/BCOM` | bitwise ops | Bitwise ops |
| `OP_LSH/RSHI/RSHU` | shift ops | Left/right arithmetic/logical shift |
| `OP_NEGF/ADDF/SUBF/DIVF/MULF` | float ops | Float arithmetic |
| `OP_CVIF` | pop int → push float | Int to float conversion |
| `OP_CVFI` | pop float → push int | Float to int conversion (truncation) |
