#!/usr/bin/env bash
# Builds Nioh3Training.asi with zig's clang (no MSVC needed): pip install --user ziglang
set -e
cd "$(dirname "$0")"
# crash 2026-10-18: clang SEH is call-only by default, so __try around a plain load (Rd64 etc.) emitted no handler. -fasync-exceptions
# keeps the scope on faulting loads; the __try helpers are also noinline so every call site keeps the guard.
py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall src/training.c -o Nioh3Training.asi -lgdi32 -lmsimg32

# Package the generated archive table unchanged next to the ASI.
cp ../../data/boss-action-record.tsv Nioh3Training_action_records.tsv
