# Switch Port Status

## Current status
Milestone 1 (playable single-player core) has reached on-device boot.
The app launches, loads scenario data, and reaches the main menu.

## What works
- Full Aleph One engine cross-compiles via autotools in a Podman container
- App launches on Switch hardware via hbmenu
- MML script loading (scenario filenames, string overrides)
- Scenario data loading from SD card
- Main menu is reachable

## Build output
- `out/switch/alephone.elf`
- `out/switch/alephone.nacp`
- `out/switch/alephone.nro`

Build command:

```bash
./scripts/switch-container-build.sh
```

Manual equivalent:

```bash
podman build -t alephone-switch -f containers/switch/Containerfile .
podman run --rm -v "$PWD":/work:Z -w /work --userns keep-id alephone-switch bash ./scripts/switch-build.sh
```

## SD card deployment
Copy the contents of `data/Scenarios/Marathon/` to `sdmc:/switch/alephone/`:

```
sdmc:/switch/alephone/
  Map.scen
  Shapes.shps
  Sounds.sndz
  Marathon.appl
  Physics.phys
  Scripts/
    Marathon.mml
  Music/
  Plugins/
```

## Debugging
- Runtime logs: `sdmc:/switch/alephone/Aleph One Log.txt`
- Logging also emits to `stderr` for early startup failures.

## Bugs fixed

### Boost.Filesystem fdopendir incompatibility
The app originally crashed on launch with:
```
fatal alert (ID=-1): Please be sure the files 'Map', 'Shapes', 'Images' and 'Sounds' are correctly installed and try again.
```

**Root cause:** Boost.Filesystem 1.87.0 (pre-compiled in devkitPro portlibs) uses `fdopendir()` for directory iteration. libnx's newlib does not implement `fdopendir` — the stub in `switch/platform_stubs.cpp` returns `nullptr` with `ENOSYS`. This silently broke all directory listing, preventing MML script loading. Without MML, the engine searched for bare filenames ("Map", "Shapes", "Sounds") instead of the actual files ("Map.scen", "Shapes.shps", "Sounds.sndz").

**Fix:** `Source_Files/Files/FileHandler.cpp` conditionally uses `std::filesystem` instead of `boost::filesystem` on Switch (`#ifdef __SWITCH__`). GCC 15.2.0's libstdc++ uses `opendir()` which is properly implemented in devkitPro's `libsysbase`.

## Disabled features (milestone 1)
- Networking (`--disable-networking`)
- OpenGL (`--disable-opengl`, software renderer only)
- Film export (dependencies disabled in build script)
- SDL_image / zzip / PNG / vorbis / vpx / matroska / ebml / curl / miniupnpc / NFD

## Remaining work
- On-device gameplay verification (menu -> level -> play -> save/load -> audio)
- Input mapping refinement for controller
- Packaging polish: final NACP metadata, icon assets
