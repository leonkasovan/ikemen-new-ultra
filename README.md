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
- `make CONFIG=Debug check` — everything that needs no display: `test_helpers` (header-only safety helpers: PNG size gate, SSZ index clamp) + `test_file` (native file I/O, 44 assertions)
- `make CONFIG=Debug test_helpers` — just the header-only helpers
- `make CONFIG=Debug test` — just the file I/O regression suite

Interactive SDL2 tests (`test_sff`, `test_font`, `test_animation`, `test_action`, `test_stage`)
and the `test_common` / `test_command` / `test_integration` / `test_matchflow` targets
that earlier revisions of this file advertised **do not exist in the Makefile**. The
only test binaries are `test_helpers` and `test_file`.

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

## Crash Reports

A Vectored Exception Handler prints an allocation-free crash summary to **stderr**
before the process dies (it never blocks the debugger/WER report):

```
==== CRASH REPORT ====
reason: access violation
fault address: 0x6179821
SSZ script location: c:\...\install\ssz\char.ssz(1153)
time regions (ms, calls):
  script:draw = 812.4  x640
  ...
======================
```

The script location comes from the JIT address table, so it is exact when the fault
happened inside compiled SSZ code. The `time regions` list is filled from an
allocation-free mirror of the profiler, so it only has content when profiling is on
(`IKEMEN_PROFILE=1`); the rest of the report is always available. The hook is
`SetUnhandledExceptionFilter`, so it only fires for faults nothing handled (a
first-chance exception a driver handles internally stays silent). To verify the
reporter on a new machine:

```bash
IKEMEN_CRASH_TEST=1 ./ikemen-debug.exe      # faults on purpose
```

## Performance Profiling

The engine has a built-in, pprof-style session profiler. No external tools needed.

**Recording is off by default** (it costs a clock read + map update per mark, and the
fight loop emits ~25 marks per frame). Enable it with an environment variable:

```bash
IKEMEN_PROFILE=1 ./ikemen-debug.exe > game.log 2>&1    # bash / sh
$env:IKEMEN_PROFILE=1; .\ikemen-debug.exe              # PowerShell
```

With the flag set, run a representative match and quit normally: a ranked time report
prints at exit. Without it, the profilers record nothing and print nothing.

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
make CONFIG=Release install       # or CONFIG=Debug (use Release for numbers)
cd install
IKEMEN_PROFILE=1 ./ikemen.exe > game.log 2>&1   # play; quit normally
rg "TIME USAGE RANKING" -A 30 game.log
```

Redirecting stdout keeps the console clean; capture the last lines of `game.log`
for the report. Profiling is gated on recording *and* printing, so a build without
`IKEMEN_PROFILE` measures the engine, not the profiler.

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
