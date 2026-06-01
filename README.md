# kameret

> Low-latency C++23 eye-tracking framework for Linux real-time targets

![Demo](https://github.com/user-attachments/assets/0aacd914-c322-4ebf-9ed7-a70191dc3ad1)
C++ running in WASM.

---

## Overview

**kameret** is a portable, real-time eye-tracking library written in C++23. It is designed around a hardware abstraction layer that decouples camera acquisition, gaze estimation, and filtering from any specific platform or ML backend. The primary targets are latency-sensitive Linux deployments on x86\_64 and ARM64, including kernels patched with `PREEMPT_RT`.

Development is done on Windows (Visual Studio 2026) with cross-compilation and integration testing performed inside QEMU virtual machines.

---

## Status

| Component | Status |
|---|---|
| OpenCV camera HAL (`ICamera`) | Working |
| One-Euro gaze filter (`IGazeFilter`) | Working |
| Stub gaze estimator | Working |
| CMake build system + vcpkg integration | Working |
| x86\_64 QEMU dev VM | Working |
| ARM64 QEMU dev VM | Working |
| MediaPipe Face Mesh adapter | Planned |
| Lock-free frame pipeline | Planned |
| SIMD image processing | Planned |
| Iris contour extraction + ellipse fitting | Planned |
| Kalman filter (process noise model) | Planned |
| PREEMPT\_RT acquisition loop | Planned |

---

## Architecture

```
apps/demo
    │
    ▼
Pipeline  ──────────────────────────────────────────────┐
    │                                                    │
    ▼                                                    ▼
ICamera (HAL)                               IGazeEstimator (HAL)
    │                                                    │
    ▼                                                    ▼
OpenCVCamera                         StubEstimator / MediaPipeEstimator
(platforms/opencv_camera.hpp)        (adapters/mediapipe/)
    │
    ▼
V4L2 / MSMF
    │
    ▼
Camera Device
```

Core types (`Frame`, `TrackingResult`, `GazePoint`) are plain data structs with no library dependencies. All filtering is done via the One-Euro algorithm (Casiez et al. 2012) in `core/include/eyetracker/one_euro_filter.hpp`.

---

## Repository Layout

```
kameret/
├── core/                        # Portable C++23 — no ML deps
│   ├── include/eyetracker/
│   │   ├── types.hpp            # POD domain types
│   │   ├── hal.hpp              # ICamera, IGazeFilter, IGazeEstimator
│   │   ├── one_euro_filter.hpp  # Low-latency gaze smoothing
│   │   └── pipeline.hpp        # Grab → estimate → filter loop
│   └── src/
├── platforms/
│   ├── opencv_camera.hpp        # Shared OpenCV ICamera impl
│   ├── windows/
│   ├── linux_x86_64/
│   └── linux_arm64/
├── adapters/
│   └── mediapipe/               # Isolated ML integration layer
│       ├── include/
│       │   ├── stub_estimator.hpp
│       │   └── mediapipe_estimator.hpp
│       └── CMakeLists.txt
├── apps/
│   └── demo/                    # Camera → filter → overlay window
├── external/
│   └── mediapipe/               # git submodule (google-ai-edge/mediapipe)
├── scripts/
│   ├── build_windows.ps1
│   ├── build_linux_x86.sh
│   └── build_linux_arm64.sh
├── CMakeLists.txt
├── CMakePresets.json
└── vcpkg.json
```

---

## Dependencies

### vcpkg (all platforms)
```json
{ "dependencies": ["opencv4", "fmt", "eigen3"] }
```

### Linux (Debian/Ubuntu)
```bash
sudo apt install build-essential cmake ninja-build clang \
                 libeigen3-dev libopencv-dev
```

### Windows
- Visual Studio 2026 with the **Desktop development with C++** workload
- [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set
- CMake 3.25+, Ninja (bundled with VS)

---

## Build

### Windows (Developer PowerShell for VS 2026)

Import the MSVC environment, then configure and build:

```powershell
# Import MSVC env vars into the current session
cmd /c "`"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat`" && set" |
  Where-Object { $_ -match '=' } |
  ForEach-Object {
    $name, $value = $_ -split '=', 2
    [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
  }

$cl  = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe"
$lnk = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\link.exe"
$lib = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\lib.exe"
$ninja = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

cmake -S . -B build/windows-release `
  -G "Ninja" `
  -DCMAKE_MAKE_PROGRAM="$ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  "-DCMAKE_CXX_COMPILER=$cl" `
  "-DCMAKE_LINKER=$lnk" `
  "-DCMAKE_AR=$lib"

cmake --build build/windows-release
```

### Linux x86\_64
```bash
export VCPKG_ROOT=~/vcpkg
./scripts/build_linux_x86.sh
```

### Linux ARM64 (cross or native)
```bash
export VCPKG_ROOT=~/vcpkg
./scripts/build_linux_arm64.sh
```

---

## Run

### Windows
```powershell
.\build\windows-release\apps\demo\kameret_demo.exe        # device 0
.\build\windows-release\apps\demo\kameret_demo.exe 1      # device 1
```

### Linux
```bash
./build/linux-x86-release/apps/demo/kameret_demo          # device 0
./build/linux-x86-release/apps/demo/kameret_demo 1        # device 1
```

Press `q` or `ESC` to quit. The demo window shows the live camera feed with a gaze overlay dot and per-frame latency in the corner. With the stub estimator the dot traces a synthetic ellipse; once `MediaPipeEstimator` is wired in it tracks the iris directly.

---

## QEMU Development VMs

```powershell
# x86_64
./scripts/boot-x86.ps1

# ARM64
./scripts/boot-arm64.ps1
```

Both VMs share the host source tree via virtio-fs. Build inside the VM using the Linux scripts above; no cross-toolchain needed.

---

## Enabling MediaPipe

The MediaPipe adapter is compiled out by default. Once `external/mediapipe` is populated (via submodule or prebuilt install):

```bash
cmake ... -DEYETRACKER_ENABLE_MEDIAPIPE=ON
```

The stub estimator is replaced automatically. Only `adapters/mediapipe/` has any dependency on MediaPipe headers or libraries; `core/` and `platforms/` are unaffected.

---

## Design Goals

- **Deterministic acquisition** — `CAP_PROP_BUFFERSIZE=1`, minimal internal queuing, One-Euro filter for sub-frame smoothing without lag
- **PREEMPT\_RT compatibility** — acquisition loop designed for eventual `SCHED_FIFO` deployment; no heap allocation on the hot path (planned)
- **Clean abstraction boundary** — `ICamera`, `IGazeEstimator`, `IGazeFilter` are the only interfaces `Pipeline` depends on; swap any layer without touching the others
- **Cross-platform SIMD** — Eigen back-end for linear algebra; SIMD image processing via OpenCV with explicit `cv::UMat` path planned
- **Embedded deployment** — ARM64 target, static linking option, no exceptions on hot path (planned)
