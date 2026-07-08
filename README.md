# Sid Meier's Civilization Revolution — native PC recompilation

**2K/Firaxis's console 4X, statically recompiled from Xbox 360 PowerPC to a
native x86-64 executable.** No emulator, no interpreter, no JIT — the original
disc code is translated to C++ with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) **v0.8.0** and linked
against its runtime. It builds clean and **boots crash-free through the entire
runtime and Gamebryo engine init**, up to asset loading.

## Status: **boots into engine init** ⚙️

Extraction → triage → codegen → build → bring-up, in one sitting. The 16.8 MB
game executable — **40,067 recompiled functions** — compiles, links into an
80 MB `civrev.exe` with a single kernel-stub bundle, and boots with **zero
`FATAL` / unresolved / unregistered lines**: D3D12 device, audio + XMA threads,
guest disk mount, `Runtime initialized successfully`, XEX load, shader storage,
GPU interrupt callback — then reaches the game's asset-mount phase.

> **Honest scope:** it does **not render yet.** The clean boot ends in an access
> violation while loading assets — the Gamebryo engine probes for a `D:` installed-
> title mount and an `Assets/` + `Resource/Xenon/` layout our disc extract doesn't
> provide, and doesn't error-check the misses. Next step is **data-path / VFS
> mapping** (+ `--protect_zero=false`). See [PROGRESS.md](PROGRESS.md).

## How it got here

```
disc ISO ──▶ XDVDFS extract ──▶ XEX triage ──▶ rexglue init + codegen
            (111 files)        (base 0x82000000)   (40,067 funcs, 231 MB C++)
      │
      └──▶ cmake build ──▶ link (+XUsbcam stubs) ──▶ boot ──▶ runtime init
                                                              ──▶ ⚙️ engine init
```

The recompiler and build were minutes of work. The craft was **runtime bring-up**,
and the lesson was to hint *less*:

- **Kernel stubs.** The link failed only on the `XUsbcam*` bundle (Xbox 360
  Vision Camera — imported by ~26% of titles, used by almost none). Dropped in
  the toolkit's default `stubs.cpp`.
- **Init-thunk wall.** First guest crash was a dense run of static-init thunks
  (`0x82E80xxx`) discovery didn't place. **Harvested** them at runtime with a
  tolerant-dispatch scaffold that logs every unregistered indirect-call target —
  one boot surfaced the complete set of **21**, all guaranteed-real entry points.
- **The over-hinting trap.** Batching in `find_missing_vtable_funcs.py`'s 301
  "vtable" hits made it *worse* — the scanner grabbed **switch/jump-table**
  entries that point mid-function, splitting real functions and turning a loop
  back-edge into a fatal `Unresolved call`. The fix was **deletion**: keep only
  the 3 codegen-required + 21 runtime-verified hints (**24 total**), and the
  whole class vanished. Runtime-verified hints beat pointer-scan guesses.

See [PROGRESS.md](PROGRESS.md) for the full blow-by-blow.

## Binary facts

| | |
|---|---|
| Title | Sid Meier's Civilization Revolution (disc, ID `545407E5`) |
| Publisher | `0x5454` = "TT" = Take-Two / 2K |
| Engine | Gamebryo (`CivCon_*` / `LeaderheadGamebryo` shaders) |
| Image base | `0x82000000` (standard) |
| Image size | `0x11F0000` (17.9 MB) |
| Imports | `xboxkrnl.exe`, `xam.xex` (175 + 260, no Live wall) |
| Recompiled functions | 40,067 |
| Manifest hints | 24 (3 codegen-required + 21 harvested) |

## Build & run

You **bring your own** copy of the game — the disc image, extracted assets, and
recompiled C++ are all git-ignored. This repo tracks the project, not the game.
Prereqs: Clang 20+, CMake 3.25+, Ninja, VS2022, and a built ReXGlue SDK v0.8.0.

```bash
# 1. Extract your disc image (XDVDFS) with the 360tools kit
python /path/to/360tools/tools/extract_iso.py "Civilization Revolution.iso" extracted/

# 2. Regenerate the recompiled C++ (git-ignored, ~231 MB)
cd project && rexglue codegen

# 3. Build (VS2022 dev shell + LLVM on PATH)
cmake --preset win-amd64-release "-DCMAKE_PREFIX_PATH=<rexglue-sdk>/out/install/win-amd64"
cmake --build out/build/win-amd64-release

# 4. Run
./out/build/win-amd64-release/civrev.exe --game_data_root=../../../extracted
```

To reproduce the init-thunk harvest (only needed if you regenerate from a fresh
manifest): configure with `-DCIVREV_HARVEST=ON`, boot once, and register the
addresses logged to `harvest.log` as `[entrypoint.functions]` hints.

## Layout

```
project/
  civrev_manifest.toml    # codegen config + 24 function-entry hints (the bring-up work)
  src/main.cpp            # ReXApp entry point
  src/civrev_app.h        # app subclass (virtual hooks)
  src/stubs.cpp           # kernel stubs (XUsbcam*)
  src/dispatch_tolerance.cpp  # harvest scaffold, OFF by default (CIVREV_HARVEST)
  generated/              # codegen output — default/ is git-ignored, regenerable
extracted/                # game data — bring your own (git-ignored)
```

## Credits

Built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (recompiler +
runtime, D3D12/Vulkan backends derived from [Xenia](https://github.com/xenia-project/xenia))
and the [360tools](https://github.com/sp00nznet/360tools) toolkit. Civilization
Revolution and all game assets are © 2K Games / Firaxis / Take-Two — this project
contains **none** of them. Every game recompiled is a game preserved.

## License

This project's own code (the ReXApp entry, stubs, manifest, and scripts) is
[MIT](LICENSE). It does **not** cover the game (bring your own) or the ReXGlue SDK
and its dependencies, which carry their own licenses.
