# EyeTracker

EyeTracker is a low latency C++23 computer vision and eye-tracking framework targeting:

* Linux
* PREEMPT_RT
* x86_64
* ARM64

The project is developed on Windows using Visual Studio and tested inside QEMU virtual machines.

## Current Scope

Current functionality:

* V4L2 camera abstraction
* CMake-based builds
* x86_64 QEMU development VM
* ARM64 QEMU development VM

Future functionality:

* Lock-free frame pipelines
* SIMD image processing
* Contour extraction
* Ellipse fitting
* Kalman filtering
* Gaze estimation

## Build

Linux:

mkdir build

cd build

cmake ..

cmake --build .

## Dependencies

Debian:

sudo apt install

build-essential
cmake
clang
libeigen3-dev
libopencv-dev

## Run

./eye_tracker

## Architecture

Application
|
v
EyeTracker Library
|
v
V4L2
|
v
Linux Kernel
|
v
Camera Device

## QEMU

x86:

powershell
./scripts/boot-x86.ps1

ARM64:

powershell
./scripts/boot-arm64.ps1

## Goals

* Deterministic low-latency camera acquisition
* PREEMPT_RT compatibility
* Cross-platform SIMD support
* Embedded deployment
* Reusable C++ library design
