# ScreenCap dev commands. Requires just, cmake, ninja and a C++20 toolchain.

set windows-shell := ["cmd.exe", "/c"]

build_dir := "build"

default:
    @just --list

[doc('Configure + build build/screencap')]
build:
    cmake -S . -B {{build_dir}} -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build {{build_dir}}

[doc('Build, then copy screencap to ~/.local/bin')]
install: build
    bash scripts/install.sh {{build_dir}}

[doc('Clone/pull, build and capture on an ssh host, then copy the PNG back')]
remote-test host:
    bash scripts/remote-test.sh {{host}}
