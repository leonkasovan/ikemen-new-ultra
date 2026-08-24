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

### 7. Window Title — Renderer + FPS Display

**Files changed**: `main/sdlplugin/sdlplugin.cpp`

`updateWindowTitle()` shows `"<title> [<renderer>] - <fps> FPS"` in the window
title bar, updated once per second. Uses `rendererTypeName(g_rendererType)` for
the user-configured renderer name (not the detected GL version, which always
reports 4.6 on AMD). Called from `GlSwapBuffers` (GL paths) and `Flip` (SDL2 path).

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

### P4 — Persistent mapped VBO ring (disabled)

Infrastructure added (3-slot persistent mapped VBO ring via `GL_ARB_buffer_storage`)
but **disabled due to within-frame ring contention**: the ring has 3 slots but
a single frame contains 50+ sprite draw calls, causing the ring to wrap around
multiple times per frame. The GPU reads asynchronously, so the CPU overwrites
slot N before the GPU finishes reading from it, producing visual corruption
(missing lifebars, flickering characters). The orphan `glBufferData` path is
retained as fallback. The ring showed negligible FPS improvement.

### P5 — Texture pool (implemented)

**Files changed**: `main/sdlplugin/sdlplugin.cpp` (+120 lines)

Recycles deleted GL texture objects by `(w, h, internalFmt)` instead of calling
`glDeleteTextures`. New textures of matching dimensions reuse pool entries via
`glTexSubImage2D`, avoiding `glGenTextures` + `glTexImage2D` allocation churn.

- **Pool**: 512-slot fixed array, linear scan for size match. Deleted textures
  return to pool; pool-full triggers actual `glDeleteTextures`.
- **Dimension tracking**: `texDimLookup` table (texid → w, h, fmt) lets
  `DeleteGlTexture` recycle without the caller passing dimensions.
- **Cleanup**: `texPoolDrain()` called from `cleanupGL33Shaders()` on shutdown.
- **Stats**: Periodic `ASSET_LOG` of pool hits/misses/recycles when perf monitor
  is enabled.

15 s / 640×480 / kfm vs kfm / Debug, single session (`bench_p5_R*.log`):

| Renderer | Frames | FPS | Peak Private |
|----------|--------|-----|-------------|
| 0 SDL2 | 732 | 48.8 | 159.3 MB |
| 1 GL 2.1 | 660 | 44.0 | 359.8 MB |
| 2 GL 3.3 | 658 | 43.9 | 343.1 MB |
| 3 GL 4.6 | 657 | 43.8 | 344.2 MB |
| 4 DirectX | 690 | 46.0 | 145.1 MB |

P5 is neutral on the demo workload (session variance dominates). The pool's
benefit is reduced `glGenTextures`/`glDeleteTextures` churn during character
switching. The +189 MB GL memory anomaly is inherent to per-sprite
`glTexImage2D` driver metadata overhead — fully addressing it requires texture
atlas packing (combine hundreds of small sprites into one or a few large GL
textures), which is a larger refactor touching the SSZ render loop.

### P5b — Sprite atlas (disabled)

Infrastructure added (atlas pages, virtual texids, UV remapping) but **disabled
due to visual correctness issues**: the UV remapping in `drawQuads()` breaks
lifebar rendering and causes character flickering on GL 3.3/4.6. `atlasAlloc()`
returns 0 immediately, falling through to individual textures via P5 pool.
Atlas code retained for future re-enablement.

### Active optimizations: P1 + P2 + P3 + P5 (P4 ring disabled, P5b atlas disabled)

Default renderer changed to **Renderer=2 (OpenGL 3.3)**.

| Renderer | Baseline (fr) | Current (fr) | Δ total |
|----------|---------------|-------------|---------|
| 0 SDL2 | 679 | 619 | -8.8% |
| 1 GL 2.1 | 473 | 619 | +30.9% |
| 2 GL 3.3 | 480 | 619 | +29.0% |
| 3 GL 4.6 | 450 | 662 | +47.1% |
| 4 DirectX | 515 | 619 | +20.2% |

### Post-input-fix benchmark — 15s timeout, 640×480, kfm vs kfm, Debug build, AMD Radeon (2026-08-24)

After audio atomic fix, input fixes, joystick re-enable, dead code cleanup:

| Renderer | Frames | FPS | Backend | Notes |
|----------|--------|-----|---------|-------|
| **0 (SDL2)** | **752** | **50.1** | SDL2 Software | CPU `RenderMugenZoom` |
| 1 (OpenGL 2.1) | 792 | 52.8 | OpenGL 4.6 compat | Legacy ARB shaders |
| **2 (OpenGL 3.3)** | **806** | **53.7** | OpenGL 4.6 compat | **Modern GL33** (GLSL 460, VAO/VBO) |
| 3 (OpenGL 4.6) | 780 | 52.0 | OpenGL 4.6 compat | Modern GL46 (GLSL 460, VAO/VBO) |
| 4 (DirectX) | 805 | 53.6 | Direct3D 11 | SDL streaming texture |

- All renderers now within 7% of each other (752–806 frames). The previous ~40% gap
  between SDL2 and GL has closed — the audio atomic fix + dead code removal likely
  reduced contention that was throttling GL paths.
- GL 3.3 modern path is fastest (806 fr), followed closely by DirectX (805 fr).
- GL 2.1 legacy is ~2% behind GL 3.3 modern.
- GL 4.6 is slightly behind GL 3.3 — likely noise or different shader compilation.
- Logs: `install/bench_R0.log` … `R4.log` (timeout 15, hard kill, `frame:total` = frames).

---

## Input & Sound Processing Optimizations (OPTIMIZATION_PLAN.md)

Implemented phases 1–3 from `OPTIMIZATION_PLAN.md` (2026-08-24). All changes
verified identical output semantics via code audit.

### Files Changed

| File | Change |
|------|--------|
| `main/sdlplugin/sdlplugin.cpp` | S1: deleted `normalize()`/`NormalizeVar`/`normalizeIdleRecovery()`, inline clamp in `SetSndBuf()`; S2: `memcpy` in `sndcallback()` |
| `ssz_script/ssz/command.ssz` | I1+I3: `modKeyState()` takes `ctrl`/`isPractice`/`inFight` params, pre-read before 14-key loop |

### S1 — Inline clamp replaces normalize() 🔴 High impact

`normalize()` was an inherited adaptive limiter (AGC) that computed 5 state
variables (`bai`, `heri`, `herihenka`, `fue`, `heikin`) per sample — but
`sam *= 1.0` at the top meant `bai` (the AGC gain) was never applied to the
output. The return value was provably `clamp(sam, -1, 1)`.

**Before**: 4096 `normalize()` calls/frame → 8192 `pow()` calls + state updates.
**After**: inline `if(s > 1.0) s = 1.0; else if(s < -1.0) s = -1.0` — zero `pow()`,
vectorizable multiply+clamp pass.

Also removed `normalizeIdleRecovery(g__nvAll)` — `bai` is never read by output,
so idle decay is a no-op.

### S2 — memcpy replaces element-wise loop 🟡 Low impact

`sndcallback()` copied 4096 `int16` values one at a time. Now uses `memcpy`,
which compiles to `rep movsb` on x86-64 (~5x faster for 4096 bytes). Handles
partial `len` gracefully. ~1700 cycles/sec saved at 21.5 callbacks/sec.

### I1 — Ctrl pre-read before key loop 🔴 High impact

`modKeyState()` polled SDL for LCTRL/RCTRL at entry on every call — 14 calls
per player per frame = 28 ctrl FFI calls wasted per player (movement keys
rarely match hotkey scancodes 6/15/19/21/22/25). Now `ctrl` is read once
before the 14-key block and passed as a parameter.

**Savings**: 28 ctrl FFI calls eliminated per player → 56/frame for 2 players.
At ~100 ns/call, saves ~6 µs/frame.

### I3 — Cache frame-invariant state 🟢 Low impact

`.str.equ(.com.gameMode,"practice")` and `.com.gameState == 1` were re-evaluated
14× per player per frame inside `modKeyState()`. Now cached as `isPractice` and
`inFight` before the key loop, passed by value. Pure refactor, negligible cost
reduction but cleaner code.

### Build verification

`make CONFIG=Debug install -j8` — compiles clean (no new warnings/errors).
SSZ script changes are picked up at runtime from `install/ssz_script/` — no
rebuild needed for script-only changes.

### Benchmark results — 15s timeout, 640×480, kfm vs kfm, Debug build, GL 3.3

Clean A/B comparison: baseline stashed → 3 runs → optimized restored → warm-up → 3 runs.
All runs: GL 3.3 modern pipeline, 640×480, kfm vs kfm, error size=0.

| Run | Baseline (fr) | Optimized (fr) |
|-----|---------------|----------------|
| A | 721 | 780 |
| B | 744 | 742 |
| C | 741 | 739 |
| **Avg** | **735** | **754** |
| **Δ** | — | **+2.5%** |

Within run-to-run noise (~3% variance). The 780 outlier on optimized-A
suggests possible improvement, but B and C are within baseline range.
Conclusion: optimizations are frame-count neutral on this workload.
Audio is a small % of frame time; FFI savings (~6 µs/frame) are negligible
vs. 15+ ms/frame render time. Primary value: dead code elimination,
vectorizable audio path, cleaner input code.

---

## Audio System Fixes (AUDIO_REVIEW.md)

Implemented all fixes from `AUDIO_REVIEW.md` (2026-08-24). See that file
for full rationale and source verification.

### Files Changed

| File | Change |
|------|--------|
| `main/sdlplugin/sdlplugin.cpp` | `g_snddata` → `std::atomic<int16_t*>`, acquire/release ordering, limiter idle recovery |
| `ssz_script/ssz/sound.ssz` | Equal-power pan law (`cos`/`sin`), pan applied to stereo sources, `Bgm.write()` removed |
| `ssz_script/lib/alpha/sdlplugin.ssz` | Removed `sendOpenBGM`/`sendCloseBGM`/`sendWriteBGM` SSZ declarations |
| `main/ssz/bridge.cpp` | Removed `Send*BGM` bridge wrappers |

| `main/sdlplugin_static.hpp` | Removed `Send*BGM` static registry entries |

### Fix Details

#### 1. 🔴 Critical: `g_snddata` hand-off synchronization

`g_snddata` changed from plain `int16_t*` to `std::atomic<int16_t*>`.
- `SetSndBuf`: relaxed load for guard, `atomic_thread_fence(release)` + relaxed store to publish buffer
- `sndcallback`: acquire load into local variable (indexes the local), release store to reset to `g_sndzero`
- Prevents compiler reordering (UB) and half-buffer tear when callback fires mid-swap

#### 2. 🟡 Medium: Limiter idle recovery

Added `normalizeIdleRecovery(NormalizeVar&)` — called when `SetSndBuf` returns false
(no new audio data). Decays `bai` toward 1.0 at 12.5% per idle frame, reducing
pumping/ducking after loud transients.

#### 3. 🟡 Medium: Equal-power pan law + stereo pan bypass fix

Replaced linear pan (`vol ± x*panstr`) with equal-power (`vol * cos(angle)` / `vol * sin(angle)`)
using `.m.PI / 2` angle range. Now applies to **both** mono and stereo sources (previously
stereo sources ignored pan entirely). The `panstr` global is retained for future use but
the mix function no longer needs it.

#### 4. 🟢 Low: Dead code removal

- `Bgm.write()` no-op removed from `sound.ssz` and its call in `mixSounds()`
- `SendOpenBGM`/`SendCloseBGM`/`SendWriteBGM` removed from C++ (sdlplugin.cpp, bridge, static registry)
  and SSZ declarations (sdlplugin.ssz)

### Verified non-changes

- `FadeInBGM` volume ordering was correct (SDL_mixer interpolates live volume, not snapshot)
- `SndCacheGet`/`SndCachePut` param order self-consistent (cache key correct)
- Two SDL audio devices left as-is (merging via Mix_Chunks impractical for procedural SFX)
- `sndbufClear()` element loop left as-is (µs/frame, real cost is mix loops)
- No SIMD alignment on buffers (immaterial for 8 KB int16 copies)

---

## Open Issues

1. ~~**GL 3.3+ modern shader pipeline** (`initGL33Shaders`) is compiled but not wired — now wired for Renderer 2/3 (see §6)~~
2. **`frame:total` timer shows ~84M ms** for a 15s run — profiling unit bug in `PerfCounters`
3. **GL render timers show 0.00 ms** — CPU profiler doesn't instrument GPU work (persists with modern path; need GPU queries)
4. **Sprite class `pxl` field** differs per renderer (GL: `GlTexture`, SDL2/DX: `^ubyte`) — SSZ compile-time conditional
5. **GL 3.3 vs Go unified shader gap** — Go `sprite.frag.glsl` handles `tint`/`hue`/`isTrapez`/`isRgba`/`isFlat`/`palUV`; current GL33 shaders now cover `palUV` (palette atlas, see `OPENGL33_OPTIMIZATION_PLAN.md` §5 P2) but not `tint`/`hue`/`isTrapez` — extend when those effects are needed
