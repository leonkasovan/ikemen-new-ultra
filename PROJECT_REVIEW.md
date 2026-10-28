# Project Review — ikemen-new-ultra (branch `optimized`, HEAD `b436f5d`)

**Reviewed:** 2026-10-28
**Scope:** whole repository — `main/` (C++ engine + SSZ JIT), `ssz_script/`, `lua_script/`, build system, docs, tests.
**Method:** read-only. Static reading + targeted greps of the large files, one `-fsyntax-only` compile of `main/ssz/bridge.cpp` under the project's own flags, and one execution of the existing test binary. `external/` (19 vendored C libraries) was excluded from the code review, but the way the Makefile builds them was reviewed.
**Verdict:** an unusually ambitious and well-instrumented engine fork with a genuinely interesting architecture (static plugins + hand-rolled JIT), undermined by a thin safety net: **no CI, one 44-assertion test file for ~65k lines of product code**, and a handful of memory-safety bugs on the asset-loading and script-array paths that are reachable from untrusted content.

---

## 1. Snapshot

| Area | Size |
|---|---|
| C++ in `main/` | 40,783 lines / 152 files (14 plugins + SSZ runtime) |
| `main/ssz/jitcompiler.hpp` | 8,888 |
| `main/ssz/sourcetree.hpp` | 8,591 |
| `main/sdlplugin/sdlplugin.cpp` | 7,526 |
| `main/ssz/x86.hpp` (hand-written emitter) | 3,680 |
| SSZ scripts | 40,531 lines / 45 files (`statebuilder.ssz` alone 9,333) |
| Lua scripts | 24,508 lines / 15 files (`screenpack.lua` 4,749) |
| Build system | one hand-written 32 KB Makefile, no CMake, no CI |
| Tests | `test/test_file.cpp` (372 lines, 44 assertions, file I/O only) |
| Vendored deps in git | 6,836 tracked files (19 C libraries) |

Branches in flight: `optimized` (current), `main`, `native`, `asmjit`, plus `stable` (empty/unborn). `optimized` is 53 commits ahead of `main`; `asmjit` and `native` each diverge by 40–50 commits with no recorded decision on which JIT strategy wins. That is a strategic question sitting unanswered in the repo (`ikemen_x64_jit_research.md` acknowledges the 32-bit constraint).

---

## 2. What is genuinely good

1. **The static plugin architecture is the right call.** 14 subsystems, each with a `main/<name>_static.hpp` registration table, all wired once in `main/main.cpp:356-410` with per-plugin failure handling. The dirs, headers, one-`.cpp`-each, one `.ssz` shim each, includes and registration calls are all mutually consistent — **no orphans, no missing entries** (verified against disk). DLL loading is gone for engine plugins (only optional `libvlc.dll` remains, `sdlplugin.cpp:2206`). The registry is 173 self-documenting lines and resolves the `<dll/…>` names that scripts still use. The one structural gap: the registry holds no plugin *list*, so `main.cpp`'s 14 hand-written `if (!x_static_register())` blocks are the only authority — adding a 15th plugin that forgets its registration call fails silently at first script use.
2. **Instrumentation is better than most commercial engines.** `mem_profiler.hpp` + `time_profiler.hpp` give pprof-style ranked reports at exit, plus a VEH/TLS crash hook. The SSZ-level `.ssz.profBegin/End` markers let script authors instrument the fight loop without rebuilding. `PROGRESS.md` and `OPTIMIZATION_PLAN.md` show measured before/after numbers rather than guesses.
3. **The documentation of past work is excellent.** `AUDIO_REVIEW.md`, `INPUT_REVIEW.md`, `TRUECOLOR_SPRITE_REVIEW.md`, `ISSUES.md` each state mechanism, correction of earlier wrong hypotheses, and fix status. That is a rare and valuable habit (see §6 for where it has drifted).
4. **Reproducible from source.** Everything builds from vendored sources with one toolchain; no system packages, no downloads at build time (`install` may download one zip; see §5).
5. **`main()` is clean and readable** — short, linear, explicit error path per subsystem. The bridge/shim layer that existed during the DLL removal is documented in `bridge.cpp`'s header comment instead of being left as a mystery.

---

## 3. Critical findings (fix before shipping anything)

### C1 — Integer-overflow → heap overflow in both PNG decoders (asset-triggered)
`sdlplugin.cpp:2690-2691` (palette) and `sdlplugin.cpp:6516` (truecolor):

```cpp
out.resize(height * width);                                        // 2690
if (out.size() < (size_t)(height * width)) out.resize(height * width);  // 2691  ← "guard", same wrapped product
...
auto buff = new png_byte[width * height * 4];                       // 6516
```

`width`/`height` are `png_uint_32`; the products are computed in 32-bit and wrap. libpng's default user limits allow 1,000,000 × 1,000,000 per side and impose no total-pixel cap, so a small, highly-compressible 65536×65536 palette PNG makes `height*width == 0`, allocates nothing, and then `png_read_image` writes 4 GiB into it. The "guard" on 2691 compares against the same wrapped value and can never fire. Any `.sff`/`.png` inside a downloaded character or stage pack is an attack surface.
Fix: 64-bit math (`(uint64_t)width*height`), an explicit pixel-count ceiling, `png_set_user_limits()`, and check the multiplication *before* `resize`/`new`.

### C2 — libpng has no error handler on the truecolor path
`LoadPngTexture` (`sdlplugin.cpp:6484-6560`) never installs `setjmp(png_jmpbuf(...))`, unlike `DecodePNG8` (`2678`). A malformed PNG makes libpng `longjmp` through an uninitialized `jmp_buf` — undefined behaviour, in practice a jump to a garbage address. The `png_create_read_struct` failure path at `6500-6505` also leaks `png_ptr` handling and reports success-shaped garbage (`*w,*h` printed from uninitialized values at `6560` if `png_get_IHDR` fails).

### C3 — The GL clip rectangle is silently dead on the modern renderer
`glSpriteBegin` (`sdlplugin.cpp:7102-7156`):

```
7117:  gl33BatchFlush();            // ← pending batch drawn HERE
7153:  glEnable(GL_SCISSOR_TEST);
7154:  glScissor(dstr->x, g_h - (dstr->y + dstr->h), dstr->w, dstr->h);
7161:  glDisable(GL_SCISSOR_TEST);  // glSpriteEnd(), after every sprite
```

`gl33BatchFlush` (`sdlplugin.cpp:1453-1468`) issues `glDrawArrays` and sets no scissor, and `glSpriteEnd` disables scissor after each sprite. Every flush therefore runs with `GL_SCISSOR_TEST` **off**, so GL 3.3/4.6 never clips to `dstr`. GL 2.1 (immediate mode) does clip correctly — a hard backend divergence, and the `glFlushByScissor` counters at `7121-7124` measure state churn that never takes effect.
Fix: set the scissor *before* the flush decision, flush when the scissor rect changes, and drop the per-sprite `glDisable` (or split the batch by scissor rect).

### C4 — The crash handler can deadlock or corrupt its own output
`main/main.cpp:37-75`: a VEH (`AddVectoredExceptionHandler(1, MemProfilerVEH)`, `main.cpp:331`) calls `SafePrintRanking()` → `MemPrintRanking()` + `TimePrintRanking()`, which `std::sort` a `std::vector` of `std::string` and `printf`. Two problems:
- If the fault was heap corruption (exactly the case these tools exist to diagnose — see C5, C1), the handler re-enters a corrupt allocator and hangs or faults again. A crash becomes an unattributable hang.
- The TLS callback (`main.cpp:66-73`) fires at `DLL_PROCESS_DETACH`, after namespace-scope destructors for `g_memEvents`/`g_timeSamples` have run on normal exit, so it touches destroyed vectors (the `g_memRankingPrinted` guard does not prevent this ordering, it only prevents double printing).
Fix: snapshot the report into a fixed-size array at mark time, and in the handler write with `WriteFile` on a preallocated buffer — no allocation, no STL. Consider dropping the VEH in favour of minidumps plus a single atexit report.

### C5 — Indexing an empty SSZ array returns a pointer *before* the buffer
`arrayandref.hpp:172-179` (`Reference::refat`, and the same clamp copied into `HeapObj::at`, `arrayandref.hpp:43-49`):

```cpp
le  = (uintptr_t)length / (uintptr_t)tsize;
tmp = (i>>31) | (((le-1)-i)>>31);
i   = (~tmp & i) | (tmp & (le-1));      // le == 0  →  i == -1
return obj()->head.data + position + i*tsize;   // data[-tsize]
```

Bounds are clamped, never reported, so scripts cannot detect an out-of-range index; and for an **empty** reference the clamp yields element `-1`, i.e. the last byte(s) of `HeapObjHead` (`arrayandref.hpp:5-11`: `data`, `datasize`, `mutex`, `refcount`). A store through that pointer corrupts heap bookkeeping. This is on the JIT load/store path (`jitcompiler.hpp:541-544`, `RefAt`).
Fix: return element 0 for empty references (matching the clamp contract) or make the JIT emit an explicit bounds error — either way, never return `data - tsize`.

### C6 — SSZ exception handling and source-line mapping are compiled out of the shipped build
`sszdef.h:14` defines `SSZ_EXCEPTION` on `_WIN32`, but the real macros are `#ifdef _MSC_VER` (`sszdef.h:139-149`); the supported toolchain is MinGW g++ (`Makefile:18`). Result: all **61** `SSZ_TRY`/`SSZ_EXCEPT` sites degrade to bare braces, and the `g_cal` address→line map built at `jitcompiler.hpp:8870-8872` is dead code. A JIT fault in a script yields a raw crash with no script filename/line — while the code reads as if that feature exists. Combined with C4 this is the difference between "5-minute bug report" and "unreproducible crash".
Related: `VirtualProtect(..., PAGE_EXECUTE, ...)` at `jitcompiler.hpp:8876` ignores its return value; on failure the block stays `PAGE_READWRITE` and is executed anyway.

---

## 4. High / medium findings

| # | Severity | Finding |
|---|---|---|
| H1 | High | **`sprintf` into a 128-byte buffer from a 512-byte source.** `sdlplugin.cpp:1530`: `sprintf(g_rendererInfo.driverInfo /*char[128], renderer.h:199*/, "%s %d.%d.%d.%d", adapterId.Description /*char[512]*/, …)`. A long D3D9 adapter name overflows the field. The surrounding function uses `strncpy(..., size-1)` correctly elsewhere (`1478-1481`) — this one line is the outlier. |
| H2 | High | **Resource creation results are unchecked in the main init path.** `sdlplugin.cpp:1849-1853`: `SDL_CreateRenderer` and `SDL_CreateTexture` results are used unconditionally ( `SDL_SetTextureBlendMode(NULL,…)`, `lockTarget()`), yet "SDL renderer created" is logged regardless → silent black screen instead of a diagnosable error. Same pattern at `905`, `940`, `1610`, `1641`, `1752`; `SDLLockTexture` result ignored (`366-370`); `TTF_RenderUNICODE_Blended` result dereferenced without a NULL check (`3086-3089`). |
| H3 | High | **Window resize is half-implemented.** `WindowSize` (`sdlplugin.cpp:2049-2054`) has `g_w = width; g_h = height;` **commented out**, and `UpdateGLViewport` (`2102-2118`) handles only `SDL_WINDOWEVENT_MAXIMIZED`, overwriting `g_w/g_h` with window pixels (not the logical game resolution used by the ortho projection (`gl33SetFrameProjection`, `1325-1350`; legacy `glOrtho(0, g_w, 0, g_h, …)`, `6685`/`7184`) and by `glScissor(g_h - …)` at `7154`). Un-maximizing is never handled, so the coordinate space stays wrong for the rest of the session. |
| H4 | Medium | **The profiler taxes what it measures, always.** `g_profilerLog` (default `false`, `ssz.cpp:128`) gates only *printing*. Recording is unconditional: 25 `profBegin/End` markers per frame from scripts, each doing `Reference → std::wstring` heap allocation + an `unordered_map` insert/erase (`ssz.cpp:874-889`) plus `TimeAccumulateMs`'s **linear scan with `std::string` comparison** (`time_profiler.hpp:44-58`). In the `bgDraw:layer` loop this multiplies by layer count. For an engine whose whole `OPTIMIZATION_PLAN.md` is about frame time, the measurement apparatus is part of the measurement. Fix: gate recording on `g_profilerLog`, and key samples by interned pointer instead of `std::string`. |
| H5 | Medium | **`MemRecord` grows without bound.** Every `MEM_MARK_*` appends a `MemorySnapshot` (with a `std::string`) that is never cleared (`mem_profiler.hpp:118-126`). Long sessions accumulate; marks inside reload/music paths (`ogg.cpp:96`, `sdlplugin.cpp:3165`) are per-load, so it is slow growth rather than per-frame. |
| H6 | Medium | **Per-heap-object `CRITICAL_SECTION` on a single-threaded interpreter.** `HeapObjHead` (`arrayandref.hpp:5-11`) carries a `LPCRITICAL_SECTION`, lazily allocated per object via `sszrefnewfunc` and spin-waited with `Sleep(0)` (`22-33`). The interpreter is single-threaded; only the GC thread (`compiler-state.hpp:80-92`) needs synchronization, and it is started/joined per `run()`. This costs an allocation + 40-odd bytes per heap object, per-object lock traffic, and hard-wires the runtime to Win32 (POSIX build has no equivalent — see §5). |
| H7 | Medium | **The Lua layer is called from the SSZ fight loop every frame.** `ssz_script/ssz/fighting.ssz:667-670`: `getGlobal("loop")` + `pcall(0,1)` per frame, purely for the Lua debug/hotkey layer. That is a string hash lookup plus a full protected call in the frame path; the SSZ JIT exists to avoid exactly this. Either hoist the function reference once, or move the debug-script hooks into SSZ. |
| H8 | Medium | **Compile errors are silently dropped after the first run.** `CompilerState::run()` sets `ss.emes.appendable = false` permanently (`compiler-state.hpp:85`); `ErrMes::Add` then early-returns, so a second `compile()` on the same state (reachable from script — `ssz_script/lib/ssz.ssz` reload helpers) returns an **empty** error string. `compileSub1` also never clears `emes`, so stale errors can be re-reported. In a project whose only debugging story is console output, this is a bad failure mode. |
| H9 | Medium | **Plugin-resolution errors are undiagnosable.** `sourcetree.hpp:7003-7012`: `loadDll()`'s `bool` is discarded, and a missing library and a missing function both produce the same message — `"Failed to load plugin."` — with no library or function name. With 183 `<dll/…>` references across the scripts (`<dll/ssz.dll>` ×19, `<dll/file.dll>` ×18, …) and no DLLs present, this is the message a new contributor will hit first. |
| H10 | Medium | **The registry is `void*`-typed with no signature check.** `static_plugin_registry.hpp:100-110` stores `{name → void*}`; a wrong `SSZ_STDCALL` signature or arity registers cleanly and crashes at first call. Nothing checks a `*_static.hpp` `extern "C"` prototype against the definition in `bridge.cpp`/`ssz.cpp` — C linkage means no type in the symbol name, so a mismatched prototype links cleanly. Making this likelier: the plugin ABI passes arguments **reversed** (`ssz.cpp:803-806` documents it: SSZ `(file, group, item, pxl)` arrives as `(pu, pxl, item, group, file)`), so all ~150 wrappers hand-reverse their parameter lists. Names are matched **case-sensitively** while library names are lower-cased (`normalise()` at `139-152` vs raw `entries[i].funcName` at `107`). `registerFunctions` validates *while* inserting (`78-83`): a null entry mid-table leaves a partially populated library, and because `libraries_[lib]` is created before validation, `hasLibrary()` still returns `true` — `loadDll` passes and lookups fail later. Duplicate names silently overwrite. The header calls `LOG_DEBUG` (`84`, `95`, `100`) without including `sszdef.h`, so it compiles only by include-order luck. |
| H11 | Medium | **The engine rewrites user data on every launch.** `main.cpp:415-416` calls `updateCharInSelectDef("data/select.def")` and `updateStageInSelectDef(...)` unconditionally: it appends character directories found in `chars/` and **deletes entries whose stage file is not found** (`main.cpp:306-308`). A stage that lives outside the expected path is silently removed from `select.def` on the next start, with only a `.bak` (whose previous `.bak` is deleted first, `main.cpp:318-322`). This is an unrequested destructive side effect in startup code, implemented with 300 lines of hand-rolled text munging in `main.cpp`. |
| H12 | Medium | **Renderer bookkeeping degrades silently.** `texPoolAcquire` on a hit does not re-register dimensions, so `DeleteGlTexture` (`6566-6576`) takes the `glDeleteTextures` branch instead of returning the texture to the pool — the pool works exactly once per size. `g_paltex` is never deleted (`626`). ARB programs are deleted only on the `fail:` path (`6863`). `g_paltexInitialized`/`g_glCache`/`glSpriteBegin`'s `static` prev-state (`7106-7108`) are never reset, so an `End()`→`Init()` cycle binds dead GL names. |
| H13 | High | **`make install` overwrites live save data.** `Makefile:736-742` does `cp -rf ssz_script/save/ install/` and `cp -rf lua_script/save/ install/` — no `-n`, no existence guard — and those directories contain `config.ssz`, `configNet.ssz`, `data_sav.lua`, `temp_sav.lua`, `tourney_sav.lua`, `training_sav.lua`, i.e. the save-data *templates* land on top of the runtime's live save data. It also silently reverts the very workflow `AGENTS.md` prescribes ("edit `install/save/config.ssz` for quick test runs"), and undoes any in-game settings change. Install should skip files that already exist (or install into a template dir consumed only on first run). |
| L1 | Low | `GetRendererInfo` (`sdlplugin.cpp:1879-1891`) cannot return its data — it logs it, with the comment "since SSZ string return is complex". A plugin API that can't return its own value will grow workarounds. |
| L2 | Low | `char buf[512]` at `1882` holds ~380 bytes worst case — safe today, but by coincidence, not by construction. |
| L3 | Low | `#if defined(DEBUG) \|\| defined(DEBUG)` (`sszdef.h:189`) — a duplicated condition; almost certainly meant `DEBUG \|\| something`. |
| L4 | Low | Dead POSIX shims `LoadLibrary`/`GetProcAddress`/`FreeLibrary` (`ssz.cpp:158-174`) survive the migration that made them dead. |
| L5 | Low | `WaitForSingleObject(gcthr, …)` with no `_beginthreadex` failure check (`compiler-state.hpp:83-89`): if the GC thread cannot start, `runner(mai)` runs with no collector. |

---

## 5. Build, portability, process

**Build system.** One 32 KB Makefile, all 18 external C libraries built from source with enumerated object lists (~380 lines of filenames; SDL2 alone is 167 entries at `Makefile:154-306`), `-MMD -MP` dependency tracking with an order-only prerequisite for the generated `pnglibconf.h` (`Makefile:600-611`) — better than most hand-rolled Makefiles. Real weaknesses:
- `MAIN_SRCS` is hardcoded (`Makefile:100-116`): adding a plugin means editing the Makefile in three places (source list, mapping header, `main.cpp`). Adding a 14th→15th plugin also needs a sixth edit for a new external lib (`DIR`, `SRCS`, `OBJS`, `LIB_x`, `ALL_LIBS`, plus two rules) — and nothing detects a forgotten entry until link time.
- `-MMD -MP` applies **only** to app and test objects (`Makefile:604`, `720`); the 18 external libraries have no header dependency tracking, so patching an external header leaves stale objects.
- `-include $(wildcard …)` (`Makefile:724`) is evaluated at parse time, so on a clean tree the `.d` files do not exist yet and header tracking only takes effect from the *second* `make` — a one-invocation staleness window.
- `ar rcs` (`Makefile:531-585`) never removes stale members: deleting a source from a list leaves its object in the archive forever.
- The `pnglibconf.h` rule `cp`s a generated header **into the third-party source tree** (`Makefile:654`), and rebuilds every app object when it changes.
- `-fpermissive` plus eight `-Wno-*` suppressions (`Makefile:38-45`) on a C++17 codebase, no `-Wextra`, no `-Werror`, no `-Wconversion`. `bridge.cpp` compiles clean under the current flags (verified with `-fsyntax-only`) — so the suppressors are hiding other files' noise, which nobody can now quantify.
- Debug is `-O1 -g` (`Makefile:48`): optimized *and* debug, the configuration that is hardest to step through; no sanitizers anywhere.
- `install` will `wget` `install.zip` from GitHub with no checksum if `install/` is missing (`Makefile:730-735`) — a supply-chain path in the build — and both `wget` and `unzip` are undeclared in the toolchain notes.
- `test:` runs one binary (`Makefile:719`); there is no aggregate `check` target. Conversely, script-only edits are correctly *not* prerequisites of `$(TARGET)`, so they never force a rebuild — but `install` won't re-run either, so `install/` goes stale silently.
- Dependency-count drift: `README.md`/`AGENTS.md` say "19 external libraries"; `ALL_LIBS` (`Makefile:525-528`) builds **18**, `external/` holds 20 directories, and `external/portaudio/` is referenced by no rule at all — a fully unused vendored dependency.

**Portability.** The Linux path is not merely untested, it is broken by construction: the JIT's `mmap` shim passes Windows `PAGE_*` constants as `prot` and uses `MAP_SHARED` (`ssz.cpp:206-213`); `VirtualFree` forwards size 0 to `munmap` from `~CompilerState` (`compiler-state.hpp:59`) so every JIT block leaks; `mprotect` is called with `PAGE_EXECUTE`. `renderer.h:18` pulls in `<windows.h>`, `PerfCounters` embeds `LARGE_INTEGER`, `gluPerspective`/WGL are used directly. Decide: **either** drop the Linux build and say so in `README.md`, **or** fix these four points — the current "experimental" label hides that it cannot work.

**Tests.** 44 assertions over one file for a 40k-line C++ engine, 40k lines of SSZ and 24k lines of Lua. I ran `build/Release/test_file.exe`: `=== 44 tests, 0 failures ===`. It links against a *prebuilt* `build/Debug/main/file/file.o` — a Debug/Release mismatch hazard. Zero coverage for: the SSZ parser/compiler (the highest-risk, highest-churn component), the SFF/PNG loaders (see C1/C2), the renderer, the state machine, and the scripts. `README.md` advertises six more test targets (`test_common`, `test_command`, `test_integration`, `test_matchflow`, `test_sff`, …) that **do not exist in the Makefile** — the only test target is `test_file`. No `.github/`, no CI, nothing gates a commit.

**Documentation drift.** `AGENTS.md`'s table is stale (`main.cpp` 165→430, `sdlplugin.cpp` 5,826→7,526, `sszdef.h` 176→199); `ISSUES.md` covers only the screenshot system; `README.md` lists the fictional tests above; `PROGRESS.md` says the sprite atlas is "disabled (P5b)" while `LoadPngTexture` calls `atlasAlloc` on the active modern path (`6524`). The habit of writing these documents is a strength — the drift is what will cost someone a day.

**Repo hygiene.** 6,836 tracked files, dominated by 19 vendored C libraries (~1,674 `.c`, 1,499 `.h`). No submodule or tarball strategy, so every upstream bump is pure diff noise. `.gitignore` has `*.exe` twice and directory entries `Debug`/`Release`; the untracked `.commandcode/` local tool directory is not ignored. `test/work/` is ignored, good. Five branches with 40–50 commits of divergence and no decision record is the single biggest *organizational* risk: two JIT backends (`x86.hpp` vs `asmjit` branch) are being carried in parallel.

---

## 6. Recommended order of work

1. **Asset-decoder hardening (C1, C2)** — 64-bit size math, pixel ceilings, `png_set_user_limits`, error handler on every libpng entry point. Add a malformed-PNG corpus test. This is the only finding I would call remotely exploitable.
2. **Make the crash path trustworthy (C4, C6)** — async-signal-safe report; decide whether `SSZ_TRY`/`g_cal` are supported (if yes, implement for MinGW; if no, delete 61 no-op handler sites so nobody trusts them).
3. **GL scissor ordering (C3)** — a ~10-line fix, but it is silent wrong output on the default renderer, so it should ship in the same pass.
4. **Empty-array indexing (C5)** — pick the contract (clamp to 0, or error) and encode it in one place; `refat`/`at` have already drifted into two copies of the clamp.
5. **Turn the profiler off by default (H4)** and key it by pointer; re-measure `OPTIMIZATION_PLAN.md`'s baseline afterwards, since the numbers were taken with the tax in place.
6. **Add CI** (`build Debug`, `build Release`, `make test`, plus a "compile every `.ssz` and diff the error output" golden test). The SSZ compiler is the churn centre and has zero tests; a 50-file corpus of scripts that must compile clean would catch more regressions than any other single investment.
7. **Cheap hygiene pass** — `snprintf` everywhere (H1), check every SDL/TTF/libpng create (H2), stop `make install` from clobbering save data (H13), name the plugin in "Failed to load plugin." (H9), registry key normalisation + a shared prototype header / `static_assert` for the reversed ABI (H10), fix the stale docs and delete the fictional test targets (§5).
8. **Decide the branches** — land or drop `asmjit`/`native`, or write the decision down and archive them. Also decide the Linux story before the `sszdef.h` POSIX shims rot further.

---

## 7. Verification status

Verified by direct reading: C1, C2, C3, C5, C6, H1, H2, H3 (comment + `MAXIMIZED`-only handling), H4, H5, H7, H8, H9, H10, H11, H12 (pool/`texDimRegister` path), H13 (install rules + the save-data templates they copy), all §5 build claims, the plugin-dir/header/shimm/registration consistency sweep, the test run, and all line references above.
Not verified (reported by sub-review, not reproduced): the `drawQuads`/`DrawTile` potential infinite loop when coordinates are NaN (`sdlplugin.cpp:~6990-7020`) and the `dbl` division before its guard; the null `ppal` dereference for truecolor sprites with alpha 127–254 (`5512-5525`); whether the GC thread can free concurrently with a running script. These are worth their own look, but I did not construct a reproduction.
Left out of scope: `external/` sources, asset content (`chars/`, `stages/`, `data/`), and the Lua script corpus beyond the fight-loop call site.

---

## 8. Remediation — 2026-10-28

Everything below was applied in one pass on top of `b436f5d`. Two items were
deliberately **not** "fixed" in the naive way and are documented as such.

### 8.1 Fixed

| ID | Change | Where |
|---|---|---|
| C1 | Single 64-bit size gate `pngCheckImageSize()` (per-side 16384, ≤16 Mi-pixel, ≤256 MiB), `png_set_user_limits()`, and the byte count checked **before** `resize`/`new` in all three decoders. The bogus "guard" that compared the same wrapped product is gone. | `main/sdlplugin/pngutil.h` (new), `sdlplugin.cpp` `DecodePNG8` / `DecodePNG32` / `LoadPngTexture` |
| C2 | `LoadPngTexture` now installs `setjmp(png_jmpbuf())`, frees `buff`/row pointers on both paths, and only sets `*w/*h` after a successful decode. | `sdlplugin.cpp` |
| C3 | Scissor is applied **before** vertices are emitted and stays enabled until the batch is rasterised; `glSpriteEnd()` no longer disables it; `glScissorOff()` is called by the flat-fill path and after the final frame flush (before `glClear`, which honours the scissor test). | `sdlplugin.cpp` `glScissorApply`/`glScissorOff`/`glSpriteBegin`/`glSpriteEnd`/`rectFillGl`/`GlSwapBuffers` |
| C4 | Crash path rewritten: `CrashDumpReport()` writes a fixed, pre-formatted buffer with `WriteFile` (no STL, no allocation, no locks) from a re-entry-guarded **unhandled-exception filter**. A Vectored Exception Handler was tried first and rejected during verification — it also sees benign first-chance faults the program handles itself (a GL driver probe was reported as a crash) — so the hook is `SetUnhandledExceptionFilter`, which only runs for genuinely unhandled faults. The TLS callback — which ran after static destructors had destroyed the profiler vectors — is deleted. Rich sorted reports still run from `atexit`. | `main/crash_log.hpp` (new), `main/ssz/ssz.cpp`, `main/main.cpp` |
| C5 | Index clamp made explicit and **behaviour-preserving**: non-empty ranges clamp as before, a negative index still means "from the end" (the script layer relies on `str[-1]`-style peeks), and the empty-range case — which used to hand back `data - tsize`, i.e. heap metadata — now returns either the last byte of the reference's own buffer (when `position > 0`) or a zero-filled pad. | `main/ssz/arrayandref.hpp` |
| C6 | `SSZ_EXCEPTION` is now defined only for `_MSC_VER` (it was defined on `_WIN32` while the macros needed MSVC, so 61 handler sites silently compiled to `{ }`). The JIT address table (`g_cal`) is consequently no longer dead: `SSZLookupScriptLocation()` maps a fault address to `file(line)` **without allocating or locking**, and the crash filter prints it. `VirtualProtect` failure is now logged. | `main/ssz/sszdef.h`, `ssz.cpp`, `jitcompiler.hpp` |
| H1 | `sprintf` → `snprintf` for every renderer-info field, including the D3D9 adapter description (512-byte source into a 128-byte field). | `sdlplugin.cpp` |
| H2 | Every `SDL_CreateRenderer` / `SDL_CreateTexture` / `SDL_LockTexture` / `TTF_RenderUNICODE_Blended` / `SDL_CreateTextureFromSurface` result is checked and logged; no NULL texture or renderer is passed on. | `sdlplugin.cpp` |
| H3 | `UpdateGLViewport` handles `RESIZED`, `SIZE_CHANGED`, `MAXIMIZED` and `RESTORED` and only re-sizes the GL viewport from the *drawable* size — `g_w`/`g_h` stay the logical resolution (which is what the streaming texture and clip rects are sized from). The stale `gluPerspective`/`g_w = window width` behaviour is gone. | `sdlplugin.cpp` |
| H4 | Recording, not just printing, is gated on `g_profilerLog`; `ProfBegin`/`ProfEnd` early-return; name lookups use a hash index instead of a linear `std::string` scan. Enabled with `IKEMEN_PROFILE=1`. | `main/time_profiler.hpp`, `main/ssz/ssz.cpp`, `main/main.cpp` |
| H5 | `MemRecord` aggregates by name (adds `count`) instead of appending an unbounded vector. | `main/mem_profiler.hpp` |
| H7 | Partially: the per-frame `pcall(0, 1)` leaked one Lua stack slot per frame for the whole match (unbounded growth, rising cost). Now `pcall(0, 0)`. The `getGlobal("loop")` lookup remains — hoisting it needs an ABI-level change to the lua plugin (see 8.2). | `ssz_script/ssz/fighting.ssz` |
| H8 | `compileSub1()` re-enables error appending and clears stale messages; `run()` still disables appending for the runtime phase. | `main/ssz/compiler-state.hpp` |
| H9 | Missing plugin/export now reports *which* plugin and *which* function, and distinguishes "library not registered" from "function not found". | `main/ssz/sourcetree.hpp` |
| H10 | Validation is a full pass before any insertion (no partially populated library, so `hasLibrary()` no longer lies); export names are normalised like library names; duplicates are detected and logged; the header includes `sszdef.h` instead of relying on include order. | `main/ssz/static_plugin_registry.hpp` |
| H11 | `select.def` is no longer rewritten on every launch: entries whose directory/file is missing are **kept** and reported instead of deleted, the file is only rewritten when something was actually appended, and the `.bak` backup is never overwritten. | `main/main.cpp` |
| H12 | Pooled textures are re-registered on reuse (the pool previously worked once per size); `g_paltex`, the legacy ARB programs and all GL state caches are released/reset at teardown; the `glSpriteBegin` batch state was moved out of function-local statics so it can be reset. | `sdlplugin.cpp` |
| H13 | `make install` uses `cp -rf -n` for the save directories, so live `config.ssz` / `*_sav.lua` survive an install. | `Makefile` |
| L1 | `GetRendererInfoStr()` returns the info to the script (`^/char`); `getRendererInfo()` keeps its logging behaviour. Wired through the bridge, the static table and `ssz_script/lib/alpha/sdlplugin.ssz`. | `sdlplugin.cpp`, `bridge.cpp`, `sdlplugin_static.hpp`, `ssz_script` |
| L2–L5 | `snprintf` for the 512-byte info buffer; the duplicated `DEBUG` condition fixed; the dead POSIX `LoadLibrary`/`GetProcAddress`/`FreeLibrary` shims removed; a failed GC-thread start now aborts `run()` with a message instead of running without a collector. | `ssz.cpp`, `sszdef.h`, `compiler-state.hpp` |
| — | Defensive guards for the unverified items: `(fsr ? fsMode : fsMode)` no-op removed; NaN/zero divisions in `drawTile`/`drawQuads` guarded and the tessellation count clamped (a huge `n` could reserve gigabytes); iteration caps on both `drawTileHolizon` fill loops; `fastSimpleAddRectBlit` rejects a NULL palette; `MemoryKakuho` rejects negative/overflowing sizes. | `sdlplugin.cpp`, `ssz.cpp` |
| — | Build/process: `-Og -g3` for Debug (was `-O1 -g`), new `check` / `test_helpers` targets, `test/test_helpers.cpp` covering the PNG size gate and the index clamp contract, CI workflow added. Docs corrected (`AGENTS.md` sizes, `README.md` test targets + profiler opt-in + crash reports, `PROGRESS.md` atlas status). | `Makefile`, `test/`, `.github/workflows/build.yml`, docs |

### 8.2 Deferred, with reasons

- **H6 (per-object `CRITICAL_SECTION`)** — removing it means changing the memory
  model that the GC thread depends on. It is a design change, not a bug fix, and
  the safe version (a per-heap lock or a GC-only stop-the-world phase) needs its
  own review. Left as-is and documented here.
- **H7 remainder (per-frame `getGlobal`)** — the fix is to resolve `loop` once per
  match and call it by Lua registry reference. That requires a new lua-plugin
  export with the *native* ABI (which mixes with the old reversed ABI), so it
  belongs in its own change with a test, not in a remediation sweep.
- **H10 remainder (signature checking)** — a shared prototype header plus
  `static_assert`s for the ~150 wrappers is worthwhile but mechanical; the
  argument-reversal convention is now documented at the one place that defines it.
- **`ar rcs` stale archive members; `-MMD` for external libraries** — both need a
  dependency-tracking change across ~380 enumerated library objects; the current
  behaviour only bites when a source is removed from a library list.
- **Branch consolidation, vendored-library strategy, `external/portaudio/`
  deletion** — repository policy decisions, not code changes.

### 8.3 Verification of the remediation

- `make CONFIG=Debug check` → `28 checks, 0 failures` (helpers) + `44 tests, 0
  failures` (file I/O).
- `make CONFIG=Debug install` and `make CONFIG=Release` both link.
- Boot matrix, 8 s each, from `install/`: Renderer 0 (SDL2 software), 1 (GL 2.1),
  2 (GL
3.3 → driver reports 4.6), 4 (Direct3D 11) — all reach menus and keep running,
  no crash, no alert. This exercises the scissor changes (GL), the
  streaming-texture checks (SW/DX) and the PNG/asset path.
- `IKEMEN_PROFILE=1` clean-exit run prints the TIME and MEMORY rankings (records +
  sorts correctly); a default run prints nothing.
- `IKEMEN_CRASH_TEST=1` (new, documented) faults on purpose: the reporter prints
  reason, fault address and the (empty) region list, and the process still dies —
  no hang. The `SSZ script location` line said "not inside JIT code" because the
  injected fault is in `main.cpp`, not in JIT output; the JIT branch of
  `SSZLookupScriptLocation` is compile-verified only.
- Release build (`-O3`) links, installs and boots; `make CONFIG=Release check`
  passes both suites. A 30-second Release boot produced **zero** spurious crash
  reports after the hook moved from a VEH to `SetUnhandledExceptionFilter`.

**Regression found and fixed during verification:** the first version of the C5
clamp mapped *negative* indices to element 0. The script layer uses `str[-1]` to
mean "last character", so lifebar/SFF loading broke — and only at `-O1`, while
`-Og` happened to work, which is exactly the kind of split that hides for months.
The culprit was bisected by reverting file groups and rebuilding. The clamp now
preserves the negative-index convention and only hardens the genuinely
out-of-bounds empty-range case; `test/test_helpers.cpp` pins the contract.
