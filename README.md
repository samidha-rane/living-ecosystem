# Living Ecosystem: C++ OOP in the browser

Predator–prey evolution simulator. Core engine is object-oriented C++17 (`src/ecosystem.cpp`), compiled to WebAssembly with Emscripten.

**Classes:** `Entity` (abstract) → `Plant`, `Creature` (abstract) → `Herbivore`, `Predator` · `Genome` (mutating traits) · `World` (owns and steps everything). Uses inheritance, polymorphism, encapsulation, smart pointers.

## Build (once, needs Emscripten)
    git clone https://github.com/emscripten-core/emsdk.git && cd emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh && cd ..
    ./build.sh          # creates public/sim.js + public/sim.wasm

## Test locally
    python3 -m http.server -d public 8000   # open http://localhost:8000

## Deploy on Vercel
Commit everything, including the built `public/sim.js` and `public/sim.wasm`, push to GitHub, then import the repo at vercel.com/new. No framework, no build command (`vercel.json` already sets the output folder to `public`).

## Native C++ test (no browser)
    g++ -std=c++17 -O2 -DNATIVE_TEST src/ecosystem.cpp -o t && ./t
