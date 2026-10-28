# GL Stage Background Rendering Bug — RESOLVED

**Stage:** `XX'DB'CLOCK'TOWER'TOP'XX.def` (SFFv2, `XX'DB'CLOCK'TOWER'TOP'XX.sff`)

**Symptom (as reported):** left half of the screen black with OpenGL renderers (1/2/3); correct with the software renderer (0). By the time this was investigated the symptom had worsened to a **completely black** GL frame.

**Status: fixed.** Two independent defects in `main/sdlplugin/sdlplugin.cpp`, both OpenGL-only:

| # | Defect | Effect |
|---|--------|--------|
| 1 | `drawTileHolizon()` returned early for `tile.w <= 0` | every non-tiled sprite draw became a no-op → **whole GL frame black** |
| 2 | `RenderMugenGl()`/`RenderMugenGlFc()` clamped the **sprite rect** to the viewport | stage layers drawn at 1/3 size and shifted → **left half black** (the original report's symptom) |

---

## Root cause 1 — `tile.w == 0` was treated as "don't draw"

`drawTileHolizon()` (sdlplugin.cpp:7239) is the horizontal-tiling helper for the GL path. `tile.w` is the horizontal *repeat count*: `0` means "not tiled — draw the sprite once", which is the normal case (the script layer passes `tile = {0,0,0,0}`, see `sff.ssz` `Sprite.draw()`).

A hardening pass added:

```cpp
int n = tile.w;
if(n <= 0) return;      // <-- draws NOTHING for the ordinary case
```

Pre-existing behaviour for `n == 0` was: emit one quad, then `if(n == 0) break;`. So the guard silently converted every ordinary sprite draw into a no-op.

**Measured** (instrumented counters, one frame, Renderer=2):

```
drawn=57056   tile=57056   tileHz=57056   hzSkip=0   wLe0=57056   quads=0
```

`wLe0 == tileHz` — *every* call bailed out at the guard, and the quad counter stayed at zero for the whole run. After the fix: `quads=55698`, `quadsPushed=55698`, and the batch flushes again.

**Fix:**

```cpp
int n = tile.w;
if(n < 0) n = 0;        // negative width: same "draw once" path
int guard = 0;          // the step guard still bounds the loop
```

`drawTile()`'s vertical loop (`n = tile.h`) was never given the guard, which is why the vertical case kept working and only the *horizontal* helper broke.

---

## Root cause 2 — the viewport clamp overwrote the sprite's own rect

Both GL entry points began with:

```cpp
// Clamp clipping rect to game viewport ...
if(rect->x >= g_w || rect->y >= g_h
   || rect->x + rect->w <= 0 || rect->y + rect->h <= 0){
    rect->x = 0; rect->y = 0;
    rect->w = g_w; rect->h = g_h;      // <-- destroys the sprite's dimensions
}
```

`rect` here is the **sprite rect** (`spr~rct`), not a clipping rect. Its `x`/`y` are the sprite's **axis / draw offset**, which legitimately exceed the screen: the stage SFF's layers are 2048x1019 and 2048x1536 with axis `(1024,476)` and `(1024,993)`.

Parsed straight from `XX'DB'CLOCK'TOWER'TOP'XX.sff` (SFFv2, 10 sprites):

```
g=0   n=0  640x480    axis=(320,0)     fmt=10
g=100 n=0  640x480    axis=(320,0)     fmt=10
g=100 n=1  640x480    axis=(320,0)     fmt=10
g=1   n=0..3  2048x1019  axis=(1024,476)   fmt=10
g=2   n=0..2  2048x1536  axis=(1024,993)   fmt=10
```

So `rect->x >= g_w` is `1024 >= 640` → **true** → the rect is rewritten to `(0,0,640,480)`, and `drawTile(r.w, r.h, …)` then builds the layer with `w=640, h=480` instead of `2048x1019`. The layer is drawn at roughly a third of its size, its geometry slides off to the right, and the uncovered left part of the screen shows the cleared (black) buffer.

**Measured** (log at draw time, Renderer=2):

```
tex=797 driver=2048x1019 table=2048x1019 rect=0,0,640,480     <- rewritten from (1024,476,2048,1019)
tex=794 driver=640x480   table=640x480   rect=320,0,640,480   <- axis 320, correctly NOT rewritten
```

**Decisive coverage test:** with the palette shader temporarily replaced by `FragColor = vec4(1,0,1,1)` (no discard), magenta coverage was `295716` pixels, split `left=147684 / right=148032`, with **no column un-covered**. So geometry did reach the left half; the pixels there were black because the layer's *size/position* was wrong, not because nothing was drawn.

**Fix** — clamp the **clip window** (`dstr`), which is what the clamp was written for (the script side already does exactly this for its `window` rect in `sff.ssz` `Animation.draw()`):

```cpp
if(dstr && (
       dstr->x >= g_w || dstr->y >= g_h
    || dstr->x + dstr->w <= 0 || dstr->y + dstr->h <= 0)){
    dstr->x = 0; dstr->y = 0;
    dstr->w = g_w; dstr->h = g_h;
}
```

Applied at both sites (`RenderMugenGl`, `RenderMugenGlFc`). `RenderMugenGlFcS` never had the clamp.

---

## Why this hit only OpenGL, and only the stage

- `drawTileHolizon`, `drawTile`, `drawQuads` and the rect clamps exist only in the GL sprite path. The software/DirectX path (`RenderMugenZoom` → `mRender`) clips source and destination pixel-wise and never mutates the sprite rect — hence `Renderer=0` and `4` were always correct.
- Only the stage has sprites whose axis exceeds 640x480 *and* whose size exceeds the viewport. Characters and HUD sprites have small axis values and pass the clamp untouched, which is why characters and lifebars kept rendering while the background vanished.

## Verification matrix

Auto-screenshot at frames 150–900 of an active match (`lua_script/script/match.lua` `loop()` hook, `roundstate()==2`), measured over the stage area `y=140..470`:

| Renderer | before | after |
|---|---|---|
| 1 (GL 2.1) | 0/307200 non-zero — all black | 0 black columns, L=105600 R=105600 |
| 2 (GL 3.3) | 0/307200 — all black | 0 black columns, L=105600 R=105600 |
| 3 (GL 4.6) | 0/307200 — all black | 0 black columns, L=105600 R=105600 |
| 0 (SW) | correct | 0 black columns, L=105600 R=105600 |
| 4 (DirectX) | correct | 0 black columns, L=105600 R=105600 |

`L`/`R` are non-black pixel counts per half; 105600 = 320x330 is full coverage. Visual check: the tower stage renders edge to edge at correct scale.

---

## Corrections to the earlier investigation in this file

The original report's conclusions, re-checked:

- **"No compaction exists; the compaction theory is refuted" — correct and still stands.** Refs are `new[]`/`delete[]` blocks (`MemoryKakuho`), and `CircularGC` never moves memory.
- **"Stale/dangling pixel buffer (use-after-free)" — not the cause.** No dangling buffer is needed to explain the symptom; the pixels were fine and the *geometry* was wrong.
- **The all-zero 640x480 upload (`nz=0/307200`) is real but irrelevant.** Dumping every large texture by id showed exactly one all-zero 640x480 buffer (id 794 = sprite `g=0,n=0`, a 1162-byte PNG8 layer), while its neighbours 795/796 and all 2048-wide layers carried data. The scene renders correctly with that blank layer still present — it is a legitimately empty layer, not corruption.
- **The `refnew` leak (`arrayandref.hpp`) is unrelated.** Verified by re-testing with the release removed: the all-zero texture appeared identically. The leak fix stands on its own (see `test_refnew_leak.cpp`), but it neither caused nor cured this bug.
- **The `px=` / pass-by-reference script workaround is unrelated to the rendering.** It affects whether `px.new()` buffers reach callers, not sprite geometry.
- **Superseded:** the "16 GB / heap-state / first-load" theories, the "JIT limitations" table, and the `SetPxlLog` dispatch puzzle — all were chasing a data-corruption hypothesis for what is a geometry bug.

## Reproduction

1. `make CONFIG=Debug install -j8`
2. Set `Renderer = 2` in `install/save/config.ssz`
3. Add a temporary screenshot hook at the top of `loop()` in `lua_script/script/match.lua`:
   `if roundstate() == 2 then takeScreenShot(screenshotPath.."/shot.png") end`
4. Run, reach the stage, inspect `install/screenshots/shot.png`
5. Expected after the fix: the stage fills the full 640x480. Before: the left ~half is black (with defect 1 re-added: the entire frame is black).

---

# Historical investigation (superseded, kept for context)

## Original problem statement

- Stage `XX'DB'CLOCK'TOWER'TOP'XX`, left half of the screen black under Renderer 1/2/3, correct under Renderer 0.
- Converting the SFF from PCX to PNG10, and from SFF v1 to v2, changed nothing.
- The bug was therefore assumed to be after `ReadV1`/`ReadV2`, in the upload or draw path.

## The original diagnostic trail

`Load8bitTexture()` was instrumented to log `w/h` plus a non-zero count (`nz`) and checksum of every upload:

```
[SSZ] [Tex] w=640 h=480 nz=0/307200    ck=00000000 f4=00000000 px=17ecf054   <- ALL ZEROS
[SSZ] [Tex] w=640 h=480 nz=307200/307200 ck=02d99931 f4=fcf4fcf4 px=17e4004c   <- VALID
```

This is the observation that produced the (now superseded) use-after-free theory. Its two premises were wrong: the all-zero buffer is a blank layer, and the black band was geometry, not pixels.

Also tried at the time, all without effect on the bug:

- `pxl.new(1)` → `pxl.new(w*h)` in `setPxl`'s GL path (expected — the allocation size was never the problem).
- `.sdl.SetPxlLog` / `LogPxBuffer` C++ bridges: registered successfully but never dispatched from script; the instrumentation approach was abandoned in favour of C++-side counters.
- `case 11,12` (`loadPngTexture`, bypasses `^ubyte px` entirely): not exercised, because this stage's sprites are all SFF-compressed PNG8 (`fmt=10`), never formats 11/12.

## Files that mattered

| File | Role |
|---|---|
| `main/sdlplugin/sdlplugin.cpp` `drawTileHolizon()` ~:7239 | defect 1 — the `tile.w <= 0` early return |
| `main/sdlplugin/sdlplugin.cpp` `RenderMugenGl()`/`RenderMugenGlFc()` ~:7540 / ~:7633 | defect 2 — the misdirected viewport clamp |
| `main/sdlplugin/sdlplugin.cpp` `GlSwapBuffers()` | deferred GL screenshot (`glReadPixels(GL_BACK)` before swap); verified correct, so black captures were real |
| `ssz_script/ssz/sff.ssz` `Sprite.draw()` / `Animation.draw()` | passes `tile={0,0,0,0}` and clamps the **window** rect (this clamp is correct — `rect` there is the clip window) |
| `ssz_script/ssz/bg.ssz` `BackGround.draw()` | computes the layer window and its own viewport clamp |
| `test_refnew_leak.cpp` | standalone proof for the unrelated `Reference::refnew`/`listnew` leak fix |
