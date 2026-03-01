# Quake3e-subtick — Full Engine Overview

> **How to use this documentation:** Start here to find which file owns any piece of functionality, then follow the link to the relevant section doc for detailed function listings, data structures, and interaction notes.
>
> **Custom changes** made in this fork are marked **[CUSTOM]**. Everything else is stock Quake3e / ioquake3.

---

## Repository Layout

```
code/
├── qcommon/          ← Core engine: memory, console, cvar, cmd, filesystem, math, crypto
│   ├── common.c/h    ← Com_Init, Com_Frame, memory zones, hunk, logging
│   ├── cvar.c        ← Console variable system
│   ├── cmd.c         ← Command buffer and execution
│   ├── files.c       ← Virtual filesystem (PAK/PK3), path resolution
│   ├── msg.c         ← Bit-level message serialization + delta compression
│   ├── net_chan.c     ← Reliable/unreliable channel layer over UDP
│   ├── net_ip.c      ← OS socket abstraction (IPv4 + IPv6)
│   ├── vm.c          ← QVM loader, symbol tables, dispatch
│   ├── vm_x86.c      ← x86-64 JIT compiler
│   ├── vm_aarch64.c  ← ARM64 JIT compiler
│   ├── vm_armv7l.c   ← ARMv7 JIT compiler
│   ├── vm_interpreted.c ← Bytecode interpreter fallback
│   ├── cm_load.c     ← BSP collision-map loader
│   ├── cm_trace.c    ← Ray/box/capsule trace engine
│   ├── cm_patch.c    ← Curved-surface patch collision
│   ├── cm_test.c     ← Point-in-solid tests, area portals
│   ├── q_shared.c/h  ← Math library, string utils, shared types
│   ├── q_math.c      ← Trigonometry, vector ops, matrix ops
│   ├── huffman.c     ← Huffman compression for network packets
│   ├── md4.c/md5.c   ← Hash functions (pak checksums, auth)
│   └── unzip.c       ← Zlib/deflate for PK3 file reading
│
├── server/           ← Dedicated / listen server
│   ├── sv_main.c     ← [CUSTOM] Frame loop, sv_fps/sv_gameHz decoupling
│   ├── sv_init.c     ← [CUSTOM] Cvar registration (all custom cvars here)
│   ├── sv_client.c   ← [CUSTOM] Usercmd handling, multi-step Pmove
│   ├── sv_snapshot.c ← [CUSTOM] Snapshot building, position extrapolation
│   ├── sv_antilag.c  ← [CUSTOM] Engine-side shadow antilag (new file)
│   ├── sv_antilag.h  ← [CUSTOM] Antilag public API (new file)
│   ├── sv_game.c     ← QVM game syscall handler (G_TRACE intercept)
│   ├── sv_world.c    ← Entity world sectors, SV_Trace, SV_LinkEntity
│   ├── sv_ccmds.c    ← [CUSTOM] Server console commands, map restart fix
│   ├── sv_bot.c      ← Bot integration with server
│   ├── sv_net_chan.c ← Server-side netchan encode/decode
│   ├── sv_filter.c   ← IP ban/filter system
│   ├── sv_rankings.c ← Rankings / statistics tracking
│   └── server.h      ← Server-internal types (server_t, client_t, etc.)
│
├── client/           ← Client (renders, input, network, sound)
│   ├── cl_main.c     ← CL_Init, CL_Frame, connect/disconnect, demo recording
│   ├── cl_input.c    ← [CUSTOM] Key→usercmd, mouse, download pacing
│   ├── cl_cgame.c    ← [CUSTOM] cgame QVM interface, time sync, cvar intercept
│   ├── cl_parse.c    ← [CUSTOM] Network message parser, snapshot interval EMA
│   ├── cl_console.c  ← In-game console rendering and history
│   ├── cl_keys.c     ← Key binding management
│   ├── cl_scrn.c     ← Screen layout, loading plaque, FPS counter
│   ├── cl_ui.c       ← UI QVM interface
│   ├── cl_cin.c      ← Cinematic video (RoQ decoder)
│   ├── cl_jpeg.c     ← JPEG screenshot writer
│   ├── cl_avi.c      ← AVI video capture
│   ├── cl_net_chan.c  ← Client-side netchan (decode, demo filtering)
│   ├── cl_curl.c/h   ← HTTP download support (libcurl integration)
│   ├── snd_main.c    ← Sound system dispatcher (picks DMA or HD backend)
│   ├── snd_dma.c     ← Base PCM DMA sound backend
│   ├── snd_dmahd.c   ← High-definition sound backend (higher quality mixing)
│   ├── snd_mem.c     ← Sound data loading and caching
│   ├── snd_mix.c     ← Audio sample mixing
│   ├── snd_adpcm.c   ← ADPCM codec
│   ├── snd_codec.c   ← Sound codec dispatcher
│   ├── snd_codec_wav.c ← WAV file codec
│   ├── snd_wavelet.c ← Wavelet codec (Quake 3 .wav format variant)
│   └── client.h      ← [CUSTOM] clientActive_t with snapshotMsec field
│
├── renderer/         ← OpenGL renderer (legacy/fallback)
│   ├── tr_init.c     ← R_Init, GL context setup, GL extensions
│   ├── tr_main.c     ← Scene setup, view transforms, frustum
│   ├── tr_backend.c  ← Render backend: state machine, draw calls
│   ├── tr_bsp.c      ← BSP world rendering, PVS, portals
│   ├── tr_shader.c   ← Shader parser and cache
│   ├── tr_image.c    ← Texture loading, mip generation, GL upload
│   ├── tr_scene.c    ← RE_ClearScene, RE_AddRefEntityToScene
│   ├── tr_surface.c  ← Surface dispatch (mesh, fog, flare, etc.)
│   ├── tr_light.c    ← Dynamic lighting
│   ├── tr_marks.c    ← Decal/mark system
│   ├── tr_shadows.c  ← Stencil shadows
│   ├── tr_sky.c      ← Skybox rendering
│   ├── tr_shade.c    ← Per-surface shading (multi-pass)
│   ├── tr_shade_calc.c ← Shader animation calculations (tcMod, etc.)
│   ├── tr_curve.c    ← Bezier patch tessellation
│   ├── tr_mesh.c     ← MD3 model surface rendering
│   ├── tr_model.c    ← MD3/MDC/IQM model loader
│   ├── tr_model_iqm.c ← IQM skeletal animation loader
│   ├── tr_animation.c ← MD3 tag/frame interpolation
│   ├── tr_arb.c      ← ARB assembly shader extensions
│   ├── tr_cmds.c     ← Render command list (thread-safe submission)
│   ├── tr_flares.c   ← Lens flare system
│   ├── tr_world.c    ← World surface culling and submission
│   └── tr_vbo.c      ← Vertex Buffer Object management
│
├── renderervk/       ← Vulkan renderer (preferred on modern hardware)
│   ├── vk.c/h        ← Core Vulkan: device, swapchain, pipelines, descriptors
│   ├── vk_flares.c   ← Vulkan lens flares
│   ├── vk_vbo.c      ← Vulkan vertex/index buffers
│   └── tr_*.c        ← Same structure as renderer/ but using Vulkan API
│
├── renderercommon/   ← Shared renderer utilities (both GL and VK use these)
│   ├── tr_public.h   ← refexport_t: the renderer's public function table
│   ├── tr_types.h    ← Shared renderer types (refEntity_t, refdef_t, etc.)
│   ├── tr_font.c     ← Font/glyph loading and rendering
│   ├── tr_noise.c    ← Procedural noise (for shader effects)
│   └── tr_image_*.c  ← Image loaders: BMP, JPEG, PCX, PNG, TGA
│
├── botlib/           ← Bot AI library (AAS pathfinding + behavior)
│   ├── be_interface.c ← Entry point: Export_BotLibSetup, all exports
│   ├── be_aas_*.c    ← AAS (Area Awareness System): BSP graph, routing
│   ├── be_ai_*.c     ← AI behaviors: movement, goals, weapons, chat, char
│   ├── be_ea.c       ← Elementary actions (move, attack, etc.)
│   └── l_*.c         ← Support libs: memory, logging, script parser, CRC
│
├── game/             ← Interface headers for QVM modules
│   ├── g_public.h    ← Engine→game (syscall IDs, GAME_* entry points, entityShared_t)
│   └── bg_public.h   ← Shared physics types (trajectory_t, playerState_t, entityState_t)
│
├── cgame/
│   └── cg_public.h   ← Engine→cgame (CG_* syscall IDs, cgame entry points)
│
├── ui/
│   └── ui_public.h   ← Engine→UI module syscall interface
│
├── unix/             ← Linux/Unix platform layer
├── win32/            ← Windows platform layer
└── sdl/              ← SDL2 platform layer (input, window, audio)
```

---

## Subsystem Dependency Graph

```
                    ┌─────────────────────────────┐
                    │         Com_Frame()          │
                    │   (qcommon/common.c)         │
                    └──────────┬──────────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
         SV_Frame()       CL_Frame()        Renderer
         (server)         (client)         (tr_init.c)
              │                │
         ┌────┴────┐      ┌────┴────┐
         │  Game   │      │ cgame   │
         │  QVM    │      │  QVM    │
         └────┬────┘      └────┬────┘
              │                │
         ┌────▼────┐      ┌────▼────┐
         │ Botlib  │      │ Sound   │
         └────┬────┘      └─────────┘
              │
         ┌────▼────────────────────┐
         │  qcommon (all use it)   │
         │  CM, VM, FS, NET, MSG   │
         └─────────────────────────┘
```

---

## Startup Sequence

```
main() → Com_Init(commandLine)
  Sys_Init()               ← OS-specific init (console, affinity)
  Com_InitZoneMemory()     ← heap zones
  Com_InitHunkMemory()     ← hunk allocator
  Cvar_Init()              ← cvar hash table
  Cmd_Init()               ← command table, built-in commands
  FS_InitFilesystem()      ← PAK/PK3 search paths
  NET_Init()               ← sockets
  Netchan_Init()           ← channel layer
  VM_Init()                ← QVM dispatch setup
  SV_Init()                ← server cvars, antilag init [CUSTOM]
  CL_Init()                ← client cvars, renderer, sound, UI

Com_Frame() loop (called by Sys_ConsoleInputEvent / main loop)
  CL_Frame(msec)           ← input, time sync, cgame, renderer
  SV_Frame(msec)           ← game logic, snapshots, antilag
```

---

## Section Documents

| File | Contents |
|------|----------|
| `QCOMMON.md` | Memory, console, cvar, cmd, filesystem, math, huffman, hash |
| `NETWORKING.md` | NET sockets, Netchan protocol, MSG bit-packing, LZSS compression |
| `COLLISION.md` | CM map loading, trace engine, patches, area portals |
| `QVM.md` | QVM virtual machine, JIT compilers, bytecode, syscall contract |
| `SERVER.md` | Full server subsystem — all files, all functions, all custom changes |
| `CLIENT.md` | Full client subsystem — input, time sync, cgame, UI, console, demo |
| `SOUND.md` | DMA and HD sound backends, mixing, codecs |
| `RENDERER.md` | OpenGL and Vulkan renderers, scene graph, shaders |
| `BOTLIB.md` | AAS pathfinding, AI behaviors, elementary actions |
| `GAME_INTERFACES.md` | QVM syscall tables: g_public.h, cg_public.h, bg_public.h |
| `PLATFORM.md` | OS layers: unix/, win32/, sdl/ |

---

## Key Global State

| Variable | Type | Owner | Purpose |
|---|---|---|---|
| `sv` | `server_t` | sv_main.c | Per-map server state: time, gameTime, entity arrays |
| `svs` | `serverStatic_t` | sv_main.c | Persistent server state: clients[], time, nextHeartbeat |
| `gvm` | `vm_t *` | sv_main.c | Game QVM handle |
| `cl` | `clientActive_t` | cl_main.c | Active client game state: serverTime, snaps, snapshotMsec |
| `clc` | `clientConnection_t` | cl_main.c | Connection state: netchan, serverAddress, demo recording |
| `cls` | `clientStatic_t` | cl_main.c | Persistent client state: realtime, rendererStarted |
| `cgvm` | `vm_t *` | cl_cgame.c | cgame QVM handle |
| `uivm` | `vm_t *` | cl_ui.c | UI QVM handle |
| `cm` | `clipMap_t` | cm_load.c | Loaded BSP collision model |
| `tr` | `trGlobals_t` | tr_init.c | Renderer state (either GL or VK version) |
| `botlib_export` | `botlib_export_t *` | sv_game.c | Botlib function table |

---

## Custom Changes Summary (This Fork)

All engine changes relative to stock Quake3e:

| File | Change |
|------|--------|
| `server/sv_main.c` | Dual-rate frame loop (sv_fps outer + sv_gameHz inner); antilag recording; snapshot dispatch inside loop |
| `server/sv_init.c` | New cvars: sv_gameHz, sv_snapshotFps, sv_busyWait, sv_pmoveMsec, sv_extrapolate, sv_smoothClients, sv_bufferMs, sv_velSmooth |
| `server/sv_client.c` | Multi-step Pmove (sv_pmoveMsec), bot exclusion, sv_snapshotFps policy in SV_UserinfoChanged |
| `server/sv_snapshot.c` | Engine-side position extrapolation; TR_LINEAR smoothing; sv_bufferMs ring buffer; sv_velSmooth averaging |
| `server/sv_antilag.c` | **New file** — entire engine-side shadow antilag system |
| `server/sv_antilag.h` | **New file** — antilag public API header |
| `server/sv_ccmds.c` | SV_MapRestart_f: sync sv.gameTime before restart |
| `client/client.h` | Added `snapshotMsec` to `clientActive_t` |
| `client/cl_parse.c` | Snapshot interval EMA measurement; bootstrap from sv_snapshotFps configstring |
| `client/cl_cgame.c` | Proportional time-sync thresholds; serverTime clamp; QVM cvar intercept for snaps/cg_smoothClients |
| `client/cl_input.c` | Download pacing uses snapshotMsec instead of hardcoded 50ms |
| `qcommon/net_ip.c` | net_dropsim changed CVAR_TEMP → CVAR_CHEAT |
