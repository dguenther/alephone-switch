# Switch Port Status

## Current milestone
Milestone 1 (playable core) has started.

## Completed in this step
- Added containerized Switch toolchain definition:
  - `containers/switch/Containerfile`
- Added host helper wrapper:
  - `scripts/switch-container-build.sh`
- Added in-container build entrypoint:
  - `scripts/switch-build.sh`
- Added initial Switch build target:
  - `switch/Makefile`
- Added bootstrap source used to validate toolchain + SDL2 wiring:
  - `switch/source/phase1_bootstrap.c`

## What this currently builds
- `switch/alephone.nro` and `switch/alephone.elf`
- copied artifact: `out/switch/alephone.nro`

This is a bootstrap executable to validate container/toolchain setup. It is not yet the full Aleph One engine build.

## Remaining for Phase 1 completion
- Replace bootstrap source set with minimum Aleph One source graph for single-player runtime.
- Introduce initial compile-time feature gates for Switch milestone policy:
  - `DISABLE_NETWORKING`
  - OpenGL disabled
  - film export disabled
- Resolve missing cross-platform dependency surface for Aleph One core build (notably Boost/ASIO/libsndfile equivalents on Switch).
