# PROGRESS.md — Rendering Backend Development

## Renderer Architecture

### Renderer Type Mapping

| Config (SSZ `cfg.Renderer`) | Enum (`RendererType`) | SSZ String | Init Path |
|-----------------------------|----------------------|------------|-----------|
| 0 | `RENDERER_SDL2` (1) | "SDL2" | SDL2 software + streaming texture |
| 1 | `RENDERER_OPENGL_2_1` (2) | "OpenGL 2.1" | Raw GL context + legacy ARB shaders |
| 2 | `RENDERER_OPENGL_3_3` (3) | "OpenGL 3.3" | Raw GL context + modern GL 3.3 shaders |
| 3 | `RENDERER_OPENGL_4_6` (4) | "OpenGL 4.6" | Raw GL context + modern GL 4.6 shaders |
| 4 | `RENDERER_DIRECTX` (5) | "DirectX" | SDL2 D3D11 backend + streaming texture |

### Key Files

- `main/sdlplugin/renderer.h` — Enum definitions, `isOpenGLRenderer()`, `parseRendererType()`
- `main/sdlplugin/sdlplugin.cpp` — All renderer init, GL render functions, streaming texture
- `ssz_script/lib/alpha/sdlplugin.ssz` — SSZ `init()` → integer-to-string renderer mapping
- `ssz_script/ssz/*.ssz` — Compile-time `/?/` conditionals for GL vs software paths

---

## Completed Work

### 1. DirectX Renderer (Renderer=4)

**Files changed**: `main/sdlplugin/sdlplugin.cpp`

- Added `RENDERER_DIRECTX` path in `RendererInit()` — tries D3D11 first, falls back to D3D9
- `GlSwapBuffers()` redirects to SDL2 streaming texture present for non-GL renderers
- `fillRendererInfo()` queries DXGI adapter name via `IDXGIFactory`
- `InitMugenGl()` guard returns `true` (skip GL shader setup) for non-GL renderers

**How it works**: SDL2's `direct3d11` render driver backs the streaming texture. Sprites render on CPU via `RenderMugenZoom` → `g_pix` buffer, then `SDL_RenderCopy` + `SDL_RenderPresent` blits to D3D11.

### 2. SSZ Compile-Time Conditionals

**Files changed**: All `ssz_script/ssz/*.ssz` (8 files, 21 conditionals)

The `/?/` directive is a **tokenizer-level** compile-time conditional (`source.hpp` `SkipSpace`). `ConstShiki()` evaluates the expression at parse time. For `/?/*.cfg.Renderer != 0 && .cfg.Renderer != 4 && .cfg.Renderer != 1:` — the expression is evaluated as a compile-time constant. If false, the block is skipped as a comment.

| Old conditional | New conditional |
|----------------|----------------|
| `/?/*.cfg.Renderer != 0:` | `/?/*.cfg.Renderer != 0 && .cfg.Renderer != 4 && .cfg.Renderer != 1:` |
| `/?/*.cfg.Renderer == 0:` | `/?/*.cfg.Renderer == 0 \|\| .cfg.Renderer == 4 \|\| .cfg.Renderer == 1:` |

**Effect**: GL code only compiles for Renderer 1/2/3 (OpenGL). Renderer 0 (SDL2) and 4 (DirectX) use the software rendering path (`renderMugenZoom`).

**Sprite class difference** (in `sff.ssz`):
- OpenGL: `^&.sdl.GlTexture pxl` — GL texture handle (pixel data in driver memory)
- SDL2/DX: `^ubyte pxl` — Raw pixel array on SSZ heap

### 3. RendererType Enum Refactoring

**Files changed**: `main/sdlplugin/renderer.h`, `main/sdlplugin/sdlplugin.cpp`, `ssz_script/lib/alpha/sdlplugin.ssz`

**Old enum**:
```
RENDERER_UNKNOWN=0, RENDERER_SDL2=1, RENDERER_OPENGL=2,
RENDERER_OPENGLES=3, RENDERER_VULKAN=4, RENDERER_DIRECTX=5
```

**New enum**:
```
RENDERER_UNKNOWN=0, RENDERER_SDL2=1, RENDERER_OPENGL_2_1=2,
RENDERER_OPENGL_3_3=3, RENDERER_OPENGL_4_6=4, RENDERER_DIRECTX=5
```

- Added `isOpenGLRenderer(t)` helper — returns true for GL 2.1/3.3/4.6
- Removed `RENDERER_OPENGLES` and `RENDERER_VULKAN` (Vulkan stub removed, GLES removed)
- Removed `GLESVersion` enum and `g_glesVersion` global
- All `g_rendererType == RENDERER_OPENGL || == RENDERER_OPENGLES` → `isOpenGLRenderer(g_rendererType)`

### 4. GL Version Probing with Fallback

- Requests GL 2.1 context (minimum) to avoid AMD driver slowdown from requesting high versions
- Detects actual version via `detectGLVersion()` after context creation
- Logs: `"Detected OpenGL version: 4.6 (requested: OpenGL 3.3)"`

**Note**: On AMD drivers, requesting GL 4.6 explicitly via `SDL_GL_SetAttribute` causes ~20% FPS regression. Always request 2.1 and let the driver give the highest version.

### 5. GL InitMugenGl Guard

```cpp
bool SSZ_STDCALL InitMugenGl()
{
    if (!isOpenGLRenderer(g_rendererType))
    {
        INIT_LOG("InitMugenGl: non-GL renderer (%s), skipping GL shader setup",
            rendererTypeName(g_rendererType));
        return true;
    }
    // ... GL shader setup ...
}
```

### 6. GL 3.3 Modern Shader Pipeline — Wired

**Files changed**: `main/sdlplugin/sdlplugin.cpp` (+~180 lines)

**Reference**: `C:\Projects\ikemen-develop-update\src\render_gl33.go` + `shaders/sprite.vert.glsl` / `sprite.frag.glsl`

Wired `initGL33Shaders()` (GLSL 330/460 + VAO/VBO) into the render path. Previously all GL renderers used legacy ARB `glBegin`/`glEnd`; now Renderer 2 (GL 3.3) and 3 (GL 4.6) use the modern path, Renderer 1 (GL 2.1) keeps legacy ARB.

**Dispatch**: `g_useModernGL` set on successful `initGL33Shaders()` in `RendererInit`; `useModernRender()` = `g_useModernGL && (type==3.3||4.6)`. `InitMugenGl()` early-returns on modern (skips ARB build, disables depth test, enables blend).

**Shaders**:
- `g_gl33_palVS` shared vertex shader (`#version 330/460`, `aPos`/`aUV` → `uProj`)
- `g_gl33_palFS` / `g_gl46_palFS` — indexed palette (`sampler2D tex` + `sampler1D pal`, `msk`, `a`)
- `g_gl33_fcFS` / `g_gl46_fcFS` — full-color (`neg`, `gray`, `add`, `mul`, `a`)
- `g_gl33_shadowFS` / `g_gl46_shadowFS` — shadow (`color`, `a`)
- `g_gl33_flatProg` — solid fill (`color`) for `MugenFillGl`, shares `palVS` (new)

Sampler bindings fixed: `pal` → unit 1 (was default 0, sampled wrong texture), `tex` → unit 0.

**Geometry**: `drawQuads()` / `rectFillGl()` branch on `useModernRender()`:
- Modern: build strip vertices (same `n` formula + UV order as `glBegin` path) into `std::vector<float>`, `glBufferData(..., GL_STREAM_DRAW)` (orphan avoids `glBufferSubData` stall from earlier batch experiment) + `glDrawArrays(GL_TRIANGLE_STRIP)`. `rectFillGl` uploads 4 vertices as strip via flat program (`glUniform4f color`).
- Legacy: unchanged `glBegin`/`glEnd` + `glColor4f`.

**Projection**: `gl33SetFrameProjection()` computes `ortho(0,w,0,h) * translate(0,h,0)` as `uProj` uniform (matches legacy `glOrtho` + `glTranslated`). Uploaded per-frame in `GlSwapBuffers` and lazily on first sprite before first swap. `renderMugenGl`/`MugenFillGl` skip `glMatrixMode`/`glPushMatrix` on modern.

**Entry points**: `RenderMugenGl` → palette prog, `RenderMugenGlFc` → fullcolor prog, `RenderMugenGlFcS` → shadow prog, `MugenFillGl` → flat prog; each selects program/uniform locations via `modern ? g_gl33* : g_uniform*` and uses `glUseProgram` vs `glUseProgramObjectARB`.

**Compat context**: Window requests GL 1.1 compat profile; driver gives highest compat (4.6 on test HW), so both `#version 330 core` and legacy fixed-function coexist. No `SDL_GL_CONTEXT_PROFILE_MASK` change needed.

**Verification**: Debug build, Renderer=2, 640×480 kfm vs kfm — 4 programs link, VAO/VBO created, `InitMugenGl: modern pipeline active`, 543 frames/15 s, HUD/lifebars/stage/characters/shadows render correctly (screenshot `install/gl33_wire_capture.png` removed after check).

---

## Benchmark Results

### Pre-wiring — 15s timeout, 640×480, kfm vs kfm, Debug build, AMD Radeon

| Renderer | Frames | FPS | Peak Private | Notes |
|----------|--------|-----|-------------|-------|
| **0 (SDL2)** | **708** | **47.2** | **159.4 MB** | CPU software blit to streaming texture |
| 1 (OpenGL 2.1) | 615 | 41.0 | 348.4 MB | Legacy ARB shaders, detected GL 4.6 |
| 2 (OpenGL 3.3) | 615 | 41.0 | 347.4 MB | Same legacy path (modern pipeline not wired) |
| 3 (OpenGL 4.6) | 615 | 41.0 | 348.2 MB | Same legacy path (modern pipeline not wired) |
| **4 (DirectX)** | **600** | **40.0** | **145.9 MB** | D3D11 via SDL2 |

### Post-wiring — 15s timeout, 640×480, kfm vs kfm, Debug build, AMD Radeon (2026-08-23, `bench_auto_R*.log`)

| Renderer | Frames | FPS | Peak Private | Backend | Shader Path |
|----------|--------|-----|-------------|---------|-------------|
| **0 (SDL2)** | **679** | **45.3** | **161.3 MB** | SDL2 Software | CPU `RenderMugenZoom` |
| 1 (OpenGL 2.1) | 473 | 31.5 | 355.3 MB | OpenGL 4.6 compat | Legacy ARB (`g_mugenshader*`, `glBegin`) |
| 2 (OpenGL 3.3) | 480 | 32.0 | 343.8 MB | OpenGL 4.6 compat | **Modern GL33** (GLSL 460, VAO/VBO, `glDrawArrays`) |
| 3 (OpenGL 4.6) | 450 | 30.0 | 342.7 MB | OpenGL 4.6 compat | **Modern GL46** (GLSL 460, VAO/VBO) |
| 4 (DirectX) | 515 | 34.3 | 145.0 MB | Direct3D 11 | SDL streaming texture |

- All GL contexts are **4.6 Compatibility Profile** (request 2.1, driver gives 4.6) — modern shaders compile as `#version 460 core` in compat.
- R1 builds modern shaders (`Palette shader: program=3`) but **does not use them** (`modern GL3.3 pipeline active` only on R2/R3) — verifies `useModernRender()` gate.
- Modern path ~1–2% faster than legacy on same HW (480 vs 473) and ~11 MB lower peak, but still ~2× SDL2 memory and slower than SDL2/DX — per-sprite texture bind + palette upload remains bottleneck (see §GL 3.3 VBO/VAO Investigation “What would actually make GL faster”).
- Logs: `install/bench_auto_R0.log` … `R4.log` (timeout 15, hard kill, `frame:total` calls = frames).

### Memory Profile (peak private bytes at milestones)

| Milestone | SDL2 | OpenGL | DirectX |
|-----------|------|--------|---------|
| PROCESS-START | 2.4 | 2.4 | 2.4 |
| POST-INIT | 35.8 | 109.1 | 22.8 |
| LUA-COMMON | 104.7 | 140.0 | 92.0 |
| MATCH-START | 144.5 | 182.1 | 132.0 |
| FIGHT-SETUP-DONE | 157.7 | 347.2 | 144.4 |
| **Peak** | **159.4** | **348.4** | **145.9** |

### Why SDL2 is faster than OpenGL (legacy path)

The GL render path uses **legacy immediate-mode GL** (`glBegin`/`glEnd`), which is emulated by AMD drivers:

**Per sprite**: ~20 GL API calls (`glUseProgramObjectARB`, `glUniform1iARB` ×2, `glEnable` ×3, `glScissor`, `glActiveTexture` ×2, `glTexSubImage1D`, `glBindTexture`, `glPushMatrix`, `glTranslated`, `glPopMatrix`, `glUniform1fARB`, `glBlendFunc`, `glBegin`/`glEnd` with 4-12 vertices)

**SDL2 path**: One `memcpy` per sprite (CPU → streaming texture), then one `SDL_RenderCopy` + `SDL_RenderPresent` per frame.

### OpenGL Memory Anomaly (+189 MB vs SDL2)

| Component | SDL2 | OpenGL | Delta |
|-----------|------|--------|-------|
| SSZ heap (pixel data) | 98.9 MB | 43.4 MB | −55.5 MB (freed after GL upload) |
| GL driver (textures + context) | 0 | ~305 MB | +305 MB |
| SDL/other overhead | 60.7 MB | 0 | — |
| **Total** | **159.4** | **348.4** | **+189 MB** |

GL context overhead: ~73 MB (driver state, command buffer, secondary context, shader cache).
GL texture storage: ~232 MB (per-sprite `glTexImage2D` calls, driver metadata overhead).

---

## GL 3.3 VBO/VAO Batch Rendering — Investigation

### What was built

- `GL33_BATCH_MAX_QUADS = 2048` vertex buffer (192 KB)
- `batchQuad()` accumulates 2 triangles (6 vertices) per quad
- `flushGL33Batch()` uploads via `glBufferSubData` + `glDrawArrays(GL_TRIANGLES)`
- State change detection: flush before texture/shader/blend changes
- Frame lifecycle: begin batch at `GlSwapBuffers` start, flush before `SDL_GL_SwapWindow`

### Why it was reverted

`glBufferSubData` per-sprite is **slower** than `glBegin`/`glEnd` when we must flush after every sprite (different texture per sprite):

| Method | Per-sprite overhead |
|--------|-------------------|
| `glBegin`/`glEnd` | ~2-5 μs (driver internal buffer) |
| `glBufferSubData` + `glDrawArrays` | ~8-15 μs (GPU upload + draw) |

The batch only helps when **multiple quads accumulate before flushing** (tiled sprites with 5-10+ quads). For simple sprites (1 quad), it adds overhead.

### What would actually make GL faster

1. **Sort sprites by state** — group by shader + blend mode + texture, batch within each group
2. **Upload palette once per frame** — same 1 KB for all sprites in a character
3. **Use instanced rendering** — one draw call for all sprites with the same texture
4. **Use texture atlases** — pack all sprites into one texture, eliminate per-sprite bind

These require restructuring the SSZ render loop (larger refactor).

---

## Open Issues

1. ~~**GL 3.3+ modern shader pipeline** (`initGL33Shaders`) is compiled but not wired — now wired for Renderer 2/3 (see §6)~~
2. **`frame:total` timer shows ~84M ms** for a 15s run — profiling unit bug in `PerfCounters`
3. **GL render timers show 0.00 ms** — CPU profiler doesn't instrument GPU work (persists with modern path; need GPU queries)
4. **Sprite class `pxl` field** differs per renderer (GL: `GlTexture`, SDL2/DX: `^ubyte`) — SSZ compile-time conditional
5. **GL 3.3 vs Go unified shader gap** — Go `sprite.frag.glsl` handles `tint`/`hue`/`isTrapez`/`isRgba`/`isFlat`/`palUV`; current GL33 shaders now cover `palUV` (palette atlas, see `OPENGL33_OPTIMIZATION_PLAN.md` §5 P2) but not `tint`/`hue`/`isTrapez` — extend when those effects are needed
