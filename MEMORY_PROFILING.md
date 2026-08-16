# Memory Profiling

In-house memory profiling for the SSZ engine, answering: *"why does this build show
~200 MB in Task Manager when real M.U.G.E.N. sits at ~50 MB?"*

All numbers below are from the Release build (`make CONFIG=Release install`), demo config
(kfm vs kfm, `stages/stagez.def`, `script/main.lua` CPU-vs-CPU match, ~2 min).

---

## 1. TL;DR

Peak process footprint is **~174 MB working set / ~161 MB private bytes** after the
SFF/SND pixel caches (see §3.5). Before the fix it was **~227 MB / ~216 MB**. The
breakdown (post-fix):

| Where | Size | % of private | Notes |
|---|---|---|---|
| **SSZ-managed heap** (engine data model) | **~99 MB peak live** | **~61 %** | sprites keep pixel data as SSZ byte arrays; whole engine runs as SSZ objects (pixel buffers now shared across compilations) |
| SDL subsystems (window, software renderer, audio) | ~+33 MB at init | ~20 % | `POST-INIT` milestone |
| Lua runtime + scripts (common.lua, match.lua, screenpack) | ~+13–20 MB | ~9 % | included in the `LUA-COMMON` jump below |
| JIT machine code | 2.6 MB | ~2 % | **compile is NOT the memory hog** |
| JIT literal/global data (gre) | 0.3 MB | <1 % | |
| C++ engine internals, SDL surfaces, frame buffer | rest | ~8 % | |

Real M.U.G.E.N. keeps everything in C++ structs (~50 MB total). This engine implements
the whole game in a garbage-collected scripting heap (SSZ), so its object graph + sprite
pixel buffers ARE the footprint. **The script-layer data model is the memory.**

---

## 2. How to re-measure

No external tools. Run a match, then read the report printed at exit (also printed on
crashes via the VEH hook):

```bash
make CONFIG=Release install
cd install && rm -f game.log && ./ikemen.exe > game.log 2>&1   # play a representative match, quit
grep -A 30 "PROCESS MEMORY" game.log        # process-wide section (Task Manager numbers)
grep -A 30 "MEMORY USAGE RANKING" game.log  # per-phase SSZ-heap deltas + alloc-size histogram
```

### What each report section shows

| Section | Meaning |
|---|---|
| `MEMORY USAGE RANKING` | Per-phase **SSZ-heap** deltas (`MemMarkBefore/After` — allocator-tracked only) + `PEAK SSZ LIVE HEAP` + **alloc-size histogram** |
| `PROCESS MEMORY` | **Process-wide** working set / private bytes (what Task Manager shows) — peaks, milestone table with deltas, 1-sample/sec timeline |
| milestones | `PROCESS-START`, `PLUGINS-REGISTERED`, `PRE-COMPILE`, `POST-INIT`, `LUA-COMMON`, `LUA-MATCH-LOADED`, `LUA-PRE-SELECT`, `LUA-SELECT-DONE`, `CHAR-CODE-COMPILED`, `STAGE-LOADED`, `MATCH-LOADED`, `MATCH-START`, `MATCH-END`, `LUA-GAME-DONE`, `POST-SYSTEM-LOAD` |
| `JIT machine code` | total x86 code VirtualAlloc'd (cumulative) |
| `JIT literal data` | per-function gre/global buffers kept resident |

### Coverage caveats

- The SSZ heap counters (`g_allocBytes`/`g_freeBytes`) only cover allocations through
  `MemoryKakuho` (the SSZ `new`). SDL surfaces, Lua state, JIT code (`VirtualAlloc`),
  `std::vector` buffers are **not** in the SSZ-heap numbers — that's why the
  `PROCESS MEMORY` section exists.
- The alloc-size histogram is **cumulative** (allocs are not subtracted on free), so it
  shows what the heap churns, not what is live at any moment.
- `LOG_DEBUG` lines from the markers are compiled out in Release; milestones still record.

### Adding a marker

- **C++:** `MemMarkProcess("TAG")` for a process-wide snapshot; `MEM_MARK_BEFORE/AFTER(TAG)`
  for SSZ-heap deltas; per-second samples already run in `Flip()`.
- **Script (SSZ):** `.ssz.procMemMark("TAG")` (process) and `.ssz.memMarkBefore/After("TAG")`
  (SSZ heap). `ssz_script/lib/ssz.ssz` wraps them.
- **Script (Lua):** `procMemMark("TAG")` — registered into Lua by `ikemen.ssz::init()`.
- Add a counter anywhere with the `LOG_INFO("Memory", ...)` pattern; report hooks live in
  `main/main.cpp` `SafePrintRanking()`.

---

## 3. Measured profile (Release, kfm vs kfm)

### 3.1 Milestones — private bytes (what Task Manager shows)

| Milestone | ws (MB) | priv (MB) | Δpriv | Phase |
|---|---|---|---|---|
| PROCESS-START | 8.0 | 2.3 | — | exe + CRT baseline |
| PLUGINS-REGISTERED | 8.9 | 2.4 | +0.1 | 14 static plugins |
| PRE-COMPILE | 9.1 | 2.4 | +0.0 | |
| POST-INIT | 45.1 | 35.5 | **+33** | SDL init: window, software renderer, audio, fonts |
| LUA-COMMON | 114.7 | 105.0 | **+70** | common.lua builds engine object graph + screenpack sprites in SSZ |
| LUA-MATCH-LOADED | 128.0 | 118.4 | +13 | match.lua + lifebar def (SSZ heap +13.3 MB) |
| LUA-PRE-SELECT | 136.4 | 124.9 | +7 | loader.lua char/stage metadata |
| LUA-SELECT-DONE | 136.4 | 125.0 | +0 | |
| CHAR-CODE-COMPILED | 155.4 | 144.3 | **+19** | chara() sprites + StateBuilder + state compile (JIT = 2.6 MB) |
| STAGE-LOADED | 156.4 | 145.2 | +0.9 | |
| MATCH-START | 156.3 | 145.1 | −0.1 | |
| MATCH-START | 158.0 | 144.9 | −0.1 | |
| FIGHT-SETUP-DONE | 170.7 | 157.9 | **+12.7** | match setup: screenpack assets on the debug state — now **cached**, was +70 |
| **MATCH-END** | **174.0** | **161.1** | **+0** | **peak, up from 226.5 / 215.7 pre-fix** |
| LUA-GAME-DONE | 174.1 | 161.1 | +0 | |

Timeline during the fight: flat — no meaningful leak; the GC thread keeps up.
Pre-fix the match-time jump was **+70 MB** (MATCH-START 156 → MATCH-END 226);
post-fix it is **+16 MB** (158 → 174).

### 3.2 SSZ heap (allocator-tracked)

- **Peak live: 98.9 MB** (pre-fix: 153.2 MB).
- Cumulative churn over the session: **189 MB** allocated total (pre-fix: 250 MB).
- Lifebar alone: +13.3 MB live SSZ objects (`FIGHT data/lifebars/winmugen/fight.def`).
- Match-time screenpack rebuild on the debug state: **+12.9 MB** (pre-fix: +69.8 MB) —
  front-loaded at match start, not a per-frame leak.

### 3.3 Match-time +70 MB growth — full breakdown (SSZ heap, pre-fix measurement)

Markers added around `fighting.ssz::game()` setup, `debug-script.ssz::loadFile`,
`match.lua` top-level and `common.lua` top-level pin the whole match-time growth to
**one place**: the setup phase re-loads `match.lua` into the debug Lua state, which
re-runs `common.lua`, which re-runs `require("script.screenpack")` — creating a
**second copy of the screenpack** on the debug state.

| Piece | SSZ heap Δ |
|---|---|
| **FIGHT-SETUP total** (`fighting.ssz::game()` setup block) | **+70.0 MB** |
| ├ DSCRI-RUNFILE (running `match.lua`) | +70.0 MB |
| │ └ ML-COMMON (`assert(loadfile("script/common.lua"))()`) | +70.0 MB |
| │ │ └ **CL-SCREENPACK (`require("script.screenpack")`)** | **+69.8 MB** |
| │ │ └ CL-OPTIONS / CL-CMDS / CL-DATA | +96 KB |
| │ └ ML-PAUSE (`require("script.pause")`) | +9.5 KB |
| │ └ rest of match.lua top-level | +720 B |
| ├ DSCRI-REGISTER (engine fn registration) | +0 |
| ├ SETUP-CHARS (rootInit, life/power, copyVar) | +50 KB |
| ├ SETUP-STAGE (stage reset/action) | +0 |
| └ SETUP-RESET (cam init + reset) | +240 B |

Roundstate phases after setup are negligible: RS-0 (VS/intro) +704 B, RS-2 (fight)
+24 KB, RS-3 (KO) +104 B, RS-4 (round end) +0.

The same `CL-SCREENPACK` line allocates **+69.8 MB a second time at boot**
(`require("script.common")` → `require("script.screenpack")` on the main Lua
state). So ~140 MB of the ~153 MB peak SSZ heap is the screenpack object graph
**existing twice** — once per Lua state. The codebase is aware of this design
(common.lua header comment: "Every time that a match start, all lua data is
reseted, so this script is loaded there like first time that you start the
engine").

**Fix directions** (highest impact first):

1. ✅ **DONE — Share the heavy pixel data across compilations** (SFF sprite pixels +
   SND samples via a process-global cache, see §3.5). Knocked the match-time rebuild
   from +69.8 MB to +12.9 MB. The remaining +12.9 MB is mostly fonts (+11.8 MB), which
   are ~90 % per-compilation container objects — not worth caching.
2. Share one Lua state between menu and match instead of the separate debug state +
   full re-load — **tried, reverted**: SSZ object refs carry per-compilation type IDs
   (`DynRefGet` requires `typ == src.typ`), so the fight compilation can't consume the
   main state's `&Sff`/`&Sprite`/font objects. The pixel-level cache avoids this
   because `^ubyte` arrays use the fixed global `REF_TYPEID`.
3. Reduce the screenpack's own footprint (sprites loaded but never drawn, etc.)
   — shrinks both copies.

---

### 3.5 The implemented fix — SFF/SND pixel caches (process-global)

**Result: peak working set 227 → 174 MB (−53), match-time screenpack rebuild
+69.8 → +12.9 MB.** The match still runs in its own SSZ compilation + Lua state
(original architecture preserved); what changed is the *big* shareable data:

| Cache | What it holds | Match-time Δ pre→post |
|---|---|---|
| **SFFv2 sprite pixels** | decoded `^ubyte` pixel + palette arrays, keyed by file path + group/number + palette ID | +47.9 MB → **+1.0 MB** |
| **SND samples** | raw sample `^ubyte` arrays, keyed by file path | +10.1 MB → **+20 KB** |
| (fonts) | container objects dominate — cache reverted, negligible win | +11.8 MB → +11.8 MB |

**Why it's safe:** the shared buffers are `^ubyte` arrays (fixed global `REF_TYPEID`),
not per-compilation class objects — they cross compilation boundaries freely, and
both compilations keep their own small container objects on top. Key = file
path + sprite group/number + palette (SFF) or path (SND), so palette swaps / indexed
reads are still correct per compilation.

**Files:** `main/ssz/ssz.cpp` + `main/ssz_static.hpp` (`SffV2CacheGet/Put`,
`SndCacheGet/Put` — process-global `std::map`, key = path + IDs, value = `Reference`
holding the shared `^ubyte` array; registered as `dll/ssz.dll` plugin functions),
`ssz_script/ssz/sff.ssz` (`readV2` checks `SffV2CacheGet` before re-decoding,
`Sff::loadFile` passes the cache key), `ssz_script/ssz/sound.ssz` (`Snd::loadFile`
checks `SndCacheGet` before re-reading), callers updated in `script.ssz`, `char.ssz`,
`fight.ssz`.

**Dead ends along the way (all reverted):** sharing one Lua state between menu and
match (blocked by per-compilation SSZ type IDs, see §3.3), and a font cache
(+11.8 MB → −0.8 MB, fonts are ~90 % containers).

---

### 3.6 Alloc-size histogram (cumulative, what the heap churns)

| Size class | Volume | % of total | Allocs | Likely contents |
|---|---|---|---|---|
| 64 KB – 128 KB | **85.5 MB** | 34 % | 889 | **sprite pixel buffers** (`^ubyte` in `sff.ssz`) |
| 8 KB – 16 KB | 20.4 MB | 8 % | 1,552 | sprites, large strings |
| 16 B – 31 B | 16.3 MB | 7 % | 791,310 | engine object graph (refs, small structs) |
| 64 B – 127 B | 13.6 MB | 5 % | 156,457 | small objects |
| 256 KB – 512 KB | 17.1 MB | 7 % | 46 | full-screen sprites (640×480) |
| 128 KB – 256 KB | 10.6 MB | 4 % | 59 | large sprites |
| other small classes | ~86 MB | ~35 % | ~300 K | strings, arrays, objects |

Sprite pixel data (`^ubyte` SSZ arrays) is the single biggest allocation class. The SDL
software renderer blits **directly from those SSZ arrays** (no per-sprite SDL surface copy
in the main sprite path; the 64-entry blit texture cache is bounded), so there is no
double storage — the pixels simply live on the SSZ heap.

---

## 4. Findings

1. **The memory is the script-layer data model, not the assets or the compiler.**
   ~99 MB of ~161 MB private is the SSZ heap: the engine object graph + sprite pixel
   buffers. Real MUGEN's C++ engine fits the same job in ~50 MB total.
2. **The match-time growth was ONE thing: `require("script.screenpack")` running a
   second time.** `match.lua` re-loads `common.lua` into the debug Lua state
   (separate from the menu's state), which re-runs the screenpack load — the same
   ~70 MB object graph was built twice and both copies stayed live. **Fixed** by
   sharing the two big byte-array payloads (SFF pixels, SND samples) across
   compilations; the match-time screenpack rebuild is now +12.9 MB, almost all of
   it fonts. The fight, VS screen and lifebar remain cheap: RS-0 intro +704 B,
   RS-2 fight +24 KB, RS-3 KO +104 B, RS-4 round end +0; the lifebar cost
   (+13.3 MB) is paid at `loadLifebar()` before the match, not during it.
3. **The compile is cheap.** JIT machine code = 2.6 MB, literals = 0.3 MB. The +107 MB
   jump that visually coincides with "char code compile" is actually the
   Lua/common.lua engine init (screenpack, fight system) happening in the same window.
4. **SDL init costs ~+33 MB** (window, software renderer, audio) — inherent to SDL2.
5. **No leak during play** (~1.6 MB/min, GC keeps up).
6. **Sprite pixels dominate the heap's big allocations** (64–128 KB bucket = 34 % of
   churn). The SFF cache already stops the *duplicate* decode across compilations;
   further wins need reducing live sprite data (only loading sprites actually used,
   the `SaveMemory` config option, or rasterizing stage layers once).

---

## 5. Optimization candidates (ranked by expected impact on the 200 MB)

### M1 — Reduce live sprite pixel data (biggest bucket)

**Target:** `sff.ssz` `Sprite.pxl` (`^ubyte` arrays) + what keeps sprites resident.

**Why:** every loaded sprite's pixels sit on the SSZ heap (64–128 KB bucket = 34 % of
alloc volume). The screenpack + char + stage + lifebar hold hundreds of sprites live.

**Ideas:**
- Profile a real stage/char before optimizing; the demo (kfm + stagez) is small.
- `SaveMemory` in `save/config.ssz` (currently false) already skips RLE-decode for small
  sprites — measure its effect.
- Unload sprites that can't appear (e.g. char B sprites while char A is fighting) — the
  engine currently keeps both chars' full SFF resident.
- Stage layers with static content could render once to a cached surface instead of
  holding per-frame pixel arrays.

### M2 — (DONE) The match-time +70 MB SSZ-heap growth

The +70 MB was almost entirely the **second screenpack load** on the debug Lua state
(`require("script.screenpack")` re-building all sprites/sounds). Fixed via the
SFF/SND pixel caches (§3.5): match-time rebuild is now +12.9 MB (mostly fonts,
which are ~90 % per-compilation containers).

**Remaining target:** the +12.9 MB font/container wave at match setup. Ideas:
- The font data is dominated by container objects per compilation; sharing would
  need cross-compilation type-ID fixes in the SSZ compiler (out of scope).
- **`SaveMemory` measured: ~0.35 MB per SFF load (−2.4 MB peak), not worth the
  draw-time RLE cost.** It only affects the V1/PCX path, and the screenpack's
  memory is 99 % `ikemen.sff` (V2, already cached). Keep it `false`; it only
  helps legacy V1 char/stage SFFs.

### M3 — Verify with a real stage/char

The demo config understates production: a heavy stage + team battle will push the
64–128 KB sprite bucket and the match-time wave much higher. Re-run the milestone
workflow on the worst-case scene before changing code.

---

## 6. Files touched by the memory profiler (for reference)

| File | Change |
|---|---|
| `main/mem_profiler.hpp` | process memory sampler (GetProcessMemoryInfo), timeline/milestones/peaks, `MemPrintProcess()`, SSZ peak + histogram printing |
| `main/ssz/ssz.cpp` | `g_peakLiveBytes`, `g_jitCodeBytes`/`g_jitDataBytes`, alloc-size histogram, `ProcMemMark` plugin |
| `main/ssz/sszdef.h` | extern counters for JIT volume |
| `main/ssz/jitcompiler.hpp` | JIT code/literal volume counters |
| `main/ssz_static.hpp` | register `ProcMemMark` |
| `main/main.cpp` | milestone markers + `MemPrintProcess()` at exit |
| `main/sdlplugin/sdlplugin.cpp` | 1-sample/sec process memory in `Flip()` |
| `ssz_script/lib/ssz.ssz` | `procMemMark` wrapper |
| `ssz_script/ssz/ikemen.ssz` | `POST-INIT`/`POST-SYSTEM-LOAD`, `MATCH-START`/`MATCH-END` marks, Lua `procMemMark` registration |
| `ssz_script/ssz/loader.ssz` | `LOAD-CHAR`/`LOAD-COMPILE` marks, `CHAR-CODE-COMPILED`/`STAGE-LOADED`/`MATCH-LOADED` milestones |
| `ssz_script/ssz/fighting.ssz` | `FIGHT-SETUP`/`SETUP-LUA`/`SETUP-CHARS`/`SETUP-STAGE`/`SETUP-RESET` marks around the match setup block |
| `ssz_script/ssz/debug-script.ssz` | `DSCRI-REGISTER`/`DSCRI-RUNFILE` marks in `loadFile`; Lua-callable `memMarkBefore`/`memMarkAfter`/`procMemMark` for the debug Lua state |
| `lua_script/script/main.lua` | `LUA-COMMON`/`LUA-MATCH-LOADED`/`LUA-PRE-SELECT`/`LUA-SELECT-DONE`/`LUA-GAME-DONE` milestones |
| `lua_script/script/match.lua` | roundstate-phase markers (`RS-0`..`RS-5`), `ML-COMMON`/`ML-PAUSE`/`ML-REST` split of match.lua top-level |
| `lua_script/script/common.lua` | `CL-DATA`/`CL-CMDS`/`CL-SCREENPACK`/`CL-OPTIONS` split of common.lua top-level (fires at boot and at each match) |

### Files touched by the memory FIX (SFF/SND caches)

| File | Change |
|---|---|
| `main/ssz/ssz.cpp` | `SffV2CacheGet/Put`, `SndCacheGet/Put` — process-global caches keyed by file path + IDs, returning shared `^ubyte` References |
| `main/ssz_static.hpp` | register the four cache functions as `dll/ssz.dll` plugin functions |
| `ssz_script/ssz/sff.ssz` | `readV2` checks `SffV2CacheGet` before re-decoding; `Sff::loadFile` builds the cache key (file + group/number + palette) |
| `ssz_script/ssz/sound.ssz` | `Snd::loadFile` checks `SndCacheGet` before re-reading WAV bytes |
| `ssz_script/ssz/script.ssz`, `char.ssz`, `fight.ssz` | pass cache keys to `Sff::loadFile` / `Snd::loadFile` callers |
| `main/alert/alert.cpp` | mirror MessageBox errors to stderr so the log captures SSZ compile errors |
