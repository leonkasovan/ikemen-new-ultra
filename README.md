
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
- `make CONFIG=Debug test` — full native SSZ test suite (379+ assertions, 0 failures) — runs from project root
- `make CONFIG=Debug test_install` — same tests run from the install/ runtime environment (uses real chars, stages, .snd files) — kills stale process, deploys binary, runs, counts PASS
- `make CONFIG=Debug test_common` — utility functions
- `make CONFIG=Debug test_command` — command loading (kfm.cmd)
- `make CONFIG=Debug test_integration` — cross-module validation
- `make CONFIG=Debug test_matchflow` — match state machine (13 tests, 0 failures)
- `make test_sff|font|animation|action|stage CONFIG=Release` — interactive SDL2 tests

Test results: `docs/native_test_results.txt`

Runtime Trace Capture:
Traces log every SSZ plugin ABI call with category filtering to reduce noise:

```powershell
# Trace only SDL operations (render, input, display, BGM)
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=64 CONFIG=Debug -j8
.\build\Debug\ikemen-debug.exe 2>&1 | findstr "[TRACE]" > trace_sdl.log

# Trace everything except file I/O (skip the 42k Read noise)
make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=254 CONFIG=Debug -j8

# Trace categories (set IKEMEN_TRACE_MASK to a bitwise OR):
#   1=FILE     File I/O (Read, Write, Seek…)
#   2=NET      Socket (Connect, Send, Recv…)
#   4=LUA      Lua bridge (NewState, Pcall, ToString…)
#   8=OGG      OGG Vorbis audio
#  16=UTIL     Regex, shell, alert, clipboard, INI…
#  32=MATH     Math functions (Sin, Cos, Sqrt…)
#  64=SDL      SDL operations (DrawTTF, Fill, Flip, PollEvent…)
# 128=SYS      Game state (Common, System, Loader…)
# 255=ALL      Trace everything (default)
```

Feature Flag Override:
Each native module can be disabled individually to fall back to the original SSZ script:
```bash
make IKEMEN_NATIVE_FILE_LIB=0 IKEMEN_NATIVE_SDLPLUGIN_LIB=0 CONFIG=Debug
```
Run `make native_manifest CONFIG=Debug` to show which modules are active.

Native SSZ Conversion:
See `docs/SSZ_CONVERSION_GUIDE.md` for the full conversion guide.
See `TODO_SSZ_CONVERSION.md` for current status by module.
See `docs/native_ssz_comparison.md` for per-module scaffold status (2,069 symbols, 100% scaffolded).

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