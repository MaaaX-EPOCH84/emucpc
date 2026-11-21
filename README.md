# EmuCPC

An emulator of the Amstrad CPC range, both generations: CPC 464, 664 and
6128, then CPC 464 Plus, 6128 Plus and GX4000.

It aims at the behaviour of the real hardware, down to the master clock
(one step every 1/16 µs), and has two uses:

- an ordinary emulator, with a window, picture, sound and keyboard;
- a measuring instrument for developers and scripts: without a window,
  driven by a script or a local socket, deterministic, with timed traces of
  what the machine does.

**Status: early development.** Nothing is usable yet.

## Building

Needs CMake 3.28 or later and a C++20 compiler (GCC 13 is used).

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

Windows executables are built on Linux with MinGW-w64 and tested under Wine:

```sh
ci/deps.sh                      # fetches SDL3 for MinGW, once
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
```

Other presets: `linux-debug`, `linux-san` (AddressSanitizer and
UndefinedBehaviorSanitizer), `macos`.

No ROM is included in this repository.

## Licence

GPL-3.0-or-later (`LICENSE`). Credits and third-party licences:
`CREDITS.md`, `third_party/VERSIONS.md`.
