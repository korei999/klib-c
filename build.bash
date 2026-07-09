#!/usr/bin/env bash

set -veuo pipefail

gcc -O0 -g -I src/klib/ThirdParty/ -I src/ buildScript.c -o build
./build "$@"
