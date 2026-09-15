#!/usr/bin/env bash

set -veuo pipefail

if command -v gcc &>/dev/null; then
    CC="gcc"
else
    CC="clang"
fi

$CC -O0 -g -I src/klib/ThirdParty/ -I src/ buildScript.c -o build
./build "$@"
