# Sid Meier's Civilization Revolution — Development Progress

Toolchain: **ReXGlue SDK v0.8.0** (self-contained; `rexglue init` / `rexglue
codegen`, no XenonRecomp).

## Phase 1: Extraction & Triage (DONE)

- Source: a **retail Xbox 360 disc image**, a 7.8 GB `.iso` in XGD / XDVDFS
  format (the `[PAL][NTSCU]` release).
- `extract_iso.py` → **111 files** to `extracted/`: `default.xex` (16,822,272 B),
  a `Resource/` tree (`Common/…`), a `shaders/fx/xenon/Release/` set of
  `CivCon_*.fxobj` compiled effects, and the disc's `$SystemUpdate` blob. The
  `CivCon_*` / `LeaderheadGamebryo` shader names confirm the **Gamebryo** engine.
- `xex_info.py` triage of `default.xex`:
  - Image base `0x82000000` (standard). Image size `0x11F0000` (**17.9 MB** —
    a large title; ~2.7× You Don't Know Jack's code image).
  - Imports: `xboxkrnl.exe`, `xam.xex` only. **No XNET/Live import wall.**
    175 kernel imports + 260 XAM imports (mostly by ordinal) = 435 total.
  - The compressed image makes the static-library table read as garbage in the
    header (expected — rexglue decompresses it during codegen).
  - Cute detail: `ORIGINAL_BASE_ADDRESS` reads `0x64656661`, ASCII **"defa"**
    (the start of "default").

## Phase 2: Scaffold & Codegen (DONE)

- `rexglue init --project-name civrev --xex-path extracted/default.xex
  --game-root extracted --project-root project` → manifest, CMake, `src/main.cpp`,
  `src/civrev_app.h`.
- `rexglue codegen`:
  - **First pass:** 3 `UnresolvedCall`s — tail-call `b` targets outside any
    discovered function (`0x82324B48`, `0x827F8840`, `0x82841038`).
  - Added all three as entry-only `[entrypoint.functions]` hints (discovery
    sizes them).
  - **Second pass: clean.**
  - Output: **117 `.cpp` files, 231 MB** in `project/generated/default/`
    (git-ignored), **40,067 recompiled functions** — a big binary.

## Phase 3: Build (DONE — one stub bundle)

- Configured with Clang 21.1.8 / Ninja / lld-link against the ReXGlue SDK v0.8.0
  install (`-DCMAKE_PREFIX_PATH=…`). CMake found `find_package(rexglue) 0.8.0`.
- All 40k recomp TUs (121 Ninja targets) **compiled first try**.
- **Link gap:** the house-standard `XUsbcam*` bundle — the game imports the
  Xbox 360 Vision Camera API (`sub_82A8A060…` thunks) that `rexruntime` doesn't
  export. Dropped in the toolkit's `templates/overlay/src/stubs.cpp` (Create →
  success, the rest no-ops), added it to `CIVREV_SOURCES`. **Links clean.**

## Phase 4: Boot & bring-up (DONE — boots crash-free through engine init)

First boot linked cleanly and got a **healthy** way in before the expected wall:

- D3D12 device (**NVIDIA RTX 5070**) → `FunctionDispatcher initialized` → SDL3
  input → audio + XMA Decoder / Audio Worker threads → mounted `extracted/` as
  the guest disk → GPU Commands + VSync threads → `GPU system initialized` →
  `Runtime initialized successfully`.
- `Loading XEX image`, Kernel Dispatch, `Initializing shader storage for title
  545407E5` (publisher `0x5454` = **"TT"** = Take-Two).
- First crash: `[FATAL] Call to invalid or unregistered function at 0x82E80DA8`
  — the house-standard bring-up class (targets discovery didn't place in a
  function).

**The bring-up craft here was learning to hint *less*, not more.** Two classes,
one real:

1. **Init-thunk class (real).** The startup iterates a table of static-init /
   module-registration thunks in a dense `0x82E80xxx` run that branch-discovery
   didn't place. Rather than guess boundaries, harvested them at runtime: a
   tolerant-dispatch scaffold (`src/dispatch_tolerance.cpp`, built with
   `-DCIVREV_HARVEST=ON` + `/force:multiple`) logs every unregistered
   **indirect-call** target and no-ops it, so one boot surfaced the complete set
   — **21 addresses**. Indirect-call targets are guaranteed real function
   entries (you can't indirect-call into the middle of a function), so these are
   trustworthy. Registered all 21.

2. **The over-hinting trap (self-inflicted).** First instinct was to also batch
   in `find_missing_vtable_funcs.py`'s **301** "missing vtable" hits. That made
   things *worse*: the scanner can't tell a vtable from a **switch/jump table**,
   and switch tables point **mid-function**. Registering those split real
   functions into fragments — e.g. the one-function loop at `0x82802A08` got
   cut at `0x82802A2C/A44/A80/AB4/AD0`, turning its internal `b 0x82802A44` loop
   back-edge into an inter-fragment `[FATAL] Unresolved call from 0x82802ACC to
   0x82802A44`. Chasing that fatal's codegen notes just registered *more*
   mid-function labels — a symptom spiral.

   **Fix: delete the speculative hints.** Dropped the whole 301-entry
   pointer-scan batch (and the codegen-note batch it spawned), keeping only the
   3 codegen-required tail-call targets + the 21 runtime-verified harvest
   entries = **24 hints total**. The `0x82802A08` function stays whole, its loop
   back-edge resolves as a normal intra-function branch, and the `Unresolved
   call` class is **gone**.

Result: **boots crash-free** through the entire runtime + Gamebryo engine init —
shader storage, GPU interrupt callback (`SetInterruptCallback`), and into asset
mounting — with **zero** `FATAL`/`unresolved`/`unregistered` lines.

> Lesson for the house: prefer **runtime-verified** hints (harvested indirect
> targets) over **pointer-scan** hints. The vtable scanner is fine when its hits
> are real vtables, but on a title with data-section switch tables it produces
> function-splitting false positives. Hint the minimum; let the runtime tell you
> what's actually missing.

## Phase 5: Asset loading (IN PROGRESS — current wall)

The crash-free run ends in an **access violation (`0xC0000005`)** during asset
load, not a runtime `FATAL`. The game probes for paths our extract doesn't
contain and doesn't error-check the misses:

- `GAME:\Resource\Xenon\`, `GAME:\Assets\Common\`, `GAME:\Assets\Xenon\`,
  `GAME:\config\config.ini` — all `VFS: entry not found`.
- `D:\Data\Shaders\Data\Fragments\Cached\DefaultCache.psc` — `NtCreateFile
  FAILED 0xC000000F` (the `D:` drive is the *installed-title* mount, not the
  disc).
- `ShaderDumpxe:\CompareBackEnds` → `[no device]`.

Our ISO extract produced `Resource/Common/`, `shaders/`, `default.xex` — the
engine wants an `Assets/` + `Resource/Xenon/` layout and an installed-title `D:`
mount. So the next class is **data-path / VFS mapping**, likely combined with
`--protect_zero=false` (the house tolerance for the null read the failed opens
lead into). See `docs/runtime-debugging.md` steps 2–3.

## Next up (TODO)

- [ ] Map the guest asset layout: mount `D:` to the installed-title data, and/or
      symlink `Assets/` + `Resource/Xenon/` to what the disc actually ships.
      Re-extract the ISO if `extract_iso.py` missed a nested directory (only
      111 files came out — light for a retail disc).
- [ ] Retry with `--protect_zero=false` to get past the null read the failed
      asset opens cause, and see the next wall.
- [ ] Drive to the front-end / first render; capture a screenshot.
- [ ] Retire the `src/dispatch_tolerance.cpp` harvest scaffold (off by default,
      `CIVREV_HARVEST=OFF`) once asset loading is clean.
