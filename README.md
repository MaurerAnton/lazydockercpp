# lazydockercpp — Docker Terminal UI (C++ port of lazydocker)

A zero-dependency C++ port of [lazydocker](https://github.com/jesseduffield/lazydocker) — a keyboard-driven terminal UI for Docker container management.

## Why lazydockercpp?

The original [lazydocker](https://github.com/jesseduffield/lazydocker) requires Go plus dozens of modules. lazydockercpp compiles with a single `make` using only C++17 and pthreads.

## Quick Start

```bash
make
./lazydockercpp
```

## Features

- Lists all containers with ID, name, image, status, CPU%, and memory
- Color-coded status (green=running, yellow=paused, red=exited)

## Keyboard Controls

| Key | Action |
|-----|--------|
| j/k | Move selection down/up |
| s | Start selected container |
| x | Stop selected container |
| r | Restart selected container |
| l | View container logs (piped to `less`) |
| i | Inspect container (`docker inspect`, piped to `less`) |
| a | Toggle show all containers / running only |
| q | Quit |

## Limitations

- No image management (pull, remove, inspect images)
- No volume or network management
- No Docker Compose integration
- No resource usage graph

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make, `docker` in PATH
