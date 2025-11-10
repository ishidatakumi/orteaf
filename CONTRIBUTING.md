# Contributing to ORTEAF

Thank you for your interest in contributing to ORTEAF!  
This document describes the contribution workflow, coding style, and review process for the project.

---

## Development Setup
To build ORTEAF locally:

```bash
git clone https://github.com/WARE10sai/orteaf.git
cd orteaf
mkdir build && cd build
cmake -G Ninja ..
ninja
```

For repeatable toolchains, the repository provides helper scripts and containers:

- `scripts/setup-cpu.sh`: installs clang/LLVM, CMake, Ninja, Doxygen, yaml-cpp (0.8.0), etc. on Linux/macOS hosts.
- `scripts/setup-cuda.sh`: extends the CPU setup on Linux and verifies CUDA drivers/toolkit (`nvidia-smi`, `nvcc`, `llvm-objcopy`).
- `scripts/setup-mps.sh`: prepares macOS hosts for Metal/MPS development (Homebrew deps + Metal toolchain download).
- `docker/run-cpu.sh`, `docker/run-cuda.sh`: build/run containers with the CPU or CUDA stacks (`docker run ... --gpus=all` handled for you).

See [docs/developer/environment.md](docs/developer/environment.md) for details.

### Directory structure
- `orteaf/include/orteaf/user`: User-facing wrapper API (`Tensor`, `Model`, etc.)
- `orteaf/include/orteaf/extension`: Extension points (`Kernel`, `Ops`, `TensorImpl`)
- `orteaf/src/extension/kernel/<backend>`: Backend-specific CUDA/MPS kernels that get embedded via CMake.
- `orteaf/include/orteaf/internal`: Runtime implementation (do not edit unless necessary)
- `orteaf/src/...`: Implementation mirrored to the include layout
- `tests/`: Unit and integration tests

---

## Editing boundaries
- **User**: Stable API wrapper. When adding new types or changing interfaces, please be mindful of backward compatibility.
- **Extension**: Area for adding new Kernel / Ops / TensorImpl / ModuleImpl. Update the design documents when making changes here.
- **Internal**: Implementations such as `SystemManager`, `Allocator`, and `Dispatcher`. Edit only for bug fixes or major improvements and ensure sufficient discussion takes place during review.

---

## Coding Style
- Use **C++20**
- Format code using **clang-format**
- Use `.h` for header files only
- Follow RAII and const correctness
- Avoid exposing platform-dependent headers in public APIs
- Namespace: lowercase (e.g. `orteaf::runtime`)
- Class names: PascalCase
- Function names: camelCase
- Use Doxygen comments for public interfaces

---

## Testing
All test code is located in the `tests/` directory.  
Please ensure new components include corresponding tests.

```bash
ctest --output-on-failure
```

---

## Pull Request Workflow
1. Fork the repository and create a new branch.
2. Make your changes and ensure tests pass.
3. Run `clang-format` before committing.
4. Write clear and concise commit messages.
5. Open a pull request for review.
