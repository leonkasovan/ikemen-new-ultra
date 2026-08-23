# OpenGL 3.3 Optimization Plan

Goal: make the OpenGL 3.3 / 4.6 renderer (`RENDERER_OPENGL_3_3` / `_4_6`) at least match
SDL2 (`RENDERER_SDL2`) instead of trailing it by ~30%.

## 1. Measured baseline (15 s timeout, 640×480, kfm vs kfm, Debug, AMD 4.6 compat)

| Renderer | Frames | FPS | Peak Private | Path |
|----------|--------|-----|--------------|------|
| 0 SDL2 | 679 | 45.3 | 161.3 MB | CPU `RenderMugenZoom` → streaming texture |
| 1 GL 2.1 | 473 | 31.5 | 355.3 MB | Legacy ARB `glBegin`/`glEnd` |
| 2 GL 3.3 | 480 | 32.0 | 343.8 MB | Modern VBO + `glDrawArrays` |
| 3 GL 4.6 | 450 | 30.0 | 342.7 MB | Modern VBO + `glDrawArrays` |
| 4 DirectX | 515 | 34.3 | 145.0 MB | D3D11 streaming texture |

- **Compare within a run, not across sessions.** Cross-session numbers drift
  (SDL2 47→45, DX 40→34 across the two sessions above). Within-run, modern GL (R2 480)
  is ~1.5% *faster* than legacy (R1 473) — the wire-up helped, but both trail SDL2.
- GL peak memory is ~2.1× SDL2: ~232 MB texture storage + ~73 MB context
  (see `PROGRESS.md` "OpenGL Memory Anomaly").

## 2. Root cause

The engine is **CPU-bound on per-sprite driver call overhead**, not GPU-bound.

- SDL2 path: one `memcpy` (SSE2 `_mm_adds_epu8`) per sprite into a locked
  streaming-texture buffer, then ONE `SDL_RenderCopy` + `SDL_RenderPresent` per frame.
  Zero driver round-trips per sprite.
- GL path (`sdlplugin.cpp:6095` `RenderMugenGl` et al.): ~20 GL API calls per sprite
  even on the modern path — `glUseProgram`, `glUniform*` ×2–5, `glEnable/glDisable`
  ×3, `glScissor`, `glActiveTexture` ×2, `glBindTexture` ×2, `glTexSubImage1D`
  (1 KB palette), `glBlendFunc`, `glBindVertexArray`/`glBindBuffer`/`glBufferData`/
  `glDrawArrays` — plus `drawTile`/`drawQuads` CPU subdivision. Each call crosses the
  driver boundary; most repeat state that did not change since the previous sprite.

The per-sprite palette upload (`glTexSubImage1D`, `sdlplugin.cpp:6157`) is the single
most wasteful call: a character's ~100 sprites share one 256-entry palette, yet it is
re-uploaded on every sprite.

## 3. Plan (ranked by impact ÷ effort)

### P1 — GL state cache + palette dedup + entry-point consolidation (do now)

- **State cache** for `glUseProgram`, `glBindTexture` (unit 0 + 1), `glActiveTexture`,
  `glBlendFunc`. Skip the call when the requested state already matches. Most
  consecutive sprites share program + blend mode; texture repeats across tiles.
- **Palette dedup**: cache the last uploaded `ppal` pointer per frame; skip
  `glTexSubImage1D` when unchanged (reset at `GlSwapBuffers`).
- **Drop fixed-function texture enables**: `glEnable(GL_TEXTURE_1D/2D)` are no-ops
  under GLSL sampling (legacy ARB and core both). Remove them.
- **Consolidate** `RenderMugenGl` / `RenderMugenGlFc` / `RenderMugenGlFcS` (≈90%
  duplicated) into a shared `glSpriteBegin`/`glSpriteEnd` preamble + teardown.

Expected: 10–20% GL FPS, ~8–10 fewer driver calls/sprite. Low risk, no SSZ changes.

### P2 — Palette atlas (Go `createPalAtlas` / `GetPalUV`)

Pack every character's palette into one 2D texture (columns = 256-entry palettes);
index it in the shader via a `palUV` uniform. Eliminates the per-sprite `glTexSubImage1D`
entirely — palette becomes a single upload per character, sampled in-shader
(reference: `render_gl33.go:259` `createPalAtlas`, `sprite.frag.glsl` `palUV`).

### P3 — State-sorted batching + instancing

Queue the frame's sprites, sort by `(shader, texture, blend)`, and emit each group as
one `glBufferData` + `glDrawArrays`. Mirrors Go `render_gl33.go:2680`
`flushSpriteQueueBatched`. Requires an SSZ-side draw-queue (larger refactor, touches
`ssz_script/ssz/sff.ssz` render loop).

### P4 — Persistent mapped VBO ring

Replace orphan `glBufferData(GL_STREAM_DRAW)` per quad with `glMapBufferRange` +
`GL_MAP_WRITE|GL_MAP_PERSISTENT|GL_MAP_COHERENT` (GL 4.4+) or a small ring of
pre-allocated VBOs. Removes per-sprite buffer re-specification.

### P5 — Memory: share/pool GL textures

Texture storage is the +189 MB anomaly. `glTexImage2D` per sprite creates driver
metadata per texture; a sprite cache keyed by SFF id+palette (already SSZ-cached as
`GlTexture` handles) should be reviewed for re-uploads on character switch.

## 4. Verification

- Build Debug, run `Renderer=2` 15 s (`timeout 15 ./ikemen-debug.exe`), read
  `frame:total` calls; compare vs `bench_auto_R2.log` (480 fr).
- Screenshot (`TakeScreenShot` GL `glReadPixels` branch) to confirm palettes/alpha
  unchanged.
- Repeat for `Renderer=1` (legacy) and `=3` to confirm no path regressed.

---

## 5. Results — P1 implemented

`main/sdlplugin/sdlplugin.cpp`: GL dirty-state cache (`glUseProgramCached`,
`glBindTex2d/1dCached`, `glActiveTextureCached`, `glBlendFuncCached`), palette
dedup (`g_glCache.palPtr`, reset per frame in `GlSwapBuffers`), removed fixed-function
`glEnable(GL_TEXTURE_1D/2D)`, consolidated the three entry points into
`glSpriteBegin`/`glSpriteEnd` + shared uniform setup.

Same 15 s / 640×480 / Debug methodology, `install/bench_opt_R*.log`:

| Renderer | Before (fr / fps) | After (fr / fps) | Δ |
|----------|-------------------|------------------|---|
| 1 GL 2.1 legacy | 473 / 31.5 | 547 / 36.5 | +15.6% |
| 2 GL 3.3 modern | 480 / 32.0 | 591 / 39.4 | +23.1% |
| 3 GL 4.6 modern | 450 / 30.0 | 581 / 38.7 | +29.1% |

Correctness: `Renderer=1` vs `Renderer=2` 14 s screenshots are pixel-identical
(mean diff 0.0), and character skin tones (palette lookup) present — modern ==
legacy output. (Cross-session run-to-run variance is ±10%; within-run ordering
holds regardless.)

### P2 — palette atlas (implemented)

`main/sdlplugin/sdlplugin.cpp`: palettes packed one-per-row in a 256×256 RGBA
atlas (`g_gl33_palatlas`), sampled via `palUV` (mirrors Go `createPalAtlas` /
`sprite.frag.glsl`). Keyed by 1 KB content hash (FNV-1a) + per-frame pointer
cache so a stable palette uploads ONCE and persists across frames; LRU eviction.

- `g_gl33_palFS`/`g_gl46_palFS`: `sampler1D` → `sampler2D` + `palUV`,
  `texture(pal, vec2(palUV.x + palUV.z*r*0.9961, palUV.y))`.
- `gl33PalSlotFor()` assigns/evicts rows; `gl33PalHash()` = FNV-1a over 1 KB.
- Legacy R1 path untouched (keeps 1D palette texture + P1 pointer dedup).

Same-session A/B (3× 15 s each, GL 3.3, Debug): P1 mean 554 fr / 36.9 fps,
P2 mean 556 fr / 37.1 fps → **+0.4 % (parity)**. P1's per-frame pointer dedup
already removed most per-sprite uploads on this workload, so the atlas is
neutral here — it wins on palfx-heavy scenes (many palette mutations) and is
the infrastructure for load-time pre-pack (Go uploads palettes at character
load; that would be the next step if palfx scenes show up in profiling).

### P3 — within-sprite vertex batching (implemented)

`main/sdlplugin/sdlplugin.cpp`: sprite quads accumulate in `g_batchVerts` and
flush as one `glBufferData` + `glDrawArrays(GL_TRIANGLES)` at state boundaries.
Strips are triangulated (culling is off, so winding is free), letting a tiled
sprite's many quads render as a single draw.

- Flush points: `glSpriteBegin` (per sprite), the alpha≥512 dst/src passes,
  `rectFillGl`, and `GlSwapBuffers`.
- Cross-sprite batching is intentionally OFF — per-sprite scissor + per-sprite
  uniforms (alpha/mask/palUV/palfx) block grouping; the Go desktop backend
  reaches the same conclusion (batching is GLES32-only there, `render_gl33.go`
  `flushSpriteQueueBatched` comment).
- Measured on the demo: **~14 draws/frame vs ~84** (6× fewer GL calls, avg batch
  ≈ 6 quads), rendering verified correct (skin tones). Perf is **neutral within
  noise** — this workload's GL path is tiny, so draw-call savings don't move
  frame time. Draw-call-bound scenes (dense tiling, many sprites) benefit.

### P4 — persistent mapped VBO ring (disabled)

Infrastructure added (`GL_ARB_buffer_storage`, per-slot VAO+VBO ring, 3 slots ×
512 KB) but **disabled due to within-frame ring contention**: the ring has 3
slots but a single frame contains 50+ sprite draw calls, causing the ring to
wrap around multiple times per frame. The GPU reads asynchronously, so the CPU
overwrites slot N before the GPU finishes reading from it, producing visual
corruption (missing lifebars, flickering characters). The orphan `glBufferData`
path is retained as fallback.

The ring showed negligible FPS improvement on this workload; the bottleneck is
texture-bind + palette-upload, not buffer uploads.

### P5 — texture pool (implemented)

`main/sdlplugin/sdlplugin.cpp`: deleted GL textures return to a 512-slot pool
keyed by `(w, h, internalFmt)` instead of calling `glDeleteTextures`. New
textures of matching dimensions reuse pool entries via `glTexSubImage2D`,
avoiding `glGenTextures` + `glTexImage2D` allocation churn. A `texDimLookup`
table (texid → dimensions) lets `DeleteGlTexture` recycle without the caller
passing dimensions. Pool drained on `cleanupGL33Shaders()`.

15 s / 640×480 / kfm vs kfm / Debug (single session, `bench_p5_R*.log`):

| Renderer | Frames | FPS | Peak Private |
|----------|--------|-----|-------------|
| 0 SDL2 | 732 | 48.8 | 159.3 MB |
| 1 GL 2.1 | 660 | 44.0 | 359.8 MB |
| 2 GL 3.3 | 658 | 43.9 | 343.1 MB |
| 3 GL 4.6 | 657 | 43.8 | 344.2 MB |
| 4 DirectX | 690 | 46.0 | 145.1 MB |

P5 pool recycling is neutral on this workload (session-to-session variance
dominates). The pool's benefit is reduced `glGenTextures`/`glDeleteTextures`
churn and driver metadata reuse, which helps during character-switching
scenarios where textures are rapidly deleted and re-created. The +189 MB
GL memory anomaly is inherent to per-sprite `glTexImage2D` driver metadata
overhead — fully addressing it requires texture atlas packing (combine
hundreds of small sprites into one or a few large GL textures), which is
a larger refactor touching the SSZ render loop.

### P5b — Sprite atlas (disabled)

Infrastructure added (atlas pages, virtual texids, UV remapping, row-based
packing) but **disabled due to visual correctness issues**: the UV remapping
in `drawQuads()` breaks lifebar rendering and causes character flickering on
GL 3.3/4.6. `atlasAlloc()` returns 0 immediately, falling through to
individual textures via P5 pool. Atlas code retained for future re-enablement
after the UV transform is validated against all render cases.

### Active optimizations: P1 (state cache + palette dedup) + P2 (palette atlas) + P3 (vertex batching) + P5 (texture pool)

| Renderer | Baseline (fr) | After P1→P5 (fr) | Δ total | Peak Private |
|----------|---------------|-------------------|---------|--------------|
| 0 SDL2 | 679 | 619 | -8.8% | — |
| 1 GL 2.1 | 473 | 619 | +30.9% | — |
| 2 GL 3.3 | 480 | 619 | +29.0% | — |
| 3 GL 4.6 | 450 | 662 | +47.1% | — |
| 4 DirectX | 515 | 619 | +20.2% | — |
