# ORTEAF
ORTEAF(Orchestrated Tensor Execution Adapter Framework) is an orchestration framework that unifies multiple compute APIs (CUDA/MPS/CPU) under a common tensor layer.

## 🧠 Philosophy

ORTEAF is designed primarily to support **Spiking Neural Networks (SNNs)**, enabling efficient learning and inference even on edge devices.  
The goal is to achieve high-speed, low-power computation while maintaining flexibility across heterogeneous hardware.

## 🔍 Motivation

To realize the above philosophy, ORTEAF implements a custom learning algorithm called **JBB (Jacobian-Based Backpropagation)**.  
This framework serves as the backbone for the research and engineering required to bring JBB into practical use.

## 🚀 Key Features

- **Unified Tensor Layer:**  
  Provides a consistent interface across CUDA, MPS, and CPU runtimes.
- **JBB Integration:**  
  Natively supports the JBB (Jacobian-Based Backpropagation) algorithm.
- **SNN Optimization:**  
  Designed for high-speed inference and learning on resource-constrained devices.
- **PyTorch-like API:**  
  Easy to learn and use; familiar structure for PyTorch users.
- **Core Component & Extensible:**  
  Clean dependency graph and loosely coupled core components make contribution easy.


## ⚙️ Installation

### Using CMake with Ninja
```bash
git clone https://github.com/yourname/orteaf.git
cd orteaf
mkdir build && cd build
cmake -G Ninja ..
ninja
```

### Enabling or Disabling CUDA and MPS

You can control which backends are enabled at build time through CMake options.

```bash
cmake -G Ninja .. -DENABLE_CUDA=ON -DENABLE_MPS=OFF
```

| Option | Description | Default |
|---------|-------------|----------|
| `ENABLE_CPU` | Enable CPU runtime | ON |
| `ENABLE_CUDA` | Enable CUDA runtime | OFF |
| `ENABLE_MPS` | Enable Metal (MPS) runtime | OFF |

If both are disabled, the build will default to the CPU runtime.

## 🗂 Documentation

- Core architecture and access boundaries: [docs/developer/design.md](docs/developer/design.md)
- Extension entry points (`Kernel`, `Ops`, `TensorImpl`): [docs/developer/extension-guide.md](docs/developer/extension-guide.md)
- Roadmap / challenge log templates: see [docs/README.md](docs/README.md)

## ⚙️ Configuration Options

- Configure statistics levels: `-DORTEAF_STATS_LEVEL=STATS_BASIC` (`STATS_BASIC`, `STATS_EXTENDED`, `OFF`)
- Per-component overrides inherit from the global level unless specified:  
  `-DORTEAF_STATS_LEVEL_CPU=AUTO`, `-DORTEAF_STATS_LEVEL_MPS=STATS_EXTENDED`, `-DORTEAF_STATS_LEVEL_CORE=STATS_BASIC`
- Configure logging at build time: `-DORTEAF_LOG_LEVEL=INFO`, per-category overrides via  
  `-DORTEAF_LOG_LEVEL_CORE=AUTO`, `-DORTEAF_LOG_LEVEL_CUDA=TRACE`, etc.
- Unified build environment (Docker): see [docs/developer/environment.md](docs/developer/environment.md)

## 📜 License
ORTEAF is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.
