Toolchain:
Requires: w64devkit x86 (https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x86-2.8.0.7z.exe)
Extract in C:\x86devkit

Clone Project:
`git clone -b static https://github.com/leonkasovan/Ikemen-Plus-Ultra.git ikemen-plus-ultra-static`
`cd ikemen-plus-ultra-static`

Build:
1. Debug   : `make CONFIG=Debug 2>&1 | tee build-debug.log`
2. Release : `make 2>&1 | tee build-release.log`

Tests:
- `make CONFIG=Debug test_common` — utility functions
- `make CONFIG=Debug test_command` — command loading (kfm.cmd)
- `make CONFIG=Debug test_integration` — cross-module validation
- `make CONFIG=Debug test_matchflow` — match state machine (13 tests, 0 failures)
- `make test_sff|font|animation|action|stage CONFIG=Release` — interactive SDL2 tests

Debug with gdb:
1. Open w64devkit shell and go to install
2. Create 
```cmd gdb_watch.cmd
set pagination off
set logging file ikemen_crash.log
set logging on
set logging redirect on
handle SIGSEGV stop
handle SIGABRT stop
handle SIGFPE stop
run ssz/ikemen.ssz

# Only print crash diagnostics if the program stopped due to a signal
# $_exitsignal is an integer on signal, void on normal exit
# Compare against $_void to avoid "Invalid type combination" error
if $_exitsignal != $_void
  bt full
  info registers
  x/30i $pc-20
end
quit
```
2. `gdb -x gdb_watch.cmd ikemen-debug.exe`

## Performance Profiling

The engine has a built-in, pprof-style session profiler. No external tools needed.
Run the game (Debug or Release), play a representative match, quit normally, and a
ranked time report prints at exit (also on crashes):

```
[Time] ==== TIME USAGE RANKING ====
[Time]   region                              total(ms)      calls      avg(ms)
[Time]   frame:total                         ...
[Time]   frame:script(SSZ+logic)             ...
[Time]   script:draw                         ...
[Time]   script:charAction                   ...
[Time]   frame:render:sprites                ...
...
```

### How to run a profile

```bash
make CONFIG=Release install       # or CONFIG=Debug
cd install
./ikemen.exe > game.log 2>&1      # play; quit normally (or wait for match end)
grep "TIME USAGE RANKING" -A 30 game.log
```

Redirecting stdout keeps the console clean; capture the last lines of `game.log`
for the report.

### Adding your own marks

- **C++:** `TIME_SCOPE(tag)` (RAII) or `TIME_MARK_BEFORE(tag)` / `TIME_MARK_AFTER(tag)`
  from `main/time_profiler.hpp`. Report printed by `TimePrintRanking()`, hooked
  into the exit path in `main.cpp` next to `MemPrintRanking()`.
- **SSZ scripts:** `.ssz.profBegin(:"tag":)` / `.ssz.profEnd(:"tag":)` (wrappers in
  `ssz_script/lib/ssz.ssz`; plugin impl in `main/ssz/ssz.cpp`). Script-only changes
  need no rebuild — copy the file over:
  `cp ssz_script/ssz/foo.ssz install/ssz/`.
- Any file using `.ssz.` must import it first: `lib ssz = <ssz.ssz>;` (missing this
  import produces `'ssz' is not defined`).

### Built-in marks

- Frame totals (always on): script, sprites, shadows, fills, flip — accumulated per
  frame in `renderer.h`/`sdlplugin.cpp` (`Flip`/`GlSwapBuffers`).
- `ssz:compile` / `ssz:run` (JIT phases).
- Fight loop: `script:stageAction`, `script:charAction`, `script:frameAdvance`,
  `script:draw`, `script:luaLoop`, `script:hotkeys`, `script:roundCheck`,
  `script:camMath`, `script:cmdUpdate` (frame pacing / event pump).
- Draw internals: `script:draw:bg|fighters|anims|shadows|overlay|faceSetup|fg`,
  `script:bgDraw:setup|layers|layer` (per background layer).

### Reading the report

- `script:*` and `ssz:*` rows use `std::chrono::steady_clock` — exact times.
- `frame:*` rows use `QueryPerformanceCounter` — on VMs this can overcount
  significantly, so treat them as **proportions**, not absolute values.
- Debug builds inflate `script:draw` (per-call printf logging); profile Release
  for representative numbers.

See **`OPTIMIZATION_PLAN.md`** for the measured baseline, the ranked optimization
candidates (the stage background layers are the one real hotspot), and a
before/after measurement workflow.
