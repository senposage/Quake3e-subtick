# Rebase to timfox/idTech3 next-gen-5 -- Complete Implementation Plan

**Prepared**: 2026-03-21  
**Source repo**: senposage/Quake3e-urt (Quake3e-based, ~467k LOC, GNU Make, C99)  
**Target upstream**: timfox/idTech3 @ next-gen-5 (~2.1M LOC, CMake 3.24+, C23/C++17)  
**Purpose**: Adopt all upstream engine improvements, then re-apply our URT-specific patches
on top of the new base.  The PBR-from-BSP feature (auto-generate usable PBR data for maps
that ship no PBR assets) is the motivating feature and is covered in detail in Phase 6.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Repo Structure Diff](#2-repo-structure-diff)
3. [URT Patches to Preserve (Never Drop)](#3-urt-patches-to-preserve-never-drop)
4. [Upstream Features to Adopt](#4-upstream-features-to-adopt)
5. [Rebase Strategy](#5-rebase-strategy)
6. [PBR-from-BSP: Auto-Generate PBR Data for Legacy Maps](#6-pbr-from-bsp-auto-generate-pbr-data-for-legacy-maps)
7. [Phase-by-Phase Implementation Plan](#7-phase-by-phase-implementation-plan)
8. [Agent Assignment Matrix](#8-agent-assignment-matrix)
9. [Build System Migration](#9-build-system-migration)
10. [Testing & Validation Checklist](#10-testing--validation-checklist)
11. [Risk Register](#11-risk-register)

---

## 1. Executive Summary

`timfox/idTech3 next-gen-5` is a comprehensive next-generation idTech3 fork with:
- A full Vulkan 1.4 PBR renderer (~444k LOC) replacing the OpenGL renderer2
- Physics (Bullet), Navigation (Recast/Detour), refactored audio, video codecs, Lua API,
  DTLS encryption, Steam integration, and an AI gameplay system
- CMake 3.24+ build system with C23/C++17 standards

Our repo (`Quake3e-urt`) has URT-specific patches on top of Quake3e that must be preserved:
- URT auth server (`USE_AUTH`)
- Rankings system, TLD filter, demo recording

Several features we carried in our fork are already present in the upstream base and will
NOT be re-ported (see section 3.6).

**Strategy**: Treat the rebase as a clean re-implementation rather than a git merge.
Take upstream as the new base, migrate our patches as new additions, and implement the
PBR-from-BSP feature on top of the upstream Vulkan PBR stack.

---

## 2. Repo Structure Diff

### Directory mapping (our `code/` -> upstream `src/`)

| Our `code/` path | Upstream `src/` equivalent | Notes |
|---|---|---|
| `code/client/` | `src/client/` | Upstream has 35+ new files |
| `code/server/` | `src/server/` | Upstream has `sv_enhanced.c` |
| `code/qcommon/` | `src/qcommon/` | Upstream adds jobs, defer, lua, DTLS |
| `code/renderer2/` | `src/renderers/vulkan/` | 50k LOC -> 444k LOC |
| `code/renderer/` | `src/renderers/opengl/` | OpenGL fallback preserved |
| `code/renderercommon/` | `src/renderers/common/` | Shared image loaders |
| `code/renderervk/` | merged into vulkan/common | |
| `code/client/snd_*.c` | `src/audio/` | Fully refactored audio subsystem |
| `code/libcurl/` | `src/external/src/curl/` | Vendored in `external/` |
| `code/libjpeg/` | `src/external/src/jpeg/` | |
| `code/libogg/` | `src/external/src/ogg/` | |
| `code/libopenal/` | system dep or `external/` | |
| `code/libvorbis/` | `src/external/src/vorbis/` | |
| `code/libsdl/` | `src/external/src/sdl/` | |
| `code/botlib/` | `src/botlib/` | Unchanged |
| `code/cgame/` | `src/cgame/` | Minimal diffs expected |
| `code/game/` | `src/game/` | Upstream added AI/gameplay stubs |
| `code/ui/` | `src/ui/` | |
| `code/asm/` | `src/asm/` | |
| _(none)_ | `src/physics/` | NEW: Bullet physics |
| _(none)_ | `src/navigation/` | NEW: Recast/Detour navmesh |
| _(none)_ | `src/external/` | NEW: vendored deps |

### New subsystems in upstream (no equivalent in our repo)

| Subsystem | Directory | Description |
|---|---|---|
| Physics | `src/physics/` | Bullet Physics, IK, DMM, cloth (XPBD), ragdoll |
| Navigation | `src/navigation/` | Recast/Detour navmesh, BSP triangle extraction |
| Lua API | `src/qcommon/lua_*.c` | 64 engine functions callable from Lua (via Duktape) |
| Job system | `src/qcommon/jobs.c/h` | Thread-pool job scheduler |
| Defer | `src/qcommon/defer.c/h` | Deferred resource cleanup |
| DTLS | `src/qcommon/net_dtls.c/h` | AES-256-GCM network encryption |
| Steam SDR | `src/qcommon/net_sdr.c/h` | Steamworks networking relay |
| Steam API | `src/client/cl_steam.c/h` | Achievements, overlay, rich presence |
| Particles | `src/client/cl_particles.c/h` | 8192-pool client particle system |
| Background map | `src/client/cl_map_background.c/h` | Menu background maps |
| Video codecs | `src/client/cl_cin_*.c` | FFmpeg, dav1d, libvpx, Theora |
| SDF fonts | `src/client/cl_sdf_font.c/h` | GPU signed-distance-field font rendering |
| Adaptive music | `src/audio/snd_music_adaptive.c/h` | Intensity-driven music layers |
| Geometry acoustics| `src/audio/effects/snd_acoustics_efx.h` | Room acoustics via EFX |
| AI gameplay | `src/game/g_director.c` etc. | Director, GOAP, Horde, Response, Choreography |
| ImGui inspector | `src/renderers/vulkan/inspector/` | Debug overlay |

### New renderer capabilities (upstream Vulkan vs our renderer2)

| Feature | Our renderer2 | Upstream Vulkan |
|---|---|---|
| API | OpenGL 3.x | Vulkan 1.4 |
| PBR | Basic metalness/roughness | Full Cook-Torrance BRDF, IBL, BRDF LUT, multi-scatter |
| PBR physical maps | `_s` specular only | ORM / RMO / MOXR / ORMS packed maps, auto-discovery |
| Normal maps | `_n`/`_nh` auto-detect, Sobel gen | `_n`/`_nh` auto-detect, MikkTSpace tangents |
| Shadows | CSM placeholder | CSM (sun), spot atlas, point cubemap array |
| Volumetric fog | None | Froxel compute, Navier-Stokes fluid sim, temporal reprojection |
| Anti-aliasing | None | SMAA + MSAA |
| SSAO | None | SSAO (Halton) + HBAO |
| Bloom/HDR | Basic | ACES/Reinhard/Filmic/AgX, eye adaptation |
| Post-process | Basic | DOF, motion blur, panini, vignette, chromatic aberration, film grain |
| OIT | None | Weighted Blended OIT |
| Atmosphere | None | Sky atmosphere scattering shader |
| SSR | None | Screen-space reflections |
| Glint NDF | None | Procedural microfacet NDF |
| Terrain | None | GPU compute CBT tessellation |
| Vegetation | None | GPU compute wind deformation |
| GPU occlusion | None | Per-entity occlusion queries |
| Model formats | MD3, IQM | + glTF 2.0, OBJ, MD5, morph targets |
| Image formats | PNG/TGA/JPG/BMP/PCX/DDS | + EXR (HDR), KTX2, QOI, SVG |
| Texture compression | DXT/BC1-3 | + BC7, KTX2 |
| HDR skybox | None | EXR equirectangular + IBL |

---

## 3. URT Patches to Preserve (Never Drop)

These are unique to our fork and must be re-implemented on top of the upstream base.
Each is listed with its source location and a brief description.

### 3.1 URT Auth Server (`USE_AUTH`)

**Files**: `code/client/cl_main.c`, `code/server/sv_main.c`, `code/server/sv_client.c`,
`code/qcommon/qcommon.h`, `code/server/server.h`, `code/client/client.h`

**What it does**: Validates clients against `authserver.urbanterror.info`.
AUTH:CL packets arrive from the auth server (separate from the Q3 authorize server).
The server sends player GUIDs to the auth server; the auth server responds with allow/deny.

**Key identifiers**: `USE_AUTH`, `cls.authorizeServer` (Q3 vanilla),
`authserver.urbanterror.info`, `AUTH:CL`, `AUTH:SV`, `SV_AuthPacket`,
`CL_AuthPacket`, `sv_authserver` cvar.

**Re-implementation note**: Upstream `sv_enhanced.c` handles unlagged/voting/physics
tweaks but does NOT have URT auth. Add a new `sv_urt_auth.c` / `sv_urt_auth.h` pair
that implements the AUTH handshake, registered in `SV_Init`.

### 3.2 Rankings System

**Files**: `code/server/sv_rankings.c`

**What it does**: Tracks kill/death/score rankings on the server, exposed via a
queryable interface (used by the URT stats/matchmaking infrastructure).

**Re-implementation note**: Add as `src/server/sv_urt_rankings.c`. No equivalent
in upstream.

### 3.3 TLD Filter

**Files**: `code/server/tlds.h`

**What it does**: Contains a table of TLDs used for IP-to-country geolocation
or player filtering on the URT server.

**Re-implementation note**: Copy `tlds.h` directly to `src/server/tlds.h`, no
functional change needed.

### 3.4 Demo Recording (`USE_SERVER_DEMO`, `USE_URT_DEMO`)

**Files**: `code/server/sv_main.c`, `code/server/sv_client.c`,
`code/client/cl_main.c`

**What it does**: Allows server-side demo recording of all clients simultaneously
(`USE_SERVER_DEMO`) and a URT-specific demo format extension (`USE_URT_DEMO`).

**Re-implementation note**: Add as `src/server/sv_urt_demo.c`. The demo recording
path interfaces with `sv_snapshot.c`; ensure compatibility with upstream's snapshot
system.

### 3.5 URT Build Flags

**Makefile flags to carry forward into CMake**:
- `USE_AUTH=1`
- `USE_SERVER_DEMO=1`
- `USE_URT_DEMO=1`
- `CNAME=quake3e-urt`
- `DNAME=quake3e-urt.ded`

Add a `cmake/URT.cmake` include file that sets URT-specific compile definitions.

### 3.6 Features Superseded by Upstream (Not Ported)

The following features existed in our fork but are already present -- and in most cases
better implemented -- in the upstream base. They will NOT be re-ported. Our source files
for these are sidelined and can be deleted after the rebase is complete.

| Our feature | Our file(s) | Upstream equivalent | Decision |
|---|---|---|---|
| Lag compensation / anti-lag | `code/server/sv_antilag.c/h` | `sv_enhanced.c` -- `SV_Unlagged_*` | Drop. Use upstream. |
| Variable server game Hz | `code/server/sv_main.c` (`sv_gamehz`) | Upstream server tick scheduler | Drop. Use upstream. |
| Voice chat (VoIP) | `code/client/snd_openal.c` (`s_openalVoipSpatial`) | `src/audio/snd_main.c` -- `s_openalVoipSpatial`, `s_openalVoipGain` | Drop. Use upstream. |
| OpenAL audio backend | `code/client/snd_openal.c` (8106 lines) | `src/audio/backends/snd_backend_openal.c` | Sidelined. Adopt upstream wholesale. |

The upstream audio backend provides a cleaner abstraction, EFX geometry acoustics,
adaptive music layering, and built-in VoIP support. There is no net URT-specific logic
in our `snd_openal.c` that is not covered by the upstream implementation.

---

## 4. Upstream Features to Adopt

All upstream features are adopted by default (the upstream is the new base).
The following require explicit integration decisions.

### 4.1 Renderer: Vulkan PBR (PRIMARY ADOPTION)

The upstream Vulkan renderer replaces our `renderer2` entirely. All renderer features
listed in the comparison table (section 2.4) are adopted wholesale by taking upstream
as the new base. The following enhancements come for free and require no extra porting:

**Rendering pipeline**:
- Full Cook-Torrance BRDF with IBL, BRDF LUT, and multi-scatter PBR (replaces our basic metalness/roughness)
- ORM / RMO / MOXR / ORMS packed physical-map auto-discovery (replaces our `_s` specular-only path)
- MikkTSpace tangent-space normal maps (replaces our `_n`/`_nh` Sobel-generated normals)
- Cascaded shadow maps (sun), spot-light atlas, point-light cubemap array (replaces CSM placeholder)
- Weighted Blended Order-Independent Transparency
- Screen-Space Ambient Occlusion (Halton SSAO) + Horizon-Based AO (HBAO)
- Screen-Space Reflections (SSR)
- Volumetric fog (froxel compute, Navier-Stokes fluid sim, temporal reprojection)
- Sky atmosphere scattering shader
- Procedural microfacet NDF (glint)
- GPU compute CBT terrain tessellation
- GPU compute vegetation wind deformation
- Per-entity GPU occlusion queries

**Post-processing**:
- SMAA + MSAA anti-aliasing
- ACES / Reinhard / Filmic / AgX tone mapping with eye adaptation
- Depth of Field, motion blur, panini projection, vignette, chromatic aberration, film grain

**Asset support**:
- glTF 2.0, OBJ, MD5, morph targets (in addition to MD3 / IQM)
- EXR (HDR), KTX2, QOI, SVG image formats (in addition to PNG / TGA / JPG / DDS)
- BC7, KTX2 texture compression (in addition to DXT / BC1-3)
- EXR equirectangular HDR skybox + IBL
- SDF font rendering (`cl_sdf_font.c/h`)
- ImGui debug inspector (`src/renderers/vulkan/inspector/`)

**Key integration tasks** (URT-specific wiring only):
- Map our `r_pbr`, `r_normalMapping`, `r_specularMapping`, `r_genNormalMaps`,
  `r_baseGloss`, `r_baseNormalX/Y`, `r_baseParallax`, `r_baseSpecular` cvars to
  their upstream equivalents (`r_pbr`, `r_pbr_packedPreferred`, etc.)
- Ensure URT `.shader` files (in the `q3ut4/` game directory) are parseable by
  the upstream shader parser. The upstream parser supports all legacy Q3 directives.
- See Phase 5 for the BSP-specific PBR data generation work.

### 4.2 Audio: Upstream Backend (Wholesale Adoption)

The upstream audio system (`src/audio/`) is a complete rewrite with a backend
abstraction, EFX geometry acoustics, adaptive music layering, and built-in VoIP.
Our `snd_openal.c` (8106 lines) is sidelined entirely -- there is no net
URT-specific audio logic that is not already covered by the upstream backend.

**What we get for free**:
- VoIP with `s_openalVoipSpatial` and `s_openalVoipGain` (upstream `snd_main.c`)
- Geometry-based room acoustics via EFX (`snd_acoustics_efx.h`)
- Adaptive music layers (`snd_music_adaptive.c`)
- Cleaner backend abstraction (easier future maintenance)

**Decision**: Adopt upstream audio wholesale. No porting work required.
Delete `code/client/snd_openal.c` after rebase is confirmed stable.

### 4.3 Physics (Bullet)

URT does not use physics beyond Q3 movement. Bullet can be compiled in but
the game code (`src/game/`) will not use ragdoll/cloth/DMM unless a future
URT game update requests it.

**Decision**: Keep physics compiled in (it's in the engine layer, not game layer).
No URT-specific work needed; just ensure `q3ut4/` game VM doesn't crash when
physics entities exist.

### 4.4 Navigation (Recast/Detour + BSP Extract)

`nav_bsp_extract.c` is relevant to Phase 6: it provides `Nav_BSP_AddVertex` /
`Nav_BSP_AddTriangle` which walks BSP geometry. This same geometry extraction
can drive the AO-baking step in Phase 6.

**Decision**: Enable navigation. The BSP extractor is directly useful for
PBR data generation.

### 4.5 Lua API

The Lua API (`src/qcommon/lua_*.c`) exposes 64 engine functions. URT mods could
leverage this for server-side scripting (map configs, custom game modes).

**Decision**: Enable Lua (`USE_LUA=ON` in CMake). No URT conflict; additive feature.

### 4.6 DTLS Network Encryption

`src/qcommon/net_dtls.c/h` implements AES-256-GCM encrypted UDP.

**Decision**: Enable (`USE_DTLS=ON`). Beneficial for URT competitive play.
Ensure our URT auth handshake still works over DTLS.

### 4.7 Steam Integration

`src/client/cl_steam.c/h` and `src/qcommon/net_sdr.c/h`.

**Decision**: Optional compile (`USE_STEAM=ON`). Not core to URT but useful
for future distribution.

### 4.8 Build System: Make -> CMake

Our repo uses GNU Make; upstream uses CMake 3.24+.

**Decision**: Migrate to CMake (see Section 9).

---

## 5. Rebase Strategy

### 5.1 Why not `git rebase` or `git merge`

The two repos have completely diverged histories and directory structures.
A literal `git rebase` or `git merge` would produce thousands of conflicts
with no clean resolution path.

### 5.2 Recommended approach: Clean re-implementation

```
1. Create a new branch `urt/next-gen-5-rebase` from upstream HEAD
2. Copy upstream as-is (this IS the new base)
3. Add URT patches one subsystem at a time (Phases 1-5 below)
4. Implement PBR-from-BSP on top of the upstream Vulkan PBR stack (Phase 6)
5. Validate with the URT test maps and game (Phase 7)
```

### 5.3 Branch strategy for multi-agent work

Each phase below maps to one or more feature branches:

```
urt/next-gen-5-rebase          <- integration branch (merge target)
  urt/phase1-build             <- CMake + CI setup
  urt/phase2-auth              <- URT auth server
  urt/phase3-server            <- rankings, demo
  urt/phase4-renderer-compat   <- shader parser, texture compat
  urt/phase5-pbr-from-bsp      <- BSP PBR data generation (main feature)
  urt/phase6-validation        <- test pass, CI green
```

### 5.4 File-by-file migration notes

For each URT patch file, the migration approach is:

| Our file | Action |
|---|---|
| `sv_rankings.c` | Add as `src/server/sv_urt_rankings.c` |
| `tlds.h` | Copy to `src/server/tlds.h` |
| `snd_openal.c` | Sidelined -- adopt upstream audio backend wholesale (section 3.6) |
| `sv_antilag.c/h` | Deleted -- upstream `sv_enhanced.c` covers this (section 3.6) |
| `cl_main.c` (auth patches) | Apply auth patches to upstream `src/client/cl_main.c` |
| `sv_main.c` (auth) | Apply to upstream `src/server/sv_main.c` |
| `sv_client.c` (auth + demo) | Apply to upstream `src/server/sv_client.c` |
| `qcommon.h` (USE_AUTH defs) | Apply to upstream `src/qcommon/qcommon.h` |
| Makefile `USE_AUTH` flags | Port to `cmake/URT.cmake` |

---

## 6. PBR-from-BSP: Auto-Generate PBR Data for Legacy Maps

This is the motivating feature of this rebase. URT maps were made without
PBR assets (no `_n` normal maps, no `_orm` physical maps). The goal is to
make these maps look correct and visually useful with the upstream Vulkan PBR
renderer, without requiring map authors to re-export assets.

### 6.1 What "usable PBR data" means for URT maps

The upstream Vulkan PBR pipeline needs, per surface:
1. **Base color / albedo** -- already present (diffuse texture)
2. **Normal map** (`_n` / `_nh`) -- MISSING on URT maps
3. **Physical map** (ORM / RMO packed: AO, Roughness, Metalness) -- MISSING
4. **Emissive map** (`_e`) -- optional, rarely present

We can generate reasonable approximations for (2) and (3) from existing data:

| PBR channel | Source in BSP | Generation method |
|---|---|---|
| Normal map (XYZ) | Diffuse texture | Sobel/emboss height-to-normal (already in `tr_image.c`) |
| AO (R of ORM) | BSP lightmap | Lightmap luminance = baked AO per texel |
| Roughness (G of ORM) | Diffuse texture | Inverse saturation heuristic: desaturated = metallic/smooth; saturated = rough dielectric |
| Metalness (B of ORM) | Diffuse texture | Near-zero default (0.04); override for detected metallic surfaces |

### 6.2 What already exists in our repo

In `code/renderer2/tr_image.c`:
- `RGBAtoNormal(pic, out, w, h, clampToEdge)` -- Sobel normal-from-height
- `IMGFLAG_GENNORMALMAP` -- flag that triggers `RGBAtoNormal` when loading diffuse
- `r_genNormalMaps` cvar (default 0) -- enables the flag during `ParseStage`

**Gap**: `r_genNormalMaps` is disabled by default and is not linked to `r_pbr`.
There is no ORM generation (only normal map generation).
There is no lightmap-to-AO extraction.

### 6.3 What exists in the upstream renderer

In `src/renderers/vulkan/tr_shader.c`:
- Full ORM/RMO/MOXR auto-discovery loop (tries all suffix variants on each shader stage)
- `r_pbr_packedPreferred` cvar to prefer a specific layout
- `stage->physicalMap`, `stage->normalMap`, `stage->physicalMapType`, `stage->normalMapType`
- `vk_create_phyisical_texture()`, `vk_create_normal_texture()` loaders

In `src/renderers/vulkan/tr_bsp.c`:
- `GenerateNormals(face)` -- generates per-vertex normals from BSP geometry
- `vk.pbrActive` flag -- gates PBR-specific codepaths

In `src/navigation/nav_bsp_extract.c`:
- `Nav_BSP_AddVertex`, `Nav_BSP_AddTriangle` -- walks BSP faces into a triangle buffer
  (intended for navmesh, reusable for AO baking)

### 6.4 The PBR-from-BSP generation system: design

Add a new file `src/renderers/vulkan/vk_pbr_autogen.c` (+ `.h`) implementing:

#### 6.4.1 Normal map generation from diffuse

This is almost the same as the existing `r_genNormalMaps` path.

**Change in `tr_shader.c`**: In the PBR auto-discovery loop (around line 1826),
after failing to find any `_n`/`_nh` file, if `r_pbr_autoNormal->integer` is set:

```c
// No normal map file found -- generate one from diffuse
if (!stage->normalMap && physicalAlbedo && r_pbr_autoNormal->integer)
    stage->normalMap = VK_AutoGenNormalFromDiffuse(physicalAlbedoImg, normalFlags);
```

`VK_AutoGenNormalFromDiffuse(image_t *diffuse, imgFlags_t flags)` in `vk_pbr_autogen.c`:
1. Load the diffuse raw pixel data from disk via `R_LoadImage`
2. Run `RGBAtoNormal` (ported from renderer2's `tr_image.c`)
3. Create and cache the result as `<texname>_n` via `R_CreateImage`
4. Return the cached image

New cvar: `r_pbr_autoNormal "1"` -- auto-generate normal maps from diffuse when no
`_n`/`_nh` file is found. Default: `1` when `r_pbr 1`, `0` otherwise.

#### 6.4.2 ORM packed map generation from diffuse + lightmap

The ORM map encodes AO (R), Roughness (G), Metalness (B) in a single texture.

**AO channel**: Extract from the BSP lightmap at the same texel resolution.
The lightmap already contains baked AO (indirect shadows). The ORM texture must
be resampled to match the diffuse texture's UV layout. This is non-trivial; the
lightmap uses lightmap UVs while the diffuse uses surface UVs. Use bilinear
sampling at each diffuse texel's lightmap UV to extract the AO value.

**Roughness channel**: Derived from diffuse texture statistics.
- Compute per-texel saturation: `S = 1 - min(R,G,B)/max(R,G,B)` (HSV saturation)
- Compute luminance: `Y = 0.299R + 0.587G + 0.114B`
- Roughness heuristic: surfaces with high saturation and medium luminance are
  likely painted/organic (rough). Shiny metals are desaturated and bright.
  `roughness = clamp(0.4 + 0.6 * saturation - 0.3 * (luminance/255.0), 0.1, 1.0)`
- Default fallback if heuristic is disabled: `r_baseGloss` mapped to roughness.

**Metalness channel**: Most URT map surfaces are non-metallic (concrete, wood,
fabric, painted metal). Default metalness = 0.0. Optionally detect metallic
surfaces by checking desaturation + high luminance:
`metalness = (saturation < 0.1 && luminance > 180) ? 0.7 : 0.0`

**Implementation**:

`VK_AutoGenORMFromDiffuse(image_t *diffuse, lightmapData_t *lm, imgFlags_t flags)`
in `vk_pbr_autogen.c`:

1. Load diffuse raw pixels
2. If lightmap data pointer is provided, sample AO from lightmap UVs
3. Compute roughness and metalness per-texel using heuristics above
4. Pack into RGBA8 image: `{ R=AO, G=roughness, B=metalness, A=255 }`
5. Cache as `<texname>_orm` via `R_CreateImage`
6. Return the cached image

New cvar: `r_pbr_autoORM "1"` -- auto-generate ORM maps. Default `1` when `r_pbr 1`.
New cvar: `r_pbr_autoORM_roughness "0.6"` -- default roughness for surfaces without
detectable material type. Range 0.0-1.0.
New cvar: `r_pbr_autoORM_metalness "0.0"` -- default metalness. Range 0.0-1.0.
New cvar: `r_pbr_autoORM_useHeuristic "1"` -- enable the saturation/luminance
heuristic for per-texel roughness/metalness estimation.

#### 6.4.3 Lightmap UV access during shader stage PBR auto-gen

The challenge is that during `ParseStage` (called from `FinishShader`), the
lightmap data for a specific surface is not directly available; shader parsing
is per-shader-name, not per-surface instance.

**Solution**: Two-pass approach:
- Pass 1 (at map load time, `RE_LoadWorldMap`): For each BSP surface, call
  `VK_AutoGenORMFromDiffuse` with the surface's lightmap region. Cache results
  in the image hash table under the `_orm` key.
- Pass 2 (during shader stage auto-discovery): `vk_create_phyisical_texture` finds
  the `_orm` image already in cache.

This means the lightmap-derived AO generation happens as a post-load step in
`tr_bsp.c` after all surfaces are loaded, inside a new function:
`R_GeneratePBRMapsForWorld(world_t *world)`.

Pseudo-code in `tr_bsp.c`:

```c
void R_GeneratePBRMapsForWorld(world_t *world) {
    if (!vk.pbrActive || !r_pbr_autoORM->integer)
        return;
    for (i = 0; i < world->numShaders; i++) {
        dshader_t *ds = &world->shaders[i];
        image_t *diffuse = R_FindImageFile(ds->shader, ...);
        if (!diffuse) continue;
        // Check if _orm already exists (authored or previously generated)
        if (R_FindImageFile(va("%s_orm", stripped), ...)) continue;
        lightmapRegion_t *lm = R_GetLightmapForShader(ds->shader, world);
        VK_AutoGenORMFromDiffuse(diffuse, lm);
    }
    for (i = 0; i < world->numShaders; i++) {
        image_t *diffuse = R_FindImageFile(ds->shader, ...);
        if (!diffuse) continue;
        if (R_FindImageFile(va("%s_n", stripped), ...)) continue;
        VK_AutoGenNormalFromDiffuse(diffuse, normalFlags);
    }
}
```

Call `R_GeneratePBRMapsForWorld` at the end of `RE_LoadWorldMap`, before
`R_GenerateDrawSurfs`.

#### 6.4.4 New cvars summary

| Cvar | Default | Description |
|---|---|---|
| `r_pbr_autoNormal` | `1` | Auto-generate `_n` normal map from diffuse via Sobel filter when no file is found |
| `r_pbr_autoORM` | `1` | Auto-generate `_orm` physical map from diffuse + lightmap when no file is found |
| `r_pbr_autoORM_roughness` | `0.6` | Default roughness for auto-generated ORM (range 0.0-1.0) |
| `r_pbr_autoORM_metalness` | `0.0` | Default metalness for auto-generated ORM (range 0.0-1.0) |
| `r_pbr_autoORM_useHeuristic` | `1` | Enable saturation/luminance heuristic for per-texel roughness/metalness |
| `r_pbr_autoORM_aoFromLightmap` | `1` | Use BSP lightmap luminance as AO channel in generated ORM |

#### 6.4.5 New file: `src/renderers/vulkan/vk_pbr_autogen.c`

This file implements:

```c
// Public API
image_t *VK_AutoGenNormalFromDiffuse(image_t *diffuse, imgFlags_t flags);
image_t *VK_AutoGenORMFromDiffuse(image_t *diffuse,
                                   const byte *lightmapData, int lmW, int lmH,
                                   const float *lmUVs, int numVerts,
                                   imgFlags_t flags);

// Internal helpers (ported from renderer2/tr_image.c)
static void VKAG_RGBAtoNormal(const byte *in, byte *out, int w, int h, qboolean clamp);
static void VKAG_RGBAtoORM(const byte *in, byte *out, int w, int h,
                             const byte *lmData, int lmW, int lmH);
```

#### 6.4.6 New file header: `src/renderers/vulkan/vk_pbr_autogen.h`

```c
#pragma once
#include "../common/tr_common.h"
image_t *VK_AutoGenNormalFromDiffuse(image_t *diffuse, imgFlags_t flags);
image_t *VK_AutoGenORMFromDiffuse(image_t *diffuse,
                                   const byte *lmData, int lmW, int lmH,
                                   const float *lmUVs, int numVerts,
                                   imgFlags_t flags);
void     R_GeneratePBRMapsForWorld(world_t *world);
```

### 6.5 Interaction with upstream PBR auto-discovery

The upstream auto-discovery loop in `tr_shader.c` already tries all ORM/RMO/etc.
suffixes. With `r_pbr_autoORM 1`, the `_orm` images are pre-generated into the image
cache by `R_GeneratePBRMapsForWorld` before shader parsing runs for map surfaces.
The existing discovery loop will then find them transparently -- no changes needed
to the discovery loop itself.

Similarly for `_n`: the upstream already tries `_n`/`_nh`. With `r_pbr_autoNormal 1`,
the `_n` image is pre-generated and found by the existing loop.

This means the PBR autogen system is **additive and non-invasive**: it only adds
pre-generated textures to the cache; it does not change the shader parsing logic.

### 6.6 Quality vs. correctness

The auto-generated data is an approximation. For best results, map authors should
supply authored `_n` and `_orm` textures. The auto-gen is a quality-of-life feature
for existing maps. Add a startup message when auto-gen is active:

```
PBR Auto-generation active (r_pbr_autoNormal 1, r_pbr_autoORM 1).
For best visual quality, supply _n and _orm textures per PBR_TEXTURES.md.
```

---

## 7. Phase-by-Phase Implementation Plan

### Phase 1: Build System Migration

**Branch**: `urt/phase1-build`  
**Agent(s)**: 1  
**Files**: `CMakeLists.txt`, `cmake/URT.cmake`, `cmake/FindOpenAL.cmake`

**Tasks**:
- [ ] Fork from upstream HEAD as branch `urt/next-gen-5-rebase`
- [ ] Add `cmake/URT.cmake` with all URT compile definitions:
  `USE_AUTH`, `USE_SERVER_DEMO`, `USE_URT_DEMO`, `PRODUCT_NAME=quake3e-urt`
- [ ] Add URT option flags to root `CMakeLists.txt` (default ON):
  `option(USE_URT "Build Urban Terror engine extensions" ON)`
- [ ] Wire `cmake/URT.cmake` into the build when `USE_URT=ON`
- [ ] Verify the upstream build compiles clean on Linux/macOS/Windows
- [ ] Update CI (`.github/workflows/build.yml`, `release.yml`) for CMake

**Deliverable**: Clean upstream build + CI green.

---

### Phase 2: URT Auth Server

**Branch**: `urt/phase2-auth`  
**Agent(s)**: 1  
**Files**: `src/server/sv_urt_auth.c`, `src/server/sv_urt_auth.h`,
`src/server/sv_main.c`, `src/server/sv_client.c`,
`src/client/cl_main.c`, `src/qcommon/qcommon.h`

**Tasks**:
- [ ] Create `src/server/sv_urt_auth.c` / `sv_urt_auth.h`
  implementing `SV_AuthPacket`, `SV_SendAuthRequest`, `SV_AuthCheckClient`
- [ ] Define `AUTHORIZE_SERVER_NAME "authorize.urbanterror.info"` in `qcommon.h`
  (separate from the URT auth server -- see stored memory note)
- [ ] Add `authserver.urbanterror.info` domain handling to `sv_main.c`
  (registered on `SV_Init`, called on `OOB AUTH:SV` packet)
- [ ] Add `CL_AuthPacket` handler to `cl_main.c` for `AUTH:CL` packets
  (these come from `authserver.urbanterror.info`, NOT from `cls.authorizeServer`)
- [ ] Gate all auth code behind `#ifdef USE_AUTH`
- [ ] Add `sv_authserver` cvar (default `"authserver.urbanterror.info"`)
- [ ] Ensure AUTH handshake works over DTLS if `net_dtls 1`

**Deliverable**: Client connects to URT auth server; auth denial kicks player.

---

### Phase 3: Server -- Rankings, Demo, TLD

**Branch**: `urt/phase3-server`  
**Agent(s)**: 1  
**Files**: `src/server/sv_urt_rankings.c`, `src/server/sv_urt_demo.c`,
`src/server/sv_client.c`, `src/server/tlds.h`

**Tasks**:
- [ ] Add `src/server/sv_urt_rankings.c` (copy + update includes)
- [ ] Add `src/server/sv_urt_demo.c` for `USE_SERVER_DEMO` / `USE_URT_DEMO`
- [ ] Copy `tlds.h` to `src/server/tlds.h`
- [ ] Gate URT-specific server code behind `#ifdef USE_URT`

**Deliverable**: URT rankings and demo recording functional.

---

### Phase 4: Renderer Compatibility

**Branch**: `urt/phase4-renderer-compat`  
**Agent(s)**: 1  
**Files**: `src/renderers/vulkan/tr_shader.c`, `src/renderers/vulkan/tr_init.c`,
`src/renderers/common/tr_image*.c`

**Tasks**:
- [ ] Verify the upstream shader parser handles all shader keywords used in URT
  `.shader` files (Q3 standard directives: `map`, `clampmap`, `blendfunc`, `rgbGen`,
  `tcMod`, `surfaceparm`, etc.)
- [ ] Map legacy Q3/URT shader keywords to upstream equivalents if renamed
- [ ] Add cvar aliases for any renamed cvars our URT configs reference:
  e.g., if `r_baseGloss` was renamed, add an alias so existing configs work
- [ ] Ensure `q3ut4/` pak filesystem is correctly mounted
- [ ] Test URT map and character shader loading with the Vulkan renderer
- [ ] Verify `r_pbr 0` fallback (OpenGL path) renders URT maps correctly

**Deliverable**: URT maps and characters render in Vulkan renderer without shader errors.

---

### Phase 5: PBR-from-BSP Auto-Generation

**Branch**: `urt/phase5-pbr-from-bsp`  
**Agent(s)**: 1-2  
**Files** (all new or modified):

| File | Type | Description |
|---|---|---|
| `src/renderers/vulkan/vk_pbr_autogen.c` | NEW | Auto-generation implementation |
| `src/renderers/vulkan/vk_pbr_autogen.h` | NEW | Public API |
| `src/renderers/vulkan/tr_bsp.c` | MODIFIED | Call `R_GeneratePBRMapsForWorld` |
| `src/renderers/vulkan/tr_init.c` | MODIFIED | Register new cvars |
| `src/renderers/vulkan/tr_local.h` | MODIFIED | Declare new cvars extern |

**Tasks**:

**5a. Port `RGBAtoNormal` from renderer2**
- [ ] Copy `RGBAtoNormal`, `RGBAtoYCoCgA`, `YCoCgAtoRGBA` from our `tr_image.c`
  into `vk_pbr_autogen.c` with prefix `VKAG_` (to avoid link conflicts)
- [ ] Write `VK_AutoGenNormalFromDiffuse(image_t*, imgFlags_t)`:
  load raw pixels, call `VKAG_RGBAtoNormal`, cache as `<name>_n`, return image

**5b. Implement ORM generation**
- [ ] Write `VKAG_RGBAtoORM(pic, out, w, h, lmData, lmW, lmH)`:
  - R channel: bilinear-sample lightmap luminance at each texel's lightmap UV
  - G channel: roughness heuristic (`0.4 + 0.6*saturation - 0.3*(luma/255)`)
  - B channel: metalness heuristic (`saturation < 0.1 && luma > 180 ? 0.7 : 0.0`)
  - If `r_pbr_autoORM_useHeuristic 0`, use `r_pbr_autoORM_roughness` and
    `r_pbr_autoORM_metalness` as flat values
  - If `r_pbr_autoORM_aoFromLightmap 0`, set A=255 (no AO, fully lit)
- [ ] Write `VK_AutoGenORMFromDiffuse(image_t*, lmData, lmW, lmH, lmUVs, numVerts, imgFlags_t)`
- [ ] Handle the case where lightmap data is not available (entity surfaces, 2D):
  use `A=255, G=r_pbr_autoORM_roughness, B=r_pbr_autoORM_metalness`

**5c. World-load integration in `tr_bsp.c`**
- [ ] Add `R_GeneratePBRMapsForWorld(world_t *world)` at end of `RE_LoadWorldMap`
- [ ] For each world shader, check if `_n` and `_orm` already exist in cache;
  if not, call the appropriate autogen function
- [ ] Access lightmap data: use `tr.lightmaps[lmIndex]` pixel data OR re-extract
  from the BSP raw lightmap lump (stored in `s_worldData`)

**5d. Register cvars in `tr_init.c`**
- [ ] `r_pbr_autoNormal` (default `"1"`, CVAR_ARCHIVE | CVAR_LATCH)
- [ ] `r_pbr_autoORM` (default `"1"`, CVAR_ARCHIVE | CVAR_LATCH)
- [ ] `r_pbr_autoORM_roughness` (default `"0.6"`, CVAR_ARCHIVE | CVAR_LATCH)
- [ ] `r_pbr_autoORM_metalness` (default `"0.0"`, CVAR_ARCHIVE | CVAR_LATCH)
- [ ] `r_pbr_autoORM_useHeuristic` (default `"1"`, CVAR_ARCHIVE | CVAR_LATCH)
- [ ] `r_pbr_autoORM_aoFromLightmap` (default `"1"`, CVAR_ARCHIVE | CVAR_LATCH)

**5e. Documentation**
- [ ] Add a section to `docs/PBR_TEXTURES.md` describing the auto-gen system
- [ ] Document the fallback chain: authored assets -> auto-gen -> flat defaults

**Deliverable**: URT maps load in PBR mode with plausible normal and ORM maps
auto-generated from their diffuse textures and lightmaps. Visual quality acceptable
for competitive play; no authored asset changes required.

---

### Phase 6: Validation and CI

**Branch**: `urt/phase6-validation`  
**Agent(s)**: 1  

**Tasks**:
- [ ] Load `ut4_abbey` (representative URT map) with `r_pbr 1` -- verify no shader errors
- [ ] Verify auth: client connects, auth handshake completes (mock auth server OK)
- [ ] Run full build on Linux (GCC 15+, Clang 18+), macOS, Windows
- [ ] Run smoke tests (`scripts/smoke_test.sh` from upstream)
- [ ] Update CI workflows for the new CMake build
- [ ] Ensure no UTF/non-ASCII characters in any new `.c`/`.h` files (house rule)

---

## 8. Agent Assignment Matrix

| Phase | Work | Recommended agent specialty |
|---|---|---|
| 1 | Build system | CMake, CI/CD |
| 2 | URT auth | Networking, C server code |
| 3 | Rankings + demo | Server I/O |
| 4 | Renderer compat | Shader systems, Q3 formats |
| 5a | Normal map gen | Image processing, GLSL |
| 5b | ORM gen | Image processing, PBR theory |
| 5c | BSP integration | BSP format, renderer internals |
| 5d | Cvar wiring | Engine plumbing |
| 6 | Validation | QA, integration testing |

Phases 1-3 can run largely in parallel after Phase 1 produces a compilable base.
Phase 4 should complete before Phase 5 starts (renderer must load URT shaders first).
Phase 5 sub-tasks (5a-5d) can be split across agents in the same branch.
Phase 6 runs last.

---

## 9. Build System Migration

### 9.1 CMake option structure

```cmake
# In root CMakeLists.txt, after upstream options:
option(USE_URT        "Build Urban Terror engine extensions" ON)
option(USE_AUTH       "Enable URT auth server support"      ON)
option(USE_SERVER_DEMO "Enable server-side demo recording"  ON)
option(USE_URT_DEMO   "Enable URT demo format extension"    ON)

if(USE_URT)
    include(cmake/URT.cmake)
endif()
```

### 9.2 `cmake/URT.cmake` contents

```cmake
message(STATUS "Building with Urban Terror extensions")

target_compile_definitions(idtech3_engine PRIVATE
    USE_URT=1
    PRODUCT_NAME="quake3e-urt"
)

if(USE_AUTH)
    target_compile_definitions(idtech3_engine PRIVATE USE_AUTH=1)
    target_sources(idtech3_engine PRIVATE
        src/server/sv_urt_auth.c
    )
endif()

if(USE_SERVER_DEMO)
    target_compile_definitions(idtech3_engine PRIVATE USE_SERVER_DEMO=1)
endif()

if(USE_URT_DEMO)
    target_compile_definitions(idtech3_engine PRIVATE USE_URT_DEMO=1)
endif()

# URT server extensions
target_sources(idtech3_engine PRIVATE
    src/server/sv_urt_rankings.c
    src/server/sv_urt_demo.c
)

# PBR auto-generation for legacy maps
target_sources(idtech3_engine PRIVATE
    src/renderers/vulkan/vk_pbr_autogen.c
)
```

### 9.3 Retained compatibility

The upstream `Makefile` does not exist; all builds go through CMake.
Our existing CI scripts that call `make` must be updated to call
`cmake --build build/` instead.

---

## 10. Testing & Validation Checklist

### Renderer

- [ ] `r_pbr 0` (OpenGL fallback): URT map renders, no corruption
- [ ] `r_pbr 1` + authored assets: PBR shading correct on test surface
- [ ] `r_pbr 1` + no assets + `r_pbr_autoNormal 1`: normal map auto-generated,
  visible surface detail
- [ ] `r_pbr 1` + no assets + `r_pbr_autoORM 1`: ORM map auto-generated,
  reasonable roughness and AO visible
- [ ] `r_pbr_autoORM_useHeuristic 0`: flat roughness/metalness from cvar defaults
- [ ] `r_pbr_autoORM_aoFromLightmap 0`: AO channel is 1.0 (white)
- [ ] Map load time with auto-gen enabled: must not exceed +2 seconds vs. no auto-gen
- [ ] No `_n` or `_orm` image files written to disk (all in memory cache only)

### Server

- [ ] `USE_AUTH`: client is accepted after successful auth handshake
- [ ] `USE_AUTH`: client is kicked on auth denial
- [ ] `USE_SERVER_DEMO`: server demo file created, playable
- [ ] Rankings: kill/death increments correctly

### Build

- [ ] Linux (GCC 15+): compiles with zero errors
- [ ] Linux (Clang 18+): compiles with zero errors
- [ ] macOS (Apple Silicon): compiles
- [ ] Windows (MSVC): compiles
- [ ] No UTF/non-ASCII characters in any new `.c`/`.h` file

---

## 11. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Upstream shader parser rejects URT `.shader` keywords | Medium | High | Audit all URT shader files against upstream parser before Phase 5 (Phase 4) |
| URT auth UDP packets break over DTLS | Low | High | Test auth handshake with DTLS enabled/disabled separately |
| ORM heuristic produces incorrect metalness for colourful textures | High | Low | Clamp metalness to 0 by default; heuristic only for desaturated surfaces |
| Memory overhead of auto-generated maps | Medium | Medium | Auto-gen images share the normal image cache; size is proportional to texture count -- benchmark on `ut4_abbey` |
| CMake migration breaks mod DLL ABI | Low | High | Verify `cgame`/`game`/`ui` DLL entrypoints are unchanged |
| Upstream game code (`g_director.c` etc.) conflicts with URT `game/` | Low | Low | URT uses its own QVM; upstream game code is in `src/game/` but not compiled into the QVM |

---

## Appendix A: Key Files Quick Reference

### URT auth -- key source locations in this repo

| What | File | Symbol |
|---|---|---|
| Auth server domain | `code/qcommon/qcommon.h` | `AUTHORIZE_SERVER_NAME` |
| Auth packet handler (client) | `code/client/cl_main.c` | `CL_AuthPacket` |
| Auth request (server) | `code/server/sv_main.c` | `SV_AuthPacket` |
| Auth server cvar | `code/server/sv_main.c` | `sv_authserver` |

### PBR autogen -- new symbols (Phase 6)

| What | New file | Symbol |
|---|---|---|
| Normal from diffuse | `vk_pbr_autogen.c` | `VK_AutoGenNormalFromDiffuse` |
| ORM from diffuse+lm | `vk_pbr_autogen.c` | `VK_AutoGenORMFromDiffuse` |
| World post-load entry | `tr_bsp.c` | `R_GeneratePBRMapsForWorld` |
| Auto-normal cvar | `tr_init.c` | `r_pbr_autoNormal` |
| Auto-ORM cvar | `tr_init.c` | `r_pbr_autoORM` |

### Stored conventions (from memory)

- **No UTF/non-ASCII chars** in `.c`/`.h` files. Use `--` for em dash, `*` for
  bullet, `->` for arrow.
- Auth server is `authserver.urbanterror.info`; `AUTH:CL` packets come from there,
  NOT from `cls.authorizeServer` (which is `authorize.urbanterror.info`).
