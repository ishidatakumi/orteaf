# Metadata YAML & Code Generation Guide

The files under `configs/` define five catalogs (dtypes, ops, backends, architectures,
devices). Each catalog has a dedicated generator in `tools/codegen` that produces
constexpr tables in `build/generated/orteaf/**`, which are then consumed by the
public headers in `orteaf/include/orteaf/internal/**`.

After editing any YAML file, regenerate the metadata by running:

```bash
cmake --build build -j    # runs all generate_* targets
```

If you need API docs, run `doxygen Doxyfile` after the build. Every public header
contains short English summaries that show up in the generated documentation.

Below is a quick reference for each catalog, covering the YAML schema, generator,
public header, and unit tests.

---

## DTypes

- **YAML**: `configs/dtype/dtypes.yml`
- **Generator**: `tools/codegen/gen_dtypes.cpp` (`generate_dtypes`)
- **Public header**: `orteaf/include/orteaf/internal/dtype/dtype.h`
- **Tests**: `tests/internal/dtype/dtype_test.cpp`

```yaml
schema_version: "1.0"
dtypes:
  - id: "F32"                 # becomes DType::F32
    cpp_type: "float"
    display_name: "float32"
    category: "floating_point"
    promotion_priority: 600
    compute_dtype: "F32"
    implicit_cast_to: ["F64"]
    explicit_cast_to: ["F16", "I32"]
    metadata:
      description: "32-bit IEEE float"
      tags: ["native"]
```

`id` must be a valid C++ identifier because it feeds into both the enum and the
generated `.def` file. Promotion and cast matrices are derived from the lists
above, so remember to keep them in sync when adding new types.

---

## Ops

- **YAML**: `configs/ops/ops.yml`
- **Generator**: `tools/codegen/gen_ops.cpp` (`generate_ops`)
- **Public header**: `orteaf/include/orteaf/internal/ops/ops.h`
- **Tests**: `tests/internal/ops/ops_tables_test.cpp`

```yaml
schema_version: "1.0"
catalogs:
  categories:
    linear_algebra:
      description: "Matrix and tensor kernels"
  dtype_categories:
    floating_point: "IEEE754 types"
  shape_kinds:
    matmul: "Matrix multiplication semantics"

ops:
  - id: "MatMul"
    display_name: "MatMul"
    category: "linear_algebra"
    arity: 2
    inputs:
      - name: "lhs"
        dtype_constraints:
          mode: "allow"
          categories: ["floating_point"]
    outputs:
      - name: "output"
        dtype_rule:
          kind: "same_as"
          input: "lhs"
    compute_policy:
      kind: "derived_compute_type"
      function: "MatMulAccumulatorType"
    shape_inference:
      kind: "matmul"
      description: "Standard GEMM rule"
    metadata:
      description: "Batched matmul with optional bias"
      tags: ["linear_algebra"]
```

The generator validates every section (inputs, dtype rules, shape kinds, etc.) and
flattens the data into `ops_tables.h`. When adding or removing ops, update any
backend kernels that rely on the affected IDs.

---

## Backends

- **YAML**: `configs/backend/backends.yml`
- **Generator**: `tools/codegen/gen_backends.cpp` (`generate_backends`)
- **Public header**: `orteaf/include/orteaf/internal/backend/backend.h`
- **Tests**: `tests/internal/backend/backend_tables_test.cpp`

```yaml
schema_version: "1.0"

backends:
  - id: "Cuda"
    display_name: "CUDA"
    module_path: "@orteaf/internal/backend/cuda"
    metadata:
      description: "NVIDIA CUDA implementation"
```

Backend IDs are referenced by both the architecture and device catalogs, so keep
them stable. `module_path` determines where dispatcher code looks for the backend
implementation.

---

## Architectures

- **YAML**: `configs/architecture/architectures.yml`
- **Generator**: `tools/codegen/gen_architectures.cpp` (`generate_architectures`)
- **Public header**: `orteaf/include/orteaf/internal/architecture/architecture.h`
- **Tests**: `tests/internal/architecture/architecture_test.cpp`

```yaml
schema_version: "1.0"

architectures:
  - id: "Sm90"
    display_name: "CUDA SM90"
    backend: "Cuda"
    metadata:
      description: "Optimized kernels for Hopper GPUs"
```

Do not list the `Generic` architecture in YAML; the generator injects it and
assigns local index `0` for each backend. The remaining entries appear in the
order they are written, so keep the list sorted if that matters for humans.

---

## Devices

- **YAML**: `configs/device/devices.yml`
- **Generator**: `tools/codegen/gen_devices.cpp` (`generate_devices`)
- **Public header**: `orteaf/include/orteaf/internal/device/device.h`
- **Tests**: `tests/internal/device/device_tables_test.cpp`

```yaml
schema_version: "1.0"

devices:
  - id: "CudaH100Pcie80GB"
    display_name: "H100 PCIe (80GB)"
    backend: "Cuda"            # -> backends.yml
    architecture: "Sm90"       # -> architectures.yml
    memory:
      max_bytes: 85899345920
      shared_bytes: 229376
    supported_dtypes: ["F32", "F16", "F8E4M3", "F8E5M2"]  # -> dtypes.yml
    supported_ops: ["Add", "MatMul", "Relu"]              # -> ops.yml
    capabilities:
      compute_capability: "sm90"
      tensor_cores: "true"
    notes: "Use CUDA 12.2+ for best results"
```

Validation checks ensure that:

1. All referenced backend / architecture / dtype / op IDs exist.
2. The architecture belongs to the same backend as the device.
3. Lists avoid duplicate IDs.

Supported dtypes/ops are stored as flat arrays plus offsets, so order them the
way you want them to appear at runtime.

---

## Recommended workflow

1. Edit the YAML file(s).
2. Run `cmake --build build -j` so every `generate_*` target refreshes the tables.
3. Run `cd build && ctest --output-on-failure` to ensure the metadata tests still pass.
4. When submitting changes, mention the relationship between YAML → generator → header → tests so reviewers can follow the chain easily.
