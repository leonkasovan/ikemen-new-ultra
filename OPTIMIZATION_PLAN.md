# Performance Optimization Plan

Based on the session-profiling investigation of Ikemen GO (this repo) using the new
in-house time profiler. All measurements were taken from the demo configuration
(`data/select.def` with kfm vs kfm, `stages/stagez.def`, CPU vs CPU auto-match)
on a Windows VM running the w64devkit **Release** build unless noted.

---

## 1. Summary

The engine is **not compute-bound**. A 60 fps frame (~16.7 ms) spends:

- **~9.0 ms** waiting for the frame tick (`cmd.update()` → `se.event(60)` → `SDL_Delay`) — deliberate pacing, idle
- **~5.4 ms** in `script:draw` (sprite orchestration)
- **~0.9 ms** in character state/AI (`charAction`)
- **~0.2 ms** in the per-frame Lua callback
- **~0.1 ms** in everything else marked
- **~0.15 ms** actual C++ rendering (all sprites, shadows, fills, flip combined)

**The one genuine hotspot is the stage background:** 5 parallax layers × ~0.78 ms
each ≈ **3.9 ms/frame, ~72 % of all draw time**. Everything the user thinks of as
"the fight" (characters, AI, shadows, blitting) is cheap.

---

## 2. Profiling toolchain (how to re-measure)

No external tools required. The engine prints a ranked report at exit:

```
[Time] ==== TIME USAGE RANKING ====
[Time]   region ... total(ms) calls avg(ms)
```

- **C++ side:** `TIME_SCOPE(tag)` / `TIME_MARK_BEFORE|AFTER(tag)` macros from
  `main/time_profiler.hpp`; report printed by `TimePrintRanking()` at exit/crash
  (hooked next to `MemPrintRanking()` in `main.cpp`).
- **Script side:** `.ssz.profBegin(:"tag":)` / `.ssz.profEnd(:"tag":)` (see
  `main/ssz/ssz.cpp`, registered as SSZ plugin functions).
- **Always-on frame totals** in `renderer.h` `perfFrameEnd()` — script, sprites,
  shadows, fills, flip, accumulated every frame.
- Existing marks cover the fight loop phases (`fighting.ssz`), the event pump
  (`command.ssz`), the whole `draw()` (`char.ssz`), and the stage background
  (`stage.ssz` + per-layer `bg.ssz`).

**Workflow:**

```bash
make CONFIG=Release install     # or Debug
cd install && ./ikemen.exe > game.log 2>&1   # play a representative match, quit
grep "TIME USAGE RANKING" -A 30 game.log
```

Add a mark anywhere with `profBegin`/`profEnd` (scripts) or `TIME_SCOPE` (C++),
re-copy the script (`cp ssz_script/ssz/*.ssz install/ssz/`), no rebuild needed for
script-only changes.

### Measurement caveats

| Caveat | Impact |
|---|---|
| VM `QueryPerformanceCounter` overcounts ~40–46× | `frame:*` rows are **proportions only**; `script:*` and `ssz:*` use `steady_clock` and are exact |
| Debug build emits printf per render call | Inflated `script:draw` 2.7 → 5.7 ms; always profile Release for real numbers |
| Run-to-run variance on the VM | `script:draw` measured 2.7–5.6 ms; trust **proportions**, re-run 2–3× |
| Demo stage/chars only | Layer count and costs are stage-specific |
| ~58 fps vs 60 fps target | The tick wait absorbs slack; not a problem |

---

## 3. Measured baseline (Release, steady_clock rows)

| Region | avg/frame | Share of frame | Notes |
|---|---|---|---|
| `script:cmdUpdate` (tick wait) | 9.02 ms | 54 % | `SDL_Delay` — idle pacing, **not work** |
| `script:draw` | 5.45 ms | 33 % | all draw orchestration |
| ├ `draw:bg` (stage background) | 3.93 ms | 24 % | **the hotspot** |
| │ ├ `bgDraw:layer` ×5 layers | 0.78 ms each | — | per-layer parallax math + sprite call |
| │ └ `bgDraw:setup` | ~0.005 ms | — | free |
| ├ `draw:fighters` (3 layers) | 0.54 ms | — | characters are cheap |
| ├ `draw:anims` (2 passes) | 0.46 ms | — | explods/helpers |
| ├ `draw:shadows` | 0.29 ms | — | |
| └ `draw:overlay`/`faceSetup`/`fg` | ≤0.02 ms | — | free |
| `script:charAction` (state/AI/physics) | 0.85 ms | 5 % | |
| `script:luaLoop` | 0.19 ms | 1 % | |
| `stageAction`, `hotkeys`, `roundCheck`, `camMath`, `frameAdvance` | ≤0.02 ms each | <1 % | |
| C++ render: sprites / shadows / flip / fills (real, QPC÷42) | 0.12 / 0.003 / 0.02 / ~0 ms | ~1 % | blitting is free |
| `ssz:compile` (once) | 0.7 s | — | non-issue |

**Frame budget:** ~7.5 ms work + ~9 ms pacing ≈ 16.5 ms ≈ 60 fps. ~55 % idle.

---

## 4. Findings

1. **No hidden hotspot.** After marking every phase of the fight loop, 90 % of the
   session time is accounted for, and the remainder is pacing.
2. **The background, not the fight, is the draw cost.** 72 % of draw time is the
   stage's 5 parallax layers. It scales linearly: ~0.78 ms per visible layer.
3. **Per-layer math, not layer overhead.** The `bgDraw` setup (~50 lines) and the
   layer loop are effectively free (0.005 ms + 0.01 ms). The cost is inside
   `BackGround::draw` (bg.ssz:413): delta-scale/sin-offset/window-rect float math
   plus a sprite-draw plugin round-trip per layer.
4. **Characters, AI, physics, Lua, shadows: all cheap.** Not optimization targets.
5. **The C++ renderer is free.** Even the software blit path is ~0.1 ms/frame.
6. **Startup compile is a non-issue** (~0.7 s, once).

---

## 5. Optimization candidates (ranked by expected impact)

### P1 — Cache/hoist the per-layer background transform math

**Target:** `BackGround::draw` in `ssz_script/ssz/bg.ssz` (~line 413).

**Why:** 5 layers × 0.78 ms = 3.9 ms/frame. Cutting this in half saves ~2 ms/frame
(≈12 % of frame budget; ~40 % of non-idle time).

**Ideas (in order of simplicity):**

1. **Recompute only when inputs change.** The per-layer math depends on camera
   (`x`, `y`, `scl`), scroll deltas, sin-offset time, and layer params. When the
   camera is static (most frames are a static camera), cache the last computed
   rect/position and reuse it. Add an `&StageBG`-level dirty flag: camera moved /
   layer anim ticked / bgctrl changed.
2. **Hoist invariant sub-expressions.** `sclx`, `scly`, `wsclx`, `wscly`,
   `bgscl*localscl*yscale` etc. are recomputed per layer despite depending only on
   camera state — compute once in `bgDraw` and pass them in (or precompute into
   the layer struct when the camera changes).
3. **Fuse the sin/raster offset math.** `rasterxbspeed`, `sinxlooptime`,
   `sinxoffset` produce per-layer per-frame trig; batch/quantize when values are
   unchanged.

**Verification:** re-run with `profBegin` marks; expect `script:bgDraw:layer` avg
to drop below 0.78 ms.

### P2 — Reduce visible background layers

**Target:** `stages/stagez.def` (demo) and stage assets in general.

**Why:** cost scales linearly with visible layers (5 layers = 3.9 ms; 2 layers ≈
1.6 ms — saves ~2.3 ms/frame).

**Ideas:**

- Merge low-motion layers (sky + distant buildings) into a single pre-rendered
  layer.
- Static layers with `delta = 0` can be drawn to a cached surface once and blitted
  (they never move) — removes per-frame math entirely.
- Profile any real stage before optimizing; the demo stage may not represent
  production stages.

### P3 — Cut plugin round-trip overhead in the draw path

**Target:** the SSZ→C++ bridge used by every sprite draw.

**Why:** each layer + sprite draw crosses the plugin boundary multiple times
(rect conversions, refs). 27,100 layer draws + ~60–100 sprite draws/frame means
~400+ bridge crossings/frame.

**Ideas:**

- Batch sprite submissions: collect rect/transform params in script, submit one
  array per frame (see `RenderFontBatch` for the existing batching pattern).
- Only worthwhile if P1/P2 are insufficient; the absolute cost is small but
  multiplies with sprite count.

### P4 — Character draw orchestration (defer)

**Target:** `draw:fighters` (0.54 ms) + `draw:anims` (0.46 ms).

**Why:** ~1 ms/frame total, 10× cheaper than the background. Only revisit if P1–P3
land and this becomes the largest remaining term. Same batching approach as P3.

### Non-targets (measured cheap — do not spend time here)

- `charAction` (state machine, AI, physics): 0.85 ms
- Lua loop callback: 0.19 ms
- All C++ rendering: ~0.15 ms
- Frame pacing (`cmd.update`): by design, idle
- Startup compile: 0.7 s once

---

## 6. Recommended experiments

1. **Static-camera cache (P1.1).** Add a dirty check to `BackGround::draw`; the
   demo stage camera is static for most of a round → expect the biggest win.
2. **Layer-count scaling check (P2).** Make a copy of `stagez.def` with 2 visible
   layers and confirm `script:bgDraw:layer` calls drop to 2×frames and total bg
   time ~halves. This validates the 0.78 ms/layer model.
3. **Real-hardware confirmation.** The VM swings absolute numbers run to run and
   its QPC is unreliable. Before/after comparisons should use **ratios** and the
   same hardware.
4. **Worst-case scene test.** A stage with many layers + 2-player team battle with
   explods will bound the worst-case frame.

---

## 7. Re-measurement workflow (before/after)

```bash
# 1. build + install Release
export PATH="/c/x86devkit/bin:$PATH"
make CONFIG=Release install

# 2. script-only change? copy without rebuilding:
cp ssz_script/ssz/*.ssz install/ssz/

# 3. run the identical CPU-vs-CPU match for ~2 min
cd install && rm -f game.log && ./ikemen.exe > game.log 2>&1
# quit / wait for match end

# 4. extract the report
grep "TIME USAGE RANKING" -A 30 game.log
```

Compare the `script:*` rows (exact) — especially `script:bgDraw:layer` (per-layer
cost) and `script:draw:bg` (total). Use `avg` per call and per-frame totals, not
the QPC-based `frame:*` rows.

---

## 8. Files touched by the profiling toolchain (for reference)

| File | Change |
|---|---|
| `main/time_profiler.hpp` | **new** — TIME_SCOPE/TIME_MARK/TimeAccumulateMs/TimePrintRanking |
| `main/main.cpp` | print time ranking at exit/crash |
| `main/sdlplugin/renderer.h`, `sdlplugin.cpp` | per-frame session totals; un-gated frame tracking |
| `main/ssz/ssz.cpp`, `main/ssz_static.hpp` | `ssz:compile`/`ssz:run` scopes; `ProfBegin`/`ProfEnd` plugins |
| `ssz_script/lib/ssz.ssz` | `profBegin`/`profEnd` script wrappers |
| `ssz_script/ssz/fighting.ssz` | fight-loop phase marks + `lib ssz` import |
| `ssz_script/ssz/command.ssz` | `script:cmdUpdate` mark + `lib ssz` import |
| `ssz_script/ssz/char.ssz` | `draw()` sub-phase marks (bg/fighters/anims/shadows/overlay/…) |
| `ssz_script/ssz/stage.ssz`, `bg.ssz` | `bgDraw` setup/layers + per-layer marks (+`lib ssz`) |
