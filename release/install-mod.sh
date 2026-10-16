#!/bin/sh
exec python3 -I "$(dirname "$0")/install_mod.py" "$@"
