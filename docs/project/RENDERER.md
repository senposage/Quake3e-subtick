# Renderer — OpenGL and Vulkan

> Covers `code/renderer/` (OpenGL), `code/renderervk/` (Vulkan), and `code/renderercommon/` (shared). The two renderers are separate codebases sharing the same public interface (`refexport_t`).

---

## Table of Contents

1. [Renderer Architecture](#renderer-architecture)
2. [Public Interface (tr_public.h)](#public-interface-tr_publich)
3. [Scene Graph & Rendering Pipeline](#scene-graph--rendering-pipeline)
4. [tr_init.c — Initialization](#tr_initc--initialization)
5. [tr_main.c — View Setup & Transforms](#tr_mainc--view-setup--transforms)
6. [tr_bsp.c — World/BSP Loading](#tr_bspc--worldbsp-loading)
7. [tr_shader.c — Shader System](#tr_shaderc--shader-system)
8. [tr_image.c — Texture Loading](#tr_imagec--texture-loading)
9. [tr_scene.c — Scene Submission](#tr_scenec--scene-submission)
10. [tr_surface.c / tr_shade.c — Surface Drawing](#tr_surfacec--tr_shadec--surface-drawing)
11. [tr_world.c — World Surface Culling](#tr_worldc--world-surface-culling)
12. [tr_light.c — Dynamic Lighting](#tr_lightc--dynamic-lighting)
13. [tr_model.c — Model System](#tr_modelc--model-system)
14. [tr_backend.c — Render Backend](#tr_backendc--render-backend)
15. [Vulkan Renderer Additions (renderervk/)](#vulkan-renderer-additions-renderervk)
16. [renderercommon/ — Shared Utilities](#renderercommon--shared-utilities)
17. [Key Data Structures](#key-data-structures)
18. [Key Render Cvars](#key-render-cvars)

---

## Renderer Architecture

```
CL_CGameRendering()
    └── VM_Call(cgvm, CG_DRAW_ACTIVE_FRAME, ...)
            ↓ cgame calls syscalls:
    CG_R_CLEARSCENE       → re.ClearScene()
    CG_R_ADDREFENTITY     → re.AddRefEntityToScene()
    CG_R_ADDLIGHTTOSCENE  → re.AddLightToScene()
    CG_R_RENDERSCENE      → re.RenderScene()
    CG_R_DRAWSTRETCHPIC   → re.DrawStretchPic()  ← 2D HUD
    ...

SCR_UpdateScreen()
    ├── re.BeginFrame(stereo)
    ├── [cgame draws its scene via above calls]
    └── re.EndFrame(&frontMsec, &backMsec)
              ↓
         Backend executes render command list
         (RB_ExecuteRenderCommands)
```

The renderer runs entirely on the main thread. `re` is the `refexport_t` function table filled by whichever renderer was loaded (GL or VK).

---

## Public Interface (tr_public.h)

**File:** `code/renderercommon/tr_public.h`

The `refexport_t` struct — every function the engine calls in the renderer:

```c
typedef struct {
    void (*Shutdown)(refShutdownCode_t code);
    void (*BeginRegistration)(glconfig_t *config);
    qhandle_t (*RegisterModel)(const char *name);
    qhandle_t (*RegisterSkin)(const char *name);
    qhandle_t (*RegisterShader)(const char *name);
    qhandle_t (*RegisterShaderNoMip)(const char *name);
    void (*LoadWorld)(const char *name);
    void (*SetWorldVisData)(const byte *vis);
    void (*EndRegistration)(void);
    void (*ClearScene)(void);
    void (*AddRefEntityToScene)(const refEntity_t *re, qboolean intShaderTime);
    void (*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
    int  (*LightForPoint)(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
    void (*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
    void (*AddAdditiveLightToScene)(...);
    void (*AddLinearLightToScene)(...);
    void (*RenderScene)(const refdef_t *fd);
    void (*SetColor)(const float *rgba);
    void (*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
    void (*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
    void (*UploadCinematic)(...);
    void (*BeginFrame)(stereoFrame_t stereoFrame);
    void (*EndFrame)(int *frontEndMsec, int *backEndMsec);
    int  (*MarkFragments)(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
    int  (*LerpTag)(orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName);
    void (*ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);
    // ... screenshot, take video frame, etc.
} refexport_t;
```

The `refimport_t` struct — functions the renderer calls back into the engine (mostly `Com_Printf`, `FS_*`, `Cvar_*`, `Cmd_*`, memory alloc).

---

## Scene Graph & Rendering Pipeline

Each frame follows this sequence:

```
re.ClearScene()                    — reset scene arrays
re.AddRefEntityToScene(...)        — add N models/sprites
re.AddPolyToScene(...)             — add polygons (decals, particles)
re.AddLightToScene(...)            — add N dynamic lights
re.RenderScene(refdef_t *)         — render the 3D world
re.DrawStretchPic(...)             — draw 2D HUD elements
re.EndFrame(...)                   — execute render commands
```

### Render Command List

The front-end (game thread) builds a list of render commands. The back-end (same thread in Q3) executes them:

| Command | Data |
|---|---|
| `RC_DRAW_SURFS` | drawSurf list, entity/light counts |
| `RC_DRAW_BUFFER` | which buffer to draw into |
| `RC_SWAP_BUFFERS` | present to screen |
| `RC_SCREENSHOT` | capture screenshot |
| `RC_VIDEOFRAME` | AVI frame capture |
| `RC_SET_COLOR` | 2D color state |
| `RC_STRETCH_PIC` | 2D blit command |

---

## tr_init.c — Initialization

**File:** `code/renderer/tr_init.c` (or `renderervk/tr_init.c` for Vulkan)  
**Size:** ~1895 lines

### R_Init() / RE_Init()

Called once when renderer is loaded:
```
R_Init:
  Cmd_AddCommand("screenshot", R_ScreenShot_f)
  Cmd_AddCommand("gfxinfo", GfxInfo)
  R_InitImages()       ← hash table for textures
  R_InitShaders()      ← load default shaders
  R_InitSkins()        ← parse skin files
  R_ModelInit()        ← model cache
  R_InitFreeType()     ← font rendering (FreeType2)
  InitOpenGL() (GL) / vk_initialize() (VK) ← device/context
  R_InitExtensions()   ← query GL/VK capabilities
```

### GL Context Setup (OpenGL renderer)

`InitOpenGL()`:
- Calls `GLimp_Init()` (platform-specific: win_glimp.c / linux_glimp.c / sdl_glimp.c)
- Queries `glGetString(GL_VERSION)` — stores in `glConfig`
- Checks for required extensions: `GL_ARB_multitexture`, `GL_ARB_texture_compression`, etc.
- Sets `glConfig.maxTextureSize`, `glConfig.maxActiveTextures`

### Vulkan Initialization (Vulkan renderer)

`vk_initialize()` (vk.c):
- Create `VkInstance`, `VkPhysicalDevice`, `VkDevice`
- Create swapchain, framebuffers, render pass
- Allocate command buffers (double-buffered: 2 sets)
- Create pipeline cache
- Pre-build all shader pipelines (`MAX_VK_PIPELINES` variants)

---

## tr_main.c — View Setup & Transforms

**File:** `code/renderer/tr_main.c`  
**Size:** ~1697 lines

### RE_RenderScene(refdef_t *fd) → R_RenderView()

The frame rendering entry point:
```
RE_RenderScene:
  R_SetupViewParmsFromRefdef(fd)   ← set viewParms from refdef
  R_SetupFrustum()                 ← compute 5 frustum planes
  R_SetupProjection()              ← compute projection matrix
  R_RenderView(viewParms)
    R_GenerateDrawSurfs()          ← build drawSurf list
      R_AddWorldSurfaces()         ← BSP world
      R_AddEntitySurfaces()        ← models
      R_AddPolygonSurfaces()       ← decals
    R_SortDrawSurfs()              ← sort by shader
    RB_RenderThread or direct RB_ExecuteRenderCommands()
```

### View Transforms

`R_RotateForEntity(ent, viewParms, or)`:
- Computes `or.modelMatrix` for entity (translation + rotation)
- Entity moves: `origin = ent->origin + ent->axis[0]*ent->frame_offset`

`R_SetupProjection(dest, zProj, computeFrustum)`:
- Sets perspective projection: `fovX`, `fovY` from `refdef->fov_x/y`
- `zNear = r_znear->value` (default 4.0)
- `R_SetFarClip()` — dynamically computes zFar from visible geometry

### Culling Functions

| Function | Test |
|---|---|
| `R_CullLocalBox(bounds)` | AABB vs frustum (uses signbits trick) |
| `R_CullPointAndRadius(pt, r)` | Sphere vs frustum |
| `R_CullLocalPointAndRadius(pt, r)` | Same but in model space |
| `R_CullDlight(dl)` | Dynamic light sphere vs frustum |

### Mirror / Portal Handling

`R_MirrorViewBySurface(surf, entityNum)`:
- Computes a mirror/portal view from a surface
- Recursively calls `R_RenderView` for the portal view
- Portal views are depth-limited to prevent infinite recursion

---

## tr_bsp.c — World/BSP Loading

**File:** `code/renderer/tr_bsp.c`  
**Size:** ~2272 lines

### R_LoadBsp(name) — World Loading

Called from `RE_LoadWorldMap`. Loads the rendering half of the BSP:
- `R_LoadShaders` → map all surface shaders
- `R_LoadPlanes` → plane array
- `R_LoadFogs` → fog volumes
- `R_LoadSurfaces` → all surface types:
  - Planar (brush faces)
  - Patch/bezier (curved surfaces)
  - Triangle mesh
  - Flares
- `R_LoadMarksurfaces` → per-leaf surface index
- `R_LoadSubmodels` → inline models
- `R_LoadVisibility` → PVS table (shared with CM)
- `R_LoadLightmaps` → baked lightmap textures
- `R_LoadEntities` → parse map entity lump (for spawn points, music, etc.)

### msurface_t — World Surface

```c
typedef struct msurface_s {
    int                 viewCount;    // if == tr.viewCount: already in drawSurf list
    shader_t            *shader;
    int                 fogIndex;
    surfaceType_t       surfaceType;  // SF_FACE, SF_GRID, SF_TRIANGLES, SF_FLARE
    int                 dlightBits;   // which dynamic lights affect this surface
    srfBspFace_t        *face;        // data for SF_FACE
    srfGridMesh_t       *grid;        // data for SF_GRID
    srfTriangles_t      *triangles;   // data for SF_TRIANGLES
} msurface_t;
```

---

## tr_shader.c — Shader System

**File:** `code/renderer/tr_shader.c`  
**Size:** ~3624 lines (largest in renderer)

### Shader Architecture

A Q3 "shader" is a material script, not a GLSL program. It defines how a surface looks over multiple rendering passes.

```
shader myShader {
    cull back
    q3map_surfacelight 500
    {
        map textures/common/detail.tga
        blendFunc GL_ZERO GL_SRC_COLOR
        rgbGen identity
    }
    {
        map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        tcGen lightmap
    }
}
```

### Key Structures

```c
typedef struct shader_s {
    char    name[MAX_QPATH];
    shaderStage_t *stages[MAX_SHADER_STAGES];  // rendering passes
    int     numStages;
    int     sort;         // rendering order (translucent after opaque)
    // ... fog, sky, surface flags, shader time, deforms ...
} shader_t;

typedef struct shaderStage_s {
    textureBundle_t bundle[NUM_TEXTURE_BUNDLES]; // textures + tcMod
    waveForm_t      rgbWave;   // color animation
    waveForm_t      alphaWave;
    colorGen_t      rgbGen;    // vertex, identity, wave, etc.
    alphaGen_t      alphaGen;
    unsigned        stateBits; // GL state: blend, depth test, cull
    stageRenderer_t *stageRendererFunc; // render function for this stage
} shaderStage_t;
```

### Shader Loading

`R_FindShader(name, lightmapIndex, mipRawImage)`:
1. Hash lookup in `tr.shaders[]`
2. If not found: try to load from `.shader` files in `scripts/`
3. If no script: create default shader from texture

### Shader Sorting

After all shaders are loaded, `R_SortNewShader` sorts them:
- `SS_ENVIRONMENT` (sky) < `SS_OPAQUE` (walls) < `SS_DECAL` < `SS_BLEND` (glass) < `SS_NEAREST` (sprites)

Lower sort value → rendered first.

---

## tr_image.c — Texture Loading

**File:** `code/renderer/tr_image.c`  
**Size:** ~1764 lines

### Image Loading Pipeline

```
R_FindImageFile(name, mipmap, picmip, glWrapClamp)
  → R_LoadImage(name, &pic, &width, &height)
      try .tga, .jpg, .png, .bmp, .pcx (in renderercommon/tr_image_*.c)
  → R_CreateImage(name, pic, width, height, imgType, imgFlags)
      mipmaps: GL_GenerateMipmap or manual box filter
      upload: glTexImage2D
      texture compression: glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5
```

### Image Cache

`tr.images[MAX_DRAWIMAGES]` — all loaded textures. Hash lookup by name.

`R_FindImageFile` — finds or loads image; returns `image_t*`.

`image_t` stores: `texnum` (GL texture ID), `width`, `height`, `imgFlags` (clamp/repeat/mipmap).

### Special Textures

| Name | Purpose |
|---|---|
| `*white` | Solid white (when no texture) |
| `*black` | Solid black |
| `*scratchimage` | Cinematic upload target |
| `*shadowmapimage` | Shadow map FBO target |
| `$lightmap` | BSP baked lighting |

---

## tr_scene.c — Scene Submission

**File:** `code/renderer/tr_scene.c`

### RE_ClearScene()

Resets per-frame scene arrays: `r_numentities=0`, `r_numdlights=0`, `r_numpolys=0`.

### RE_AddRefEntityToScene(refEntity_t *ent, intShaderTime)

Adds entity to `tr_refentities[]` ring buffer. Validates `ent->hModel`.

refEntity_t fields:
- `reType` (RT_MODEL, RT_SPRITE, RT_BEAM, RT_RAIL_CORE, RT_LIGHTNING, RT_PORTALSURFACE)
- `hModel` — model handle
- `origin`, `axis[3]` — position and orientation
- `frame`, `oldframe`, `backlerp` — animation
- `customShader`, `customSkin` — override surface shader
- `shaderTime` — time origin for shader animations
- `shaderRGBA[4]` — per-entity color tint

### RE_RenderScene(refdef_t *fd)

Kicks off the full rendering pipeline for one view (see tr_main.c).

---

## tr_surface.c / tr_shade.c — Surface Drawing

### tr_surface.c — Surface Dispatch

`RB_SurfaceBegin` dispatches by surface type:

| `surfaceType_t` | Render Function |
|---|---|
| `SF_FACE` | `RB_SurfaceFace()` |
| `SF_GRID` | `RB_SurfaceGrid()` (bezier patch) |
| `SF_TRIANGLES` | `RB_SurfaceTriangles()` |
| `SF_FLARE` | `RB_SurfaceFlare()` |
| `SF_MD3` | `RB_SurfaceMesh()` |
| `SF_IQM` | `RB_IQMSurfaceAnim()` |
| `SF_ENTITY` | `RB_SurfaceEntity()` |

### tr_shade.c — Per-Stage Rendering

`RB_BeginSurface(shader, fogNum)` sets up for a new shader.

`RB_RenderShaderStages()` executes each stage:
1. `RB_StageIteratorGeneric()` — most surfaces
2. `RB_StageIteratorVertexLitTexture()` — vertex-lit models
3. `RB_StageIteratorLightmappedMultitexture()` — lightmapped world surfaces (fast path)

### tr_shade_calc.c — Shader Animations

Evaluates per-frame shader expressions:
- `RB_DeformTessGeometry()` — wave deformations
- `RB_CalcColorFromEntity()` — entity color generation
- `RB_CalcColorFromOneMinusEntity()` — inverse entity color
- `RB_CalcAlphaFromEntity()` — alpha from entity
- `RB_CalcFogTexCoords()` — fog texture coordinates
- `RB_CalcScrollTexCoords()` — scroll texture animation
- `RB_CalcRotateTexCoords()` — rotating textures
- `RB_CalcTurbulentTexCoords()` — wave-distorted textures
- `RB_CalcEnvironmentTexCoords()` — spherical environment mapping
- `EvalWaveForm(w)` — evaluate sine/square/triangle/sawtooth/noise waves

---

## tr_world.c — World Surface Culling

**File:** `code/renderer/tr_world.c`

### R_AddWorldSurfaces()

Walks the BSP tree to collect visible surfaces:
```
R_MarkLeaves()              ← update visibility from current viewcluster
R_RecursiveWorldNode(root, planeBits, dlightBits):
    at each node: R_CullLocalBox vs frustum planes
    if culled: return
    if leaf: for each surface in leaf:
        R_AddWorldSurface(surf, dlightBits)
            if surf not already added (viewCount check):
                add to draw surface list
```

### Dynamic Lights

`R_RecursiveLightNode()` — separately walks BSP for each dlight to mark which surfaces it affects. `dlightBits` mask is propagated down the tree.

---

## tr_light.c — Dynamic Lighting

**File:** `code/renderer/tr_light.c`

### R_DlightBmodel(ent)

For brush model entities: check which dlights intersect the model's bounding box.

### R_LightForPoint(point, ambient, directed, lightDir)

Samples the pre-baked lightgrid at `point`. Returns:
- `ambient` — constant ambient light color
- `directed` — directional light color
- `lightDir` — direction toward main light source

Used by models to get lighting from the BSP lightgrid.

### R_SetupEntityLighting(def, ent)

Called for each rendered entity. Sets up `ent->ambientLight`, `ent->directedLight`, `ent->lightDir` from lightgrid and nearby dlights.

---

## tr_model.c — Model System

**File:** `code/renderer/tr_model.c`

Supports three model formats:

| Format | Extension | Description |
|---|---|---|
| MD3 | `.md3` | Vertex-animated (per-frame vertex positions) |
| MDC | `.mdc` | Compressed MD3 (used in RtCW / ET) |
| IQM | `.iqm` | Inter-Quake Model — skeletal animation |

`R_RegisterModel(name)`:
1. Hash lookup in `tr.models[]`
2. Detect format from file extension
3. Call appropriate loader
4. Return `qhandle_t` → index into `tr.models[]`

---

## tr_backend.c — Render Backend

**File:** `code/renderer/tr_backend.c`  
**Size:** ~1624 lines

### GL State Machine

The backend maintains a shadow of the GL state to minimize unnecessary API calls:

`GL_State(stateBits)`:
- Compare `stateBits` vs `glState.glStateBits`
- Only call `glEnable/glDisable/glBlendFunc/glDepthMask` for changed bits

`GL_Bind(image)`:
- Compare `image` vs `glState.currenttextures[unit]`
- Only call `glBindTexture` if changed

### RB_ExecuteRenderCommands(data)

Main backend loop. Executes commands queued by front-end:
```
while (cmd->commandId != RC_END_OF_LIST):
    switch cmd->commandId:
        RC_DRAW_BUFFER:    setup framebuffer
        RC_DRAW_SURFS:     RB_DrawSurfs → RB_RenderDrawSurfList
        RC_SET_COLOR:      set 2D color
        RC_STRETCH_PIC:    RB_StretchPic → glDrawElements
        RC_SWAP_BUFFERS:   GLimp_EndFrame (or vk_end_frame)
        RC_SCREENSHOT:     RB_TakeScreenshot
```

### VBO Management (tr_vbo.c)

`R_CreateVBO(verts, count)` — uploads vertex data to GPU memory.

`R_CreateIBO(indices, count)` — uploads index data.

World BSP surfaces are uploaded once at map load. Model surfaces are uploaded per-frame if dynamic.

---

## Vulkan Renderer Additions (renderervk/)

**Key differences from OpenGL renderer:**

| Aspect | OpenGL | Vulkan |
|---|---|---|
| Pipeline state | Implicit (GL state machine) | Explicit `VkPipeline` objects |
| Memory management | Driver handles | Manual allocation (vk_alloc_vbo/ibo) |
| Command recording | Immediate | Pre-recorded `VkCommandBuffer` |
| Sync | Implicit | Explicit semaphores/fences |
| Swap chain | `wglSwapBuffers`/`glXSwapBuffers` | `vkQueuePresentKHR` |

### vk.c — Core Vulkan

`vk_initialize()`:
- Instance + device + surface setup
- Swapchain: `VK_PRESENT_MODE_IMMEDIATE_KHR` (no vsync) or `FIFO` (vsync)
- Renderpass with depth/stencil
- `NUM_COMMAND_BUFFERS = 2` — double buffered

`vk_begin_frame()` / `vk_end_frame()`:
- Acquire swapchain image
- `vkBeginCommandBuffer` → draw calls → `vkEndCommandBuffer`
- Submit to queue → `vkQueuePresentKHR`

### Pre-built Pipeline Variants

Vulkan requires a distinct `VkPipeline` for each shader stage type + state combination. The engine pre-builds all `MAX_VK_PIPELINES` variants at startup. Types include: `TYPE_SIGNLE_TEXTURE` (sic — typo in the original source), `TYPE_MULTI_TEXTURE_MUL2`, `TYPE_MULTI_TEXTURE_ADD2`, etc. — one per texture blend mode.

---

## renderercommon/ — Shared Utilities

### tr_public.h / tr_types.h

The shared types used by both renderers and the game:
- `refEntity_t` — entity to render
- `refdef_t` — view definition
- `polyVert_t` — polygon vertex
- `glconfig_t` — GL/VK configuration (screen size, extension flags)

### tr_font.c

FreeType2-based font rendering:
- `RE_RegisterFont(fontName, pointSize, fontInfo)` — load font, build glyph atlas
- `RE_GetGlyphInfo(fontHandle, glyph)` — get UV coords + metrics for a glyph

### tr_noise.c

`R_NoiseGet4f(x, y, z, t)` — 4D procedural noise used in shader `tcMod` and deform effects.

### tr_image_*.c — Image Loaders

| File | Format | Notes |
|---|---|---|
| `tr_image_tga.c` | TGA | Primary format for game textures |
| `tr_image_jpg.c` | JPEG | Screenshots, some textures |
| `tr_image_png.c` | PNG | Modern texture format |
| `tr_image_pcx.c` | PCX | Legacy format (status bar icons) |
| `tr_image_bmp.c` | BMP | Rarely used |

---

## Key Data Structures

### trGlobals_t tr — Renderer Global State

```c
typedef struct {
    // per-frame state
    viewParms_t   viewParms;       // current view transforms
    trRefdef_t    refdef;          // current scene definition
    orientationr_t or;             // current entity transform
    trRefEntity_t *currentEntity;

    // world
    world_t       *world;          // loaded BSP render data
    int           viewCluster;     // current PVS cluster

    // assets
    model_t       *models[MAX_MOD_KNOWN];
    image_t       *images[MAX_DRAWIMAGES];
    shader_t      *shaders[MAX_SHADERS];
    shader_t      *sortedShaders[MAX_SHADERS];
    skin_t        *skins[MAX_SKINS];

    // lighting
    image_t       **lightmaps;
    vec3_t        sunLight, sunDirection;

    // look-up tables for shader animations
    float         sinTable[FUNCTABLE_SIZE];
    float         squareTable[FUNCTABLE_SIZE];
    float         triangleTable[FUNCTABLE_SIZE];
    float         sawToothTable[FUNCTABLE_SIZE];
} trGlobals_t;
```

### refEntity_t — Entity to Render

```c
typedef struct {
    refEntityType_t reType;          // model, sprite, beam, etc.
    int             renderfx;        // RF_NOSHADOW, RF_LIGHTING_ORIGIN, etc.
    qhandle_t       hModel;
    vec3_t          lightingOrigin;  // for shadows and lighting
    float           shadowPlane;
    vec3_t          axis[3];         // rotation matrix
    qboolean        nonNormalizedAxes;
    vec3_t          origin;          // world position
    int             frame;           // model frame
    int             oldframe;
    float           backlerp;        // 0 = at frame, 1 = at oldframe
    vec3_t          oldorigin;       // for lerping
    int             skinNum;
    qhandle_t       customSkin;
    qhandle_t       customShader;
    byte            shaderRGBA[4];   // color tint
    float           shaderTexCoord[2];
    float           shaderTime;      // shader clock offset
    int             reType;
} refEntity_t;
```

---

## Key Render Cvars

| Cvar | Default | Notes |
|---|---|---|
| `r_renderAPI` | 1 | 0=OpenGL, 1=Vulkan |
| `r_fullscreen` | 0 | Fullscreen mode |
| `r_mode` | -2 | Resolution mode (-2=custom) |
| `r_width/r_height` | 0/0 | Custom resolution |
| `r_allowSoftwareGL` | 0 | Allow software renderer fallback |
| `r_picmip` | 1 | Mip level bias (0=full res) |
| `r_textureMode` | "GL_LINEAR_MIPMAP_NEAREST" | Texture filter |
| `r_overbrightBits` | 1 | Overbright lighting scale |
| `r_gamma` | 1.0 | Gamma correction |
| `r_lodbias` | 0 | LOD bias for models |
| `r_lodscale` | 5 | LOD scale distance |
| `r_znear` | 4 | Near clip plane |
| `r_speeds` | 0 | Show renderer performance counters |
| `r_drawentities` | 1 | Render entities |
| `r_drawworld` | 1 | Render BSP world |
| `r_skipBackEnd` | 0 | Skip backend execution (profiling) |
| `r_fastsky` | 0 | Simple colored sky (no shader) |
| `r_noportals` | 0 | Disable portal rendering |
| `r_smp` | 0 | SMP rendering thread (experimental) |
| `r_vertexLight` | 0 | Use vertex lighting instead of lightmaps |
