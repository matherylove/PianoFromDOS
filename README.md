# PianoFromDOS

**PianoFromDOS** is an experimental Windows 98 compatibility port of Brian Pantano's
[Piano From Above](https://github.com/brian-pantano/PianoFromAbove).

The port keeps the original Win32/Direct3D 9 architecture, but replaces or isolates
parts of the 2010 build that prevent a clean Windows 98 target and a reproducible
GitHub Actions build.

## Current target

- Windows 98 / Windows 98 SE
- 32-bit x86 (i686)
- Direct3D 9
- D3DX9_30
- WinMM MIDI
- Unicode UI through Microsoft's MSLU (`unicows.dll`)
- No Boost dependency in the PianoFromDOS target
- No `libprotobuf` dependency in the PianoFromDOS target
- GitHub Actions cross-compilation on Ubuntu

## GitHub Actions build

The workflow is:

```text
.github/workflows/build-win98.yml
```

Every push, pull request, or manual workflow run builds a Windows 98 x86 artifact
named:

```text
PianoFromDOS-Windows98-x86
```

The artifact contains `PianoFromDOS.exe`, runtime notes, PE information, and the
original Piano From Above license.

The CI downloads a pinned `redpanda-cpp/mingw-lite` cross-toolchain profile built
for Windows 98 (`32_686-msvcrt_win98`), verifies its SHA-256, builds the
`libunicows` import library from a pinned source commit, configures CMake, builds,
checks the resulting PE32 executable, and uploads the artifact.

No Visual Studio installation is required by CI.

## Runtime requirements on Windows 98

GitHub Actions builds the application itself, but Microsoft runtime components are
not redistributed by this repository. Place/install legally obtained copies of:

- `UNICOWS.DLL` (Microsoft Layer for Unicode), next to `PianoFromDOS.exe`.
- `D3DX9_30.DLL`, from a compatible DirectX 9.0c-era redistributable.
- Common Controls 5.80 or later is recommended for the Unicode controls used by
  the original interface.

See `WIN98_RUNTIME.txt` for the compact runtime checklist.

## What changed for PianoFromDOS

The Windows 98 target currently includes these portability changes:

- application branding changed to PianoFromDOS;
- x64 is not a supported target;
- XP-specific target macros were replaced by Windows 98 target macros;
- `boost::circular_buffer` was replaced by a small fixed-capacity C++ container;
- generated Protocol Buffers/libprotobuf runtime usage was replaced with a small
  schema-specific wire-format implementation that preserves `MetaData.pb`;
- NT extended paths (`\\?\`) are not used on Win98;
- filesystem access converts Unicode paths to the active Win98 ANSI code page at
  the OS boundary;
- `SendInput` was replaced by the Win9x-compatible `mouse_event` call;
- `SetDCBrushColor`/`DC_BRUSH` drawing was replaced by normal solid brushes;
- unsupported list-view double buffering is disabled;
- UxTheme headers/libraries are no longer part of the CI target;
- D3DX is explicitly linked against `d3dx9_30`;
- the Visual C++ resource script is converted to UTF-8 and made portable to
  MinGW `windres` on a case-sensitive Linux runner.

The old Visual Studio project files are retained as historical/reference files;
GitHub Actions uses `CMakeLists.txt` instead.

## Local cross-build

The GitHub Actions workflow is the canonical build recipe. If you already have the
same Win98 MinGW cross-toolchain and a built `libunicows.a`, the equivalent CMake
configuration is:

```sh
cmake -S . -B build-win98 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/win98-toolchain.cmake \
  -DPFD_UNICOWS_LIBRARY=/path/to/libunicows.a \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-win98
```

## License / upstream status

This repository is based on Piano From Above source code. The original
`Docs/License.txt` is preserved unchanged. That license contains restrictions on
modification and derivative works. Before publicly redistributing PianoFromDOS,
review the original license and obtain any permission that may be required from
the original author. This port is not presented as an official Piano From Above
release.
