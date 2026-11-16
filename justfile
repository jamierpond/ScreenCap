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

[doc("Build everything, then run clang-tidy (eacp's checks) over ScreenCap's sources; fails on any finding")]
[unix]
tidy: build
    scripts/tidy.sh {{build_dir}}

[doc('clang-format every tracked source in place')]
[unix]
format:
    git ls-files '*.h' '*.cpp' '*.mm' | xargs clang-format -i

[doc('Delete the build directory')]
clean:
    cmake -E rm -rf {{build_dir}}

[doc('Move branch-pinned dependencies to their branch heads and rewrite lockstep.lock')]
update-deps:
    cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=update
    cmake -S . -B {{build_dir}} -G Ninja -DLOCKSTEP_MODE=locked
