#!/usr/bin/env bash
# Builds Nioh3Training.asi with zig's clang (no MSVC needed): pip install --user ziglang
set -e
cd "$(dirname "$0")"
python3 -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Wall src/training.c -o Nioh3Training.asi
