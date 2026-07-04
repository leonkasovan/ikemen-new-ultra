The latest `TODO_SSZ_CONVERSION.md` update shows a major change: **SSZ-to-native scaffolding is now complete**. The project is at **100% scaffolding coverage for 2,069 symbols across 45 SSZ files**, with `IKEMEN_USE_NATIVE_SSZ=1` and all native modules active. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)

## Current Status

* **45 / 45 SSZ files scaffolded** — no remaining symbol gaps. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* **2,069 / 2,069 symbols mapped/scaffolded.** [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* **37 native service `.cpp` files** exist across foundation, runtime, SDL, and gameplay/resource modules. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* **34 static registration headers** exist, with **32 static registration calls wired in `main.cpp` before`Run(scriptPath)`**. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* The native manifest comparison now says **all SSZ files are scaffolded**, and the remaining work is **wiring stubs to real native logic**. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* Existing tests for file, math, thread, directory, random, range operations, PRNG, trigonometry, and logarithms are passing. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)
* The pre-conversion trace confirms startup succeeds: static plugins register, script compilation reports `error size=0`, and SDL2 renderer/audio initialize successfully. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)

## What’s Next?

You are done with **coverage/scaffolding**. The next phase is **behavioral parity**: replacing generated/native stubs with real implementations.

### 1. Regenerate and lock the manifest

First, capture this milestone.

```text
Goal: native_ssz_comparison.md shows 45/45 scaffolded and 2069/2069 symbols.
```

Commit the updated:

```text
TODO_SSZ_CONVERSION.md
docs/native_manifest.txt
docs/native_ssz_comparison.md
docs/ssz_symbol_manifest.txt
docs/ssz_dependency_graph.txt
docs/native_test_results.txt
```

This becomes the new baseline:**“all symbols scaffolded; implementation parity pending.”**

***

### 2. Start replacing stubs by runtime importance

The update explicitly says the remaining work is to replace stub scaffolds with full native implementations, especially in gameplay/resource modules. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)

Recommended priority:

```text
1. system_service
2. loader_service
3. stage_service
4. char_service
5. fight_service
6. command_service
7. common_service / share_service
8. sff_service
9. action_service
10. bg_service
11. sound_resource_service
12. font_service / video_service
```

Reasoning: `system`, `loader`, and `stage` determine whether the engine can select and load real content; `char` and `fight` are the largest gameplay-critical surfaces.

***

### 3. Build native-vs-SSZ parity tests

The trace already gives useful expected behavior: static plugin registration, script compile success, SDL2 renderer init, audio init, and startup asset state. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)

Create tests for:

```text
startup compiles with error size == 0
static plugins register successfully
selected/default char resolves correctly
selected/default stage resolves correctly
renderer init succeeds
audio init succeeds
file read/seek behavior matches SSZ
stack/ref operations match SSZ
```

This is the right moment to shift from “does it compile?” to “does native behavior exactly match SSZ?”

***

### 4. Use feature flags for A/B migration

Since every module is behind a flag, keep using module-level fallback while replacing stubs. The documented mechanism is:

```bash
make IKEMEN_NATIVE_<NAME>_LIB=0
```

For example:

```bash
make IKEMEN_NATIVE_STAGE_LIB=0
make IKEMEN_NATIVE_STAGE_LIB=1
```

Then compare runtime traces/output. This gives you safe, incremental migration.

***

### 5. Focus next implementation sprint on gameplay/resource modules

The update specifically calls out gameplay/resource modules as the next focus: `action`, `bg`, `char`, `command`, `fighting`, `fight`, `font`, `video`, `stage`, `sff`, and `sound_resource`. [\[ikemen-new...ra.zip.txt \| Txt\]](https://bri2-my.sharepoint.com/personal/00057717_hq_bri_co_id/Documents/File%20Microsoft%20Copilot%20Chat/ikemen-new-ultra.zip.txt)

I’d make the next sprint:

```text
Sprint A:
- system_service real state lookups
- loader_service real char/stage loading entry points
- stage_service real metadata/load behavior
- parity tests for kfm + stages/stageZ.def startup
```

Then:

```text
Sprint B:
- char_service core data access
- command_service parser behavior
- fight_service loop/state interfaces
```

***

## Bottom Line

The next milestone is no longer “finish scaffolding.” It is:

> **Make native services behaviorally equivalent to SSZ one module at a time.**

Immediate next action:

```text
1. Commit the 100% scaffold milestone.
2. Add parity tests from the pre-conversion trace.
3. Implement real native logic first in system_service, loader_service, and stage_service.
4. Then move into char_service and fight_service.
```
