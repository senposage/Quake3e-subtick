# Collision — BSP Maps, Traces, and Area Portals

> Covers `code/qcommon/cm_load.c`, `cm_load_bsp1.c`, `cm_load_bsp2.c`, `cm_trace.c`, `cm_patch.c`, `cm_polylib.c`, `cm_test.c`, and the public interface `cm_public.h`.

---

## Table of Contents

1. [Overview](#overview)
2. [BSP Structure](#bsp-structure)
3. [cm_load.c — Map Loading](#cm_loadc--map-loading)
4. [cm_trace.c — Ray/Box/Capsule Traces](#cm_tracec--rayboxcapsule-traces)
5. [cm_patch.c — Curved Surface Collision](#cm_patchc--curved-surface-collision)
6. [cm_test.c — Point Tests and Area Portals](#cm_testc--point-tests-and-area-portals)
7. [Public API (cm_public.h)](#public-api-cm_publich)
8. [Key Data Structures](#key-data-structures)
9. [Content Flags Reference](#content-flags-reference)

---

## Overview

The CM (Collision Map) subsystem loads the BSP file and provides two fundamental services:

1. **Traces** — given a start point, end point, and bounding box/capsule, find the first solid surface hit
2. **Point tests** — given a point, determine what content type it's in (solid, water, lava, etc.)

Everything collision-related runs through CM. The QVM game module calls `trap_Trace` → `sv_game.c:SV_GameSystemCalls(G_TRACE)` → `sv_world.c:SV_Trace` → `CM_BoxTrace`. The antilag system (`sv_antilag.c`) intercepts the `G_TRACE` syscall before this to rewind entity positions.

---

## BSP Structure

A Quake 3 BSP file contains:
- **Planes** — half-spaces that define all surfaces
- **Nodes** — binary tree nodes (BSP tree), each splitting space by a plane
- **Leafs** — terminal BSP nodes, contain lists of brushes and surfaces
- **Brushes** — convex solid volumes, each defined by a set of planes
- **Brush sides** — each plane of a brush with a surface type (shader)
- **Patches** — curved bezier surfaces for non-planar geometry
- **Models** — sub-models (doors, platforms); model 0 = world

### BSP Tree

```
cNode_t {
    cplane_t *plane;
    int children[2];   // positive = node index, negative = ~leaf index
}
```

Tree traversal: at each node, test the query against `plane`. Go into the child that the query touches (or both if it straddles). Stop at leaves.

### Visibility (PVS)

`cm.visibility` = precomputed potentially-visible-set. `CM_ClusterPVS(cluster)` returns a byte bitmask. If bit N is set, cluster N is potentially visible from `cluster`.

Used by `SV_BuildClientSnapshot` / `SV_AddEntitiesVisibleFromPoint` to cull entities from snapshots.

---

## cm_load.c — Map Loading

**File:** `code/qcommon/cm_load.c`  
**Also:** `cm_load_bsp1.c` (Quake BSP format support), `cm_load_bsp2.c` (extended BSP2 format)

### CM_LoadMap(name, clientload, checksum)

The main entry point. Called by:
- `SV_SpawnServer` (server always loads)
- `CL_CM_LoadMap` (client loads for collision — QVM also needs it for movement)

```
CM_LoadMap:
  if (already loaded and clientload): return cached
  FS_ReadFile(name, &buf)
  validate BSP header + version
  CM_ClearMap()
  CMod_LoadShaders()     ← content/surface flags for each face type
  CMod_LoadLeafs()       ← leaf array
  CMod_LoadPlanes()      ← plane array (normal + dist)
  CMod_LoadBrushSides()  ← brush side → plane + shader index
  CMod_LoadBrushes()     ← brush → brush sides list + bounds
  CMod_LoadLeafBrushes() ← leaf → brush index list
  CMod_LoadLeafSurfaces() ← leaf → surface index list
  CMod_LoadNodes()       ← BSP tree nodes
  CMod_LoadSubmodels()   ← inline model list (doors, etc.)
  CMod_LoadVisibility()  ← PVS data
  CMod_LoadEntityString() ← entities (for spawn points etc.)
  CMod_LoadPatches()     ← generate patch collision data
  CM_InitBoxHull()       ← create box/capsule hull models
  CM_FloodAreaConnections() ← area connectivity graph
  *checksum = CM_Checksum(header) ← for sv_pure verification
```

### Inline Models (Brush Models)

`CM_InlineModel(index)` → `clipHandle_t`:
- Index 0 = world geometry
- Index 1-N = inline brush models (doors, platforms, gibs)
- `BOX_MODEL_HANDLE` (255) = dynamic box trace (re-generated each call via `CM_TempBoxModel`)
- `CAPSULE_MODEL_HANDLE` (254) = capsule trace handle

### Brush Loading

Each `cbrush_t`:
```c
typedef struct {
    int             checkcount;   // avoids redundant trace tests
    int             contents;     // CONTENTS_* flags
    vec3_t          bounds[2];    // pre-computed AABB
    int             numsides;
    cbrushside_t    *sides;
} cbrush_t;
```

The `checkcount` trick: a global counter is incremented per trace. Each brush marks itself `checkcount = trace_checkcount` when first hit. If brush is found again in the same trace (via multiple leaf refs), it's skipped — prevents duplicate work.

---

## cm_trace.c — Ray/Box/Capsule Traces

**File:** `code/qcommon/cm_trace.c`  
**Size:** ~1468 lines — the most performance-critical file

### Entry Points

```c
void CM_BoxTrace(trace_t *results, start, end, mins, maxs, model, brushmask, capsule)
void CM_TransformedBoxTrace(...)  // same but with origin/angles transform
```

`capsule=qtrue` uses a capsule (cylinder + hemispherical caps) instead of box. Used for player collision to avoid corner-catching.

### Internal Trace Machinery

The trace uses a `traceWork_t` context:
```c
typedef struct {
    vec3_t      start, end;
    vec3_t      size[2];        // mins, maxs
    vec3_t      offsets[8];     // box corner offsets for plane tests
    float       maxOffset;      // max size component for sphere test
    vec3_t      extents;        // for cylinder check
    vec3_t      delta;          // end-start
    float       fraction;       // 0-1, best fraction found
    vec3_t      endpos;         // interpolated end position
    trace_t     trace;          // output
    int         contents;       // brushmask
    qboolean    isPoint;        // no extent, faster path
    cbrush_t    *skipBrush;     // don't test this brush (e.g. passed-through)
} traceWork_t;
```

### Trace Algorithm

```
CM_Trace (internal):
  1. CM_PositionTest: check if start is already stuck inside a solid
  2. CM_TraceThroughTree (BSP descent):
     - recurse down BSP tree from root
     - at each node: split ray by node's plane
       - if entirely in front: recurse front only
       - if entirely behind: recurse back only
       - if straddle: recurse both, front first
     - at leaf: CM_TraceThroughLeaf
       - for each brush in leaf: CM_TraceThroughBrush
       - for each patch in leaf: CM_TraceThroughPatch
```

### CM_TraceThroughBrush

The inner loop. For a convex brush:
1. For each plane of the brush:
   - Compute `enterFraction` and `leaveFraction` against plane
2. The intersection is `[max(enterFractions), min(leaveFractions)]`
3. If enter < leave: the ray enters this brush → record hit fraction and plane

Uses `offsets[8]` (box corner offsets) to push each plane test outward by the shape's extent. For `isPoint` traces, this is skipped for speed.

### Capsule Traces

`CM_TraceThroughSphere` and `CM_TraceThroughVerticalCylinder`:
- Used when `capsule=true`
- A capsule is a vertical cylinder (for body) + two spheres (for top/bottom)
- `CM_TraceCapsuleThroughCapsule`: capsule vs capsule (player vs capsule entity)
- More expensive than box traces but avoids catching on corners

### Double-Precision for Patches

Patch collision uses `DotProductDP` (double precision dot product) because curved surfaces can produce near-degenerate planes that lose precision in single float.

---

## cm_patch.c — Curved Surface Collision

**File:** `code/qcommon/cm_patch.c`  
**Size:** ~1830 lines

### What Patches Are

Q3 maps use bezier patches for curved surfaces (pipes, arches, organic shapes). Each patch is a 3×3 or larger grid of control points. For collision, patches are tessellated into a `patchCollide_t` structure containing:
- Flat planes (for brush-style intersection)
- Edge borders (for edge vs. box corner tests)

### Tessellation Level

`CM_GeneratePatchCollide(width, height, points)`:
- Computes subdivision level based on control point curvature
- Max subdivision depth: `PATCH_MAX_ITER = 3` (produces up to 9×9 = 81 quads per patch)
- Each quad → 2 triangles → 2 border planes + interior edge planes

### Trace Through Patch

`CM_TraceThroughPatch(tw, patch)`:
1. Broad-phase: check bounding box first
2. For each triangle in patch: test edge planes and surface plane
3. Uses double-precision where needed for stability

---

## cm_test.c — Point Tests and Area Portals

**File:** `code/qcommon/cm_test.c`

### CM_PointContents

Returns an OR of all content flags at a point (CONTENTS_SOLID, CONTENTS_WATER, etc.):
```
CM_PointContents(p, model):
  leaf = CM_PointLeafnum_r(p, 0)  ← descend BSP tree
  for each brush in leaf:
    if point inside brush: contents |= brush->contents
  return contents
```

### Area Portal System

The map is divided into "areas" — regions separated by area portals (doors). A portal can be open or closed. When closed, the two areas become invisible to each other (PVS).

`CM_AdjustAreaPortalState(area1, area2, open)`:
- Called by game QVM when a door opens/closes
- Rebuilds area connectivity

`CM_AreasConnected(area1, area2)`:
- Flood-fill from area1, returns true if area2 is reachable through open portals

`CM_WriteAreaBits(buffer, area)`:
- Returns bitmask of all areas connected to `area` (for snapshot PVS)

---

## Public API (cm_public.h)

All external callers use only these functions:

| Function | Callers | Notes |
|---|---|---|
| `CM_LoadMap(name, clientload, checksum)` | SV_SpawnServer, CL_CM_LoadMap | Loads BSP collision data |
| `CM_ClearMap()` | SV_ShutdownGameProgs | Free CM memory |
| `CM_InlineModel(index)` | SV_SetBrushModel, game QVM | Get handle for sub-model |
| `CM_TempBoxModel(mins, maxs, capsule)` | SV_Trace | Dynamic box/capsule handle |
| `CM_BoxTrace(results, start, end, mins, maxs, model, mask, capsule)` | SV_Trace | Main trace entry |
| `CM_TransformedBoxTrace(...)` | SV_ClipToEntity | Trace against transformed entity |
| `CM_PointContents(p, model)` | SV_PointContents, game QVM | Content flags at point |
| `CM_ClusterPVS(cluster)` | SV_BuildClientSnapshot | PVS bitmask |
| `CM_PointLeafnum(p)` | SV_AddEntitiesVisibleFromPoint | Leaf at point |
| `CM_BoxLeafnums(mins, maxs, list, size, last)` | Various | All leaves in box |
| `CM_AreasConnected(a1, a2)` | SV_inPVS | Area portal connectivity |
| `CM_AdjustAreaPortalState(a1, a2, open)` | SV_AdjustAreaPortalState | Door open/close |
| `CM_WriteAreaBits(buf, area)` | SV_BuildClientSnapshot | Area visibility mask |
| `CM_NumClusters()` | SV_Init | PVS cluster count |
| `CM_EntityString()` | SV_SpawnServer | Map entity lump |

---

## Key Data Structures

### clipMap_t — The Loaded BSP (global `cm`)

```c
typedef struct {
    char        name[MAX_QPATH];
    int         numShaders;       cshader_t       *shaders;
    int         numBrushSides;    cbrushside_t    *brushsides;
    int         numPlanes;        cplane_t        *planes;
    int         numNodes;         cNode_t         *nodes;
    int         numLeafs;         cLeaf_t         *leafs;
    int         numLeafBrushes;   int             *leafbrushes;
    int         numLeafSurfaces;  int             *leafsurfaces;
    int         numBrushes;       cbrush_t        *brushes;
    int         numClusters;
    int         clusterBytes;
    byte        *visibility;
    int         numEntityChars;   char            *entityString;
    int         numAreas;         cArea_t         *areas;
    int         numAreaPortals;   cAreaPortal_t   *areaPortals;
    int         numSurfaces;      cPatch_t        **surfaces;
    int         floodValid;
    int         checkcount;  // for trace deduplication
    cmodel_t    cmodels[MAX_SUBMODELS];
} clipMap_t;
```

### trace_t — Trace Result

```c
typedef struct {
    qboolean    allsolid;   // true if start point inside solid
    qboolean    startsolid; // true if start inside solid brush
    float       fraction;   // 0.0 = no movement, 1.0 = no hit
    vec3_t      endpos;     // final position
    cplane_t    plane;      // surface normal at impact
    int         surfaceFlags; // SURF_* flags of hit surface
    int         contents;   // CONTENTS_* of hit brush
    int         entityNum;  // entity hit (ENTITYNUM_NONE if world)
} trace_t;
```

### cplane_t — BSP Plane

```c
typedef struct cplane_s {
    vec3_t  normal;
    float   dist;
    byte    type;       // plane axis (PLANE_X/Y/Z for fast tests)
    byte    signbits;   // (x<0)<<0 | (y<0)<<1 | (z<0)<<2 (for BoxOnPlaneSide)
} cplane_t;
```

---

## Content Flags Reference

| Flag | Value | Meaning |
|---|---|---|
| `CONTENTS_SOLID` | 0x0001 | Opaque solid brush |
| `CONTENTS_LAVA` | 0x0008 | Lava fluid |
| `CONTENTS_SLIME` | 0x0010 | Slime fluid |
| `CONTENTS_WATER` | 0x0020 | Water volume |
| `CONTENTS_FOG` | 0x0040 | Fog volume |
| `CONTENTS_NOTTEAM1` | 0x0080 | Not solid to team 1 |
| `CONTENTS_NOTTEAM2` | 0x0100 | Not solid to team 2 |
| `CONTENTS_NOBOTCLIP` | 0x0200 | Invisible to bot pathfinding |
| `CONTENTS_AREAPORTAL` | 0x8000 | Area portal brush |
| `CONTENTS_PLAYERCLIP` | 0x10000 | Solid to players only |
| `CONTENTS_MONSTERCLIP`| 0x20000 | Solid to monsters only |
| `CONTENTS_TELEPORTER` | 0x40000 | Teleporter trigger |
| `CONTENTS_JUMPPAD` | 0x80000 | Jump pad trigger |
| `CONTENTS_BODY` | 0x2000000 | Entity body |
| `CONTENTS_CORPSE` | 0x4000000 | Dead body |
| `CONTENTS_TRIGGER` | 0x40000000 | Generic trigger volume |
| `MASK_SOLID` | SOLID\|BODY | Standard solid mask |
| `MASK_PLAYERSOLID` | SOLID\|PLAYERCLIP\|BODY | Player movement |
| `MASK_SHOT` | SOLID\|BODY\|CORPSE | Weapon hit detection |
