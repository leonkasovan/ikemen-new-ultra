# SSZ → Native Library Conversion — Progress

Tracks the ongoing migration of the SSZ script libraries (`ssz_script/lib/*.ssz`)
to native C++ implementations (`ssz_script/lib/*.cpp`) backed by the native
library registry (`main/ssz/native_lib.hpp`).  The goal: replace interpreted
`.ssz` library modules with JIT-compiled-adjacent C++ functions where the logic
is concrete, while keeping template-bound code in SSZ (see "Why some libraries
stay in SSZ" below).

## Status summary

**14 native libraries registered, 3 libraries intentionally remain in SSZ.**

| # | Library | Native source | Status | Notes |
|---|---|---|---|---|
| 1 | `time` | `time.cpp` | ✅ **Fully native** | `tickCount`/`unixTime`; `.ssz` kept as `time.ssz.bak` — first conversion (pilot) |
| 2 | `shell` | `shell.cpp` | ✅ **Fully native** | `open`/`moveToTrash`; `shell.ssz.bak` |
| 3 | `thread` | `thread.cpp` | ✅ **Fully native** | `sleep`/`delay`; `thread.ssz.bak` |
| 4 | `math` | `math.cpp` | 🔀 **Hybrid** | PRNG core (`random`/`srand`/`rand`) + trig/round native; templates delegated from `math.ssz` |
| 5 | `string` | `string.cpp` | 🔀 **Hybrid** | Plain-function core native (incl. `sToU8`/`u8ToS`/`cFind`/`percentEnc`/`percentDec`); templates, list-returners, `&Format` stay in SSZ |
| 6 | `md5` | `md5.cpp` | 🔀 **Hybrid** | One-shot `md5`/`md5str` + `&Md5` streaming (RFC 1321-correct); dead T-table constants removed from `md5.ssz` |
| 7 | `arcfour` | `arcfour.cpp` | 🔀 **Hybrid** | One-shot `arcfourEnc` + `&Arcfour` streaming (RFC 6229-correct) |
| 8 | `file` | `file.cpp` | 🔀 **Hybrid** | Module functions + `&File` methods |
| 9 | `regex` | `regex.cpp` | 🔀 **Hybrid** | `&Regex` methods |
| 10 | `sound` | `sound.cpp` | 🔀 **Hybrid** | `&Client` methods |
| 11 | `ssz` | `ssz.cpp` | 🔀 **Hybrid** | `memMarkBefore`/`memMarkAfter`/`run` + `&Compiler` methods |
| 12 | `socket` | `socket.cpp` | 🔀 **Hybrid** | Non-template `&Socket` methods; `recv<_t>`/`send<_t>`/`accept` stay in SSZ |
| 13 | `base64` | `base64.cpp` | 🔀 **Hybrid** | Byte-level core (`uintToB64Char`/`b64CharToUint`/`encB64`/`decB64`); templates delegate for byte-sized types, SSZ bit-packing fallback for wide types |
| 14 | `table` | `table.cpp` | 🔀 **Hybrid** | Concrete `hash` only; `NameTable`/`IntTable`/`intHash<_t>` stay in SSZ |
| — | `consts` | — | ⏸ **Stays in SSZ** | Pure type-system sugar (`&Signed<_t>`, `&Unsigned<_t>`, `null<_t>`) |
| — | `stack` | — | ⏸ **Stays in SSZ** | 100% template data structure (`&Stack<_t>`/`&Node<_t>`) |
| — | `alert` | — | ⏸ **Stays in SSZ** | 3-line wrapper; `_t` only feeds compile-time `typeid(_t)` |

Legend: ✅ fully native · 🔀 native core with SSZ delegation wrapper · ⏸ stays in SSZ

Every library that *can* be native is now native — the three remaining are
template-bound by design (see below).

## Conversion phases (commit history)

| Phase | Commits | What landed |
|---|---|---|
| Static plugin groundwork | `e746623`, `fa4cd69`, `d2fe8c6` | Bare-name plugin resolution (`<time>` vs `<dll/time.dll>`); static registry replaces DLL loading; `*_plugin.hpp` headers moved into each plugin dir |
| Native registry + pilot | `c48d6f9` | `native_lib.hpp` registry; `time` + `shell` fully native (first conversions) |
| Thread | `b7a896b` | `thread` fully native |
| Math PRNG | `34fdbe6` | `random`/`srand`/`rand` native + **module-variable support** in the registry (`srand` state) |
| Math trig | `dd0fd66` | `sin`/`cos`/`tan`/… + `round` family native |
| String | `624ac01` | String plain-function core native; `^/char` returns via heap `Reference` (TMPREF path) |
| MD5 / Arcfour | `ee010e3`, `fa37a84` | One-shot + streaming struct methods; out-param write-back fix (`nextLine`) |
| File | `6bf8330` | Module functions + `&File` methods |
| Struct delegation pattern | `d66bf18` | `&Regex`/`&Client`/`&Compiler`/`&Socket` via the field-out-param `&X` pattern |
| Base64 | `b43e368` | Byte core native, templates delegated |
| Winsock fix | `1093846` | Static-build `WSAStartup` gap so `connect`/`listen` work in the exe |
| Table hash | `4bd7064` | `table.hash` native; investigation documented (templates can't go native) |
| Dead code cleanup | `f73a978` | Removed md5 T1–T64 constants (native carries its own) |
| Regression suite | `bdd3a1c` | `test/ssz/*` + `test/run_ssz_tests.sh` (12 tests, frozen reference vectors) |
| decBase64 fix | `a644791` | `(_t)` → `(*_t)` cast fix — `decBase64<_t>` compiles; suite covers round-trips |
| Makefile test wiring | `d717a0f` | `make test` / `test-file` / `test-ssz` |

## Architecture

### Native library registry (`main/ssz/native_lib.hpp`)

`lib name = <name>;` with no `.ssz` file resolves through
`NativeLib::FindLibrary("name")`. Each `ssz_script/lib/<name>.cpp` exports
`extern "C" bool <name>_lib_register()` which registers functions as
`{ name, "ssz-signature", fnptr }` triples; signature strings are tokenized
into plugin-typed henshuu so calls type-check and JIT-compile exactly like
`plugin` calls.  Registration is wired in `main/main.cpp` and
`NATIVE_LIB_SRCS` in the Makefile.

### Plugin ABI (all proven by `main/ssz/bridge.cpp`)

- **Arguments arrive reversed** — last SSZ param is first C++ param.
- **32-bit args** (`int`/`uint`/`bool`/`float`) occupy the low 32 bits of an
  8-byte slot — declare `int32_t`/`uint32_t`/`float`, never `int64_t`.
- **Strings** arrive as `Reference`; convert via `refToWstring`.
- **Out-params** (`type=`) arrive as a pointer to the caller's slot
  (`index i=` → `int32_t*`, `^ubyte dest=` → `Reference*`); write in place.
- **String/array returns** return the address of a heap-allocated `Reference`
  (`sszrefnewfunc` + `init` + `wstrToRef` / `refnew`+`memcpy`), or `0`.

### The `&X` struct-method delegation pattern

Stateful structs (`&File`, `&Md5`, `&Arcfour`, `&Regex`, `&Client`,
`&Compiler`, `&Socket`) stay defined in `.ssz` as data containers; non-template
methods delegate to the native lib with the struct's fields passed as out-params:

```ssz
lib fl = <file>;
public &File
{
  index fh = 0;
  public bool open(^/char fn, ^/char mode)
  {
    ret .fl.fileOpen(`fh=, fn, mode);
  }
}
```

Template methods and whole-object moves keep their in-body `plugin` declarations
(the static registry and native registry coexist).

## Why `consts` / `stack` / `alert` stay in SSZ

Verified empirically (documented in AGENTS.md):

- **`alert`** — 3-line wrapper; `_t` only feeds compile-time `typeid(_t)`.
- **`stack`** / **`table`** — pure template data structures with
  template-typed *fields* (`^_t data`) and delegate params (`each`/`forEach`).
- **Native `_t` signatures provably fail** — a registered signature with `_t`
  compiles the module, but every call site dies with "Compilation Error."
  (`KansuuKata`/`TokenToTypeId` have no `TYPE_TOKEN` resolution; module
  henshuu are synthesized once, never re-parsed per instantiation).
- **`^null` (DYNREF) can't store data** — transient call params/returns only;
  `^null` locals and struct fields fail to compile.

Templates work in SSZ because instantiation (`probe!int?`) **re-parses the body
from source** with `_t` bound (`BlockOpen`/`MakeTree`, sourcetree.hpp ~5999) —
that's why socket's in-body plugin declarations work. The native registry has no
such re-parse.

## Known fixes / gotchas worth remembering

- **`(_t)` vs `(*_t)` casts in templates**: bare `(_t)` casts fail at
  instantiation unless the operand is already `_t`-typed; `(*_t)` works
  universally.  `decBase64`'s byte path was fixed with this one-character change.
- **`md5str(digest)` re-hashes** — `md5str` hashes its input; render a digest
  with `toHex!ubyte?` instead.
- **Test scripts must live in `test/ssz/`** — the exe resolves `lib/x.ssz`
  relative to its own dir; pass an absolute path to the script.
- **`uint` literals in templates** (`tmp = 1;`) fail — the strict int→uint
  literal rules apply inside templates too.

## Testing

- **SSZ suite**: `test/ssz/*.ssz` with frozen `test/ssz/*.expected` references,
  run by `test/run_ssz_tests.sh` from gitignored `test/work/`.  12 tests cover
  12 of the 14 native libs (sound is device-dependent — checks completion, not
  values; `shell`/`string` have no dedicated test — `string`'s `sToU8`/`u8ToS`
  are exercised indirectly by `basetest`/`md5test`).
- **C++ smoke test**: `test/test_file.cpp` (40 checks) — `make test-file`.
- **Makefile**: `make test` = `test-file` + `test-ssz` (the latter forces a
  `CONFIG=Debug install` so the runner finds `install/ikemen-debug.exe`).

## Remaining / next steps

- None of the 14 convertible libs remain — the conversion is complete for the
  concrete surface.  Future work would require JIT support for
  `TYPE_TOKEN` resolution in native signatures (to port `stack`/`table`) or
  `DynamicRef` frame slots (to hold `^null` data).
- `decBase64` wide-type fallback (bit-packing for `short`/`int`/`long`) is
  exercised but only `int`/`short` are in the regression test — `long`/`float`
  round-trips could be added.
