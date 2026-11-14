#!/usr/bin/env bash
# Needs the Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html
set -e
emcc src/ecosystem.cpp -O3 -std=c++17 \
  -sMODULARIZE=1 -sEXPORT_NAME=createSim -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=cwrap,HEAPF32 \
  -o public/sim.js
echo "Built public/sim.js + public/sim.wasm"
