# lazydockercpp — Terminal UI for Docker (C++ port of lazydocker)

A zero-dependency C++ port of [lazydocker](https://github.com/jesseduffield/lazydocker) — a terminal UI for Docker and Docker Compose management.

## Why lazydockercpp?

The original [lazydocker](https://github.com/jesseduffield/lazydocker) requires the Go toolchain plus dozens of modules. lazydockercpp compiles with a single `make` using only C++17 and standard Linux headers.

## Quick Start

```bash
make
./lazydockercpp
```

## Features

- Container lifecycle management (start, stop, restart, remove)
- Real-time log viewer with search
- Resource usage display (CPU, memory)
- Image browser (pull, remove, inspect)
- Volume and network management
- Docker Compose integration
- Keyboard-driven navigation

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make, nlohmann/json (vendored)
