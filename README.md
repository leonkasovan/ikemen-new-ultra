
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