#!/usr/bin/env bash

set -veuo pipefail

gcc -I src/klib/ThirdParty/ -I src/ buildScript.c -o build
./build
