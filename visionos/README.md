# Experimental visionOS windowed port

This development target runs the native recompiled game in a visionOS window with SDL3, Metal and gamepad input.
It is intended for local builds from the owner's game files. It is not an immersive or stereo rendering mode.

## Status

- Tested with the visionOS 27 simulator on Apple Silicon: opening movie, title controls, save creation and loading,
  name entry, dialogue and character movement in Marin and Tarin's house.
- Tested on an M2 Vision Pro running visionOS 27.0.1: native startup, first drawable acquisition and sustained
  opening-movie rendering, with a captured device framebuffer. Longer gameplay, memory pressure and controller
  interaction still need device testing.
- Both the simulator and the tested Vision Pro can reserve the existing 12.5 GiB guest virtual address window.
  This is a virtual reservation, not an upfront allocation of that much physical memory.
- CPU game code is generated on the development Mac and linked before signing. Runtime compiler subprocesses,
  randomizer generation and re-exec are unavailable in this target.
- The Metal backend decodes BC textures on devices that do not support BC compression. MetalFX and the macOS-only
  depth-format path are disabled.
- The visionOS game image renders at a fixed 1920x1080 with original 16:9 framing. Window resizing only scales
  that image; it does not increase the game's render resolution. Earlier display-relative preferences are
  overridden, and the Video tab shows the fixed format. The native window requests uniform resizing from a
  1280x720-point size to maintain 16:9, including when upgrading from a restored freeform window.
- This is an initial bring-up, not a complete game compatibility or background/resume validation.

## Build prerequisites

Use an Apple Silicon Mac with Xcode, the visionOS SDK, Python 3.12, CMake, Ninja and XcodeGen.
The SDK deployment target is 2.0; the tested SDK is 27.0.

Follow the main README's requirements for your own legally obtained **v1.0.0 extracted files** in `extracted/exefs`
and `extracted/romfs`. This repository contains no game files, generated game code, keys or Switch fonts.
No instructions for obtaining those files are provided.

From the repository root:

```sh
python3.12 -m venv tools/venv
tools/venv/bin/pip install capstone unicorn lz4 fonttools
tools/venv/bin/python visionos/prepare_font.py
make gen
make build/inc/smaa_hlsl.inc

mkdir -p build
clang -O2 -Iruntime visionos/romfs_cli.c runtime/romfs_build.c -o build/romfs-build
build/romfs-build extracted/romfs extracted/romfs.bin

bash visionos/build_deps.sh xrsimulator
# Also build device libraries if testing on Vision Pro:
bash visionos/build_deps.sh xros

xcodegen generate --spec visionos/project.yml
open visionos/LASVision.xcodeproj
```

`build_deps.sh` installs platform libraries into the sibling `deps-xros` and `deps-xrsimulator` directories used by
the Xcode project. It pins SDL3 3.4.18 and builds a minimal static LGPL FFmpeg 9.0.2. These are development builds;
review the third-party licenses before distributing binaries.

Choose the **LASVision** scheme and either a visionOS simulator or your Vision Pro. For device signing, put your
team identifier in the ignored `visionos/Local.xcconfig`:

```xcconfig
DEVELOPMENT_TEAM = YOUR_TEAM_ID
```

The Run action disables **Debug executable** to avoid debugger attachment at launch. Re-enable it manually
in Edit Scheme only when you need an interactive debugging session.

The shared `Signing.xcconfig` includes that file if present. Automatic signing is enabled. No account or team
identifier is committed. The post-build phase stages the private RomFS image and the small set of ordinary files
needed by movie playback and cosmetic patches, before Xcode signs the app. The first device install transfers
approximately 6 GB. Never publish this private app bundle or commit its staged game files.

For a command-line simulator build:

```sh
xcodebuild -project visionos/LASVision.xcodeproj -scheme LASVision \
  -sdk xrsimulator -destination 'generic/platform=visionOS Simulator' \
  -derivedDataPath build/vision CODE_SIGNING_ALLOWED=NO build
```

## Controls and diagnostics

A Bluetooth gamepad is recommended. Press L+R at the title (LB+RB on Xbox or L1+R1 on PlayStation).
Keyboard defaults: Q+E for L+R, K to confirm, J to cancel, WASD to move, Return for Plus, and F1 for the overlay.

Saves and settings stay in the app's writable container. The runtime log is under
`Library/Application Support/local/LASVision/lasvision.log`. Startup failures before runtime initialization can
also appear in the Xcode console. The host cannot re-exec to retry an address-space collision on visionOS.
The visionOS target uses 16 MiB host thread stacks; thread creation errors are logged and returned to the guest.
Set the Run environment variable `LARECOMP_STARTUP_DIAGNOSTICS=1` to save three native thread snapshots to
`startup-threads.log` alongside the runtime log, without attaching a debugger. First-frame milestones and the
UIKit scene/layer state are also recorded in the runtime log.

BC decoder regression checks, including non-multiple-of-four image dimensions and signed BC5 components:

```sh
clang -O1 -g -fsanitize=address,undefined -Iruntime/nvn \
  visionos/tests/test_bc.c runtime/nvn/bc_decode.c -o build/test-bc
build/test-bc
```

## Third-party additions

- `third_party/bcdec.h`: bcdec v0.985 by Sergii Kudlai, used under the MIT option in its embedded license.
- The fallback font is derived locally from the pinned Google Fonts Noto Sans SC source, under the SIL Open Font
  License in `visionos/fonts/OFL.txt`. The static TTF is intentionally ignored. `prepare_font.py` downloads the
  source and instantiates weight 400; this avoids unsupported variable/CFF font paths in the game engine.

Keep device test results separate from simulator results when preparing an upstream PR.
