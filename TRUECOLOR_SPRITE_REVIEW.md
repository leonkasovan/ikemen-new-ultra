# Truecolor Sprite Path — GL Review & SW/DirectX Plan

**Scope:** How 24/32-bit truecolor sprites (SFFv2 fmt 11/12 PNG) flow through
each renderer, what works, what's broken, and the implementation plan to close
the gap for Renderer 0/4.

**Test file:** `install/stages/SSF2THDR_EHonda_Stage.sff` (HD stage, truecolor PNG sprites)

---

## 1. Current State Per Renderer (TESTED)

| Renderer | Truecolor (fmt 11/12) | Paletted (fmt 0–4, 10) | Mechanism |
|---|---|---|---|
| 1 (GL 2.1) | ❌ **black bg** | ✅ working | Legacy ARB shaders + GL_LUMINANCE/GL_RGBA textures |
| 2 (GL 3.3) | ❌ **black bg** | ✅ working | GLSL 330 + palette atlas + GL_RGBA textures |
| 3 (GL 4.6) | ❌ **black bg** | ✅ working | GLSL 460 + palette atlas + GL_RGBA textures |
| 0 (SDL2 SW) | ❌ **crash** | ✅ working | 8-bit indexed software blitter (`mRender` → `mzlLoop`) |
| 4 (DirectX) | ❌ **crash** | ✅ working | Same software blitter (D3D11 present via SDL2) |

---

## 2. GL Truecolor Path (Renderers 1/2/3) — Debug Findings

### 2.1 Texture Loading: ✅ CONFIRMED WORKING

Log (`ikemen-opengl3.3.log:199-212`) proves all 12 PNG textures load with valid texids:
```
LoadPngTexture: 2815x764 -> texid=794
LoadPngTexture: 384x1148 -> texid=795
LoadPngTexture: 128x256 -> texid=2147487579  (atlas)
LoadPngTexture: 6800x640 -> texid=806
```

### 2.2 Dispatch Chain: ✅ CORRECT

```
stage.ssz:712     → bg[i].draw(...)
bg.ssz:475        → anim.draw(...)     (Anim.draw from sff.ssz)
sff.szs           → glDraw() checks rle == -12 → RenderMugenGlFc
sdlplugin.cpp     → fullcolor shader (GLSL 330/460 or legacy ARB)
```

All links verified in source. `bg.ssz:475` correctly calls `anim.draw()`.
`glDraw()` at `sff.ssz:668` checks `cond `rle == -12`` and dispatches to
`RenderMugenGlFc`. The `RenderMugenGlFc` function exists at
`sdlplugin.cpp:6844` and is properly registered.

### 2.3 Rendering: ❌ BLACK — ROOT CAUSE UNKNOWN

The textures load, the dispatch chain is correct, but the background renders
as solid black. Characters (paletted kfm.sff) render correctly on the same
frame. This isolates the issue to the truecolor rendering path.

**Possible causes (need runtime debugging to confirm):**

| # | Hypothesis | How to verify |
|---|---|---|
| 1 | Fullcolor shader `a` (alpha) uniform = 0 → sprites transparent against black bg | Add `INIT_LOG` in `RenderMugenGlFc` logging the `alpha` param |
| 2 | Texture data is all zeros (PNG decoded to black) | Add `glGetTexImage` after upload, check pixel values |
| 3 | `mul` uniform = (0,0,0) → multiplies color to black | Log the `mulr/mulg/mulb` params in `RenderMugenGlFc` |
| 4 | `glSpriteBegin()` returns false (culling) — sprites off-screen | Add log in `glSpriteBegin` for rle==-12 sprites |
| 5 | `RenderMugenGlFc` not called at all (dispatch skips it) | Add counter/log at function entry |

**Recommended debug step:** Add `INIT_LOG("RenderMugenGlFc: texid=%u alpha=%d mul=(%f,%f,%f) rect=(%d,%d,%d,%d)", ...)` at the top of `RenderMugenGlFc`. Run with the HD stage and check if the function is called and what values it receives.

### 2.4 GL Path Architecture (verified correct)

- `readV2` fmt 11/12: `rle = -12`, `loadPngTexture()` → RGBA GL texture
- `glDraw()`: `rle == -12` → `RenderMugenGlFc` (fullcolor shader)
- Fullcolor shader: samples RGBA texture (unit 0), applies PalFX (neg, color, add, mul, alpha)
- `RenderMugenGlFcS`: solid-color shadow using same RGBA texture
- `setFcPalFx()`: converts PalFX to per-sprite uniforms
- Works on GL 2.1 (legacy ARB), GL 3.3 (core GLSL 330), GL 4.6 (core GLSL 460)
- Atlas + texture pool + palette atlas integration

---

## 3. Software/DirectX Path (Renderers 0/4) — Crash Analysis

### 3.1 Current behavior

Phase 1 truecolor blitter was implemented (by another agent) but **crashes
when loading PNG** on Renderer 0. The implementation includes:

- `DecodePNG32()` (`sdlplugin.cpp:2730`): decodes PNG to RGBA `vector<uint8_t>`
- `TruecolorImg` struct (`sdlplugin.cpp:3325`): reads 4 bytes/pixel
- `mzlLoop<TruecolorImg, ccp>` specializations (`sdlplugin.cpp:4419-4662`)
- Dispatch in `mRender` for `rle == -13` (`sdlplugin.cpp:5393`)
- SSZ: `rle = -13`, `decodePNG32` call (`sff.ssz:349-350`)

### 3.2 Crash cause analysis

| # | Issue | Status |
|---|---|---|
| 1 | `TruecolorImg::nextPixel()` reads 4 bytes without checking `data+4 <= end` mid-row | ✅ **FIXED** — bounds check added at both row advance and per-pixel |
| 2 | `DecodePNG32` C++ impl `(FILE*, int32_t*, int32_t*, vector&)` vs static header `(PluginUtil*, FILE*, int32_t*, int32_t*, Reference*)` — **ABI mismatch** | ❌ Needs investigation — but `DecodePNG8` has the same pattern and works, suggesting the registry calls the correct pointer |
| 3 | Large HD sprites (6800x640 = 17 MB decoded) may cause allocation failure | Possible — needs testing |
| 4 | `TruecolorImg::skip(n)` may advance `data` past `end` for large `n` values | Possible — needs bounds check in `skip()` |

### 3.3 Fixes applied

- `TruecolorImg::nextPixel()`: added `data + 4 > end` check at both row advance and per-pixel read
- `LoadPngTexture`: added `INIT_LOG` for texture dimensions and texid (GL diagnosis)

---

## 4. Implementation Plan (Updated)

### Phase 1 — Basic truecolor blitter — **IMPLEMENTED, HAS CRASH**

The other agent implemented:
- `DecodePNG32()`: PNG → RGBA vector
- `TruecolorImg` struct with `nextPixel()` reading 4 bytes/pixel
- `mzlLoop<TruecolorImg, mTrans/mAddTrans/mSubTrans/mAlphaTrans>` specializations
- Dispatch in `mRender` for `rle == -13`
- SSZ: `rle = -13`, `decodePNG32` call

**Status:** Crashes on Renderer 0 with HD stage. Bounds check added for
`nextPixel()` but crash may also be in `DecodePNG32` or in the blit loop for
very large sprites. Needs further debugging.

### Phase 2-5 — Same as original plan

Zoom, alpha modes, rotation+tiling, shadow — not yet implemented.

---

## 5. What Go Does Differently

Go has **no software blitter** — all rendering is GPU. So truecolor is trivial:

```
Sprite.SetRaw(data, w, h, depth) → pendingData stored
Sprite.ensureTex() → GPU texture created lazily on first render
```

The GPU handles 8-bit indexed (via palette texture) and 32-bit RGBA uniformly.
Go's `readV2` just calls `png.Decode` and stores the result — no format
switching, no renderer conditionals.

---

## 6. Recommended Next Steps

1. **Test Renderer 0 with the bounds fix** — the `TruecolorImg::nextPixel()`
   OOB read fix may resolve the crash
2. **Test GL with the debug logging** — the `LoadPngTexture` log will confirm
   textures are loading; if they are, add logging to `RenderMugenGlFc` to trace
   the draw call
3. **If GL still shows black after confirming textures load**, add a debug
   color override in the fullcolor shader (e.g., `FragColor = vec4(1,0,0,1)`)
   to verify the shader is executing and the sprite is positioned correctly
4. **Check `glSpriteBegin()` return value** for truecolor sprites — if it
   returns false (off-screen culling), the sprites won't be drawn
