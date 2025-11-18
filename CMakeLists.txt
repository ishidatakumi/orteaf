cmake_minimum_required(VERSION 3.25)

# Runtime enable flags (must be set before project() for conditional language enable)
option(ENABLE_CPU "Enable CPU runtime" ON)
option(ENABLE_CUDA "Enable CUDA runtime" OFF)
option(ENABLE_MPS "Enable Metal (MPS) runtime" OFF)
option(ORTEAF_ENABLE_TEST "Enable internal test instrumentation code" OFF)

# Set languages based on enabled backends
set(LANGUAGES CXX)
if(ENABLE_CUDA)
    list(APPEND LANGUAGES CUDA)
    # Respect user-provided -DCMAKE_CUDA_ARCHITECTURES, otherwise give CMake's modern
    # auto-selection so the list tracks the installed toolkit/GPU support.
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
        if(CMAKE_VERSION VERSION_LESS "3.24")
            # Older CMake (e.g., 3.22 in the base container) does not support "all-major".
            set(_orteaf_default_cuda_archs "75;80;86;89;90")
        else()
            set(_orteaf_default_cuda_archs "all-major")
        endif()
        set(CMAKE_CUDA_ARCHITECTURES "${_orteaf_default_cuda_archs}" CACHE STRING "CUDA architectures to build for" FORCE)
    endif()
endif()

project(orteaf VERSION 0.1.0 LANGUAGES ${LANGUAGES})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ENABLE_CUDA)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_EXTENSIONS OFF)
endif()

set(ORTEAF_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/orteaf CACHE INTERNAL "")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(OrteafCudaKernels)
include(OrteafMetalKernels)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/orteaf/include/orteaf/version/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/version/version.h
    @ONLY
)

# Propagate generated include directory so both library and tests can pick it up.
set(ORTEAF_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated CACHE INTERNAL "")
set(DTYPE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/dtype CACHE INTERNAL "")
set(OPS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/ops CACHE INTERNAL "")
set(BACKEND_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/backend CACHE INTERNAL "")
set(BACKEND_KERNEL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/backend_embed CACHE INTERNAL "")
set(ARCHITECTURE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/architecture CACHE INTERNAL "")
set(DEVICE_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/device CACHE INTERNAL "")

orteaf_add_cuda_kernel_binaries()
orteaf_add_metal_kernel_binaries()

add_subdirectory(tools/codegen)

add_custom_command(
    OUTPUT
        ${DTYPE_GEN_DIR}/dtype.def
        ${DTYPE_GEN_DIR}/dtype_tables.h
    COMMAND $<TARGET_FILE:gen_dtypes> ${CMAKE_CURRENT_SOURCE_DIR}/configs/dtype/dtypes.yml ${DTYPE_GEN_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/dtype/dtypes.yml
        gen_dtypes
    COMMENT "Generating dtype metadata"
    VERBATIM
)

add_custom_target(generate_dtypes
    DEPENDS
        ${DTYPE_GEN_DIR}/dtype.def
        ${DTYPE_GEN_DIR}/dtype_tables.h
)

add_custom_command(
    OUTPUT
        ${OPS_GEN_DIR}/ops.def
        ${OPS_GEN_DIR}/ops_tables.h
    COMMAND $<TARGET_FILE:gen_ops> ${CMAKE_CURRENT_SOURCE_DIR}/configs/ops/ops.yml ${OPS_GEN_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/ops/ops.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/dtype/dtypes.yml
        gen_ops
    COMMENT "Generating op metadata"
    VERBATIM
)

add_custom_target(generate_ops
    DEPENDS
        ${OPS_GEN_DIR}/ops.def
        ${OPS_GEN_DIR}/ops_tables.h
)

add_custom_command(
    OUTPUT
        ${BACKEND_GEN_DIR}/backend.def
        ${BACKEND_GEN_DIR}/backend_tables.h
    COMMAND $<TARGET_FILE:gen_backends> ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml ${BACKEND_GEN_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml
        gen_backends
    COMMENT "Generating backend metadata"
    VERBATIM
)

add_custom_target(generate_backends
    DEPENDS
        ${BACKEND_GEN_DIR}/backend.def
        ${BACKEND_GEN_DIR}/backend_tables.h
)

add_custom_command(
    OUTPUT
        ${ARCHITECTURE_GEN_DIR}/architecture.def
        ${ARCHITECTURE_GEN_DIR}/architecture_tables.h
    COMMAND $<TARGET_FILE:gen_architectures>
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/architecture/architectures.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml
            ${ARCHITECTURE_GEN_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/architecture/architectures.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml
        gen_architectures
    COMMENT "Generating architecture metadata"
    VERBATIM
)

add_custom_target(generate_architectures
    DEPENDS
        ${ARCHITECTURE_GEN_DIR}/architecture.def
        ${ARCHITECTURE_GEN_DIR}/architecture_tables.h
)

add_custom_command(
    OUTPUT
        ${DEVICE_GEN_DIR}/device.def
        ${DEVICE_GEN_DIR}/device_tables.h
    COMMAND $<TARGET_FILE:gen_devices>
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/device/devices.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/architecture/architectures.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/dtype/dtypes.yml
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/ops/ops.yml
            ${DEVICE_GEN_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/device/devices.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/backend/backends.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/architecture/architectures.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/dtype/dtypes.yml
        ${CMAKE_CURRENT_SOURCE_DIR}/configs/ops/ops.yml
        gen_devices
    COMMENT "Generating device metadata"
    VERBATIM
)

add_custom_target(generate_devices
    DEPENDS
        ${DEVICE_GEN_DIR}/device.def
        ${DEVICE_GEN_DIR}/device_tables.h
)

# Warn if CPU backend is disabled (unusual)
if(NOT ENABLE_CPU)
    message(WARNING 
        "CPU backend is disabled. "
        "This is unusual and may cause compatibility issues. "
        "Most code expects CPU backend to be available."
    )
endif()

# Enable CTest at the top level so `ctest` from the build dir works.
enable_testing()

# Statistics level configuration (shared by library and tests)
set(ORTEAF_STATS_LEVEL "OFF" CACHE STRING
    "Default statistics level (STATS_BASIC, STATS_EXTENDED, OFF)")
set_property(CACHE ORTEAF_STATS_LEVEL PROPERTY STRINGS STATS_BASIC STATS_EXTENDED OFF)

foreach(_category CPU CUDA MPS CORE)
    set(ORTEAF_STATS_LEVEL_${_category} "AUTO" CACHE STRING
        "Statistics level override for ${_category} (AUTO inherits ORTEAF_STATS_LEVEL)")
    set_property(CACHE ORTEAF_STATS_LEVEL_${_category} PROPERTY STRINGS AUTO STATS_BASIC STATS_EXTENDED OFF)
endforeach()

function(orteaf_stats_level_value out_var level_string)
    string(TOUPPER "${level_string}" _level_upper)
    if(_level_upper STREQUAL "STATS_BASIC")
        set(_value 2)
    elseif(_level_upper STREQUAL "STATS_EXTENDED")
        set(_value 4)
    elseif(_level_upper STREQUAL "OFF")
        set(_value 8)
    else()
        message(FATAL_ERROR "Unknown stats level: ${level_string}")
    endif()
    set(${out_var} ${_value} PARENT_SCOPE)
endfunction()

orteaf_stats_level_value(ORTEAF_STATS_LEVEL_GLOBAL_VALUE_NUMERIC "${ORTEAF_STATS_LEVEL}")

function(orteaf_resolve_stats_level out_var override_value)
    if("${override_value}" STREQUAL "AUTO")
        set(${out_var} ${ORTEAF_STATS_LEVEL_GLOBAL_VALUE_NUMERIC} PARENT_SCOPE)
    else()
        orteaf_stats_level_value(_resolved "${override_value}")
        set(${out_var} ${_resolved} PARENT_SCOPE)
    endif()
endfunction()

foreach(_category CPU CUDA MPS CORE)
    orteaf_resolve_stats_level(ORTEAF_STATS_LEVEL_${_category}_VALUE_NUMERIC "${ORTEAF_STATS_LEVEL_${_category}}")
endforeach()

set(ORTEAF_LOG_LEVEL "INFO" CACHE STRING
    "Default log level (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL, OFF)")
set_property(CACHE ORTEAF_LOG_LEVEL PROPERTY STRINGS TRACE DEBUG INFO WARN ERROR CRITICAL OFF)

foreach(_category CORE TENSOR CUDA MPS IO)
    set(ORTEAF_LOG_LEVEL_${_category} "AUTO" CACHE STRING
        "Log level override for ${_category} (AUTO inherits ORTEAF_LOG_LEVEL)")
    set_property(CACHE ORTEAF_LOG_LEVEL_${_category} PROPERTY STRINGS AUTO TRACE DEBUG INFO WARN ERROR CRITICAL OFF)
endforeach()

function(orteaf_log_level_value out_var level_string)
    string(TOUPPER "${level_string}" _level_upper)
    if(_level_upper STREQUAL "TRACE")
        set(_value 0)
    elseif(_level_upper STREQUAL "DEBUG")
        set(_value 1)
    elseif(_level_upper STREQUAL "INFO")
        set(_value 2)
    elseif(_level_upper STREQUAL "WARN")
        set(_value 3)
    elseif(_level_upper STREQUAL "ERROR")
        set(_value 4)
    elseif(_level_upper STREQUAL "CRITICAL")
        set(_value 5)
    elseif(_level_upper STREQUAL "OFF")
        set(_value 6)
    else()
        message(FATAL_ERROR "Unknown log level: ${level_string}")
    endif()
    set(${out_var} ${_value} PARENT_SCOPE)
endfunction()

orteaf_log_level_value(ORTEAF_LOG_LEVEL_GLOBAL_VALUE_NUMERIC "${ORTEAF_LOG_LEVEL}")

function(orteaf_resolve_log_level out_var override_value)
    if("${override_value}" STREQUAL "AUTO")
        set(${out_var} ${ORTEAF_LOG_LEVEL_GLOBAL_VALUE_NUMERIC} PARENT_SCOPE)
    else()
        orteaf_log_level_value(_resolved "${override_value}")
        set(${out_var} ${_resolved} PARENT_SCOPE)
    endif()
endfunction()

foreach(_category CORE TENSOR CUDA MPS IO)
    orteaf_resolve_log_level(ORTEAF_LOG_LEVEL_${_category}_VALUE_NUMERIC "${ORTEAF_LOG_LEVEL_${_category}}")
endforeach()

function(orteaf_bool_to_int out_var value)
    if(${value})
        set(${out_var} 1 PARENT_SCOPE)
    else()
        set(${out_var} 0 PARENT_SCOPE)
    endif()
endfunction()

orteaf_bool_to_int(ORTEAF_ENABLE_CPU_INT ENABLE_CPU)
orteaf_bool_to_int(ORTEAF_ENABLE_CUDA_INT ENABLE_CUDA)
orteaf_bool_to_int(ORTEAF_ENABLE_MPS_INT ENABLE_MPS)
orteaf_bool_to_int(ORTEAF_ENABLE_TEST_INT ORTEAF_ENABLE_TEST)

add_subdirectory(orteaf)
add_dependencies(orteaf generate_backends generate_ops generate_dtypes generate_architectures generate_devices)

if(TARGET orteaf)
    if(ORTEAF_CUDA_EMBED_SOURCE)
        if(TARGET orteaf_cuda_kernel_binaries)
            add_dependencies(orteaf orteaf_cuda_kernel_binaries)
        endif()
        target_compile_definitions(orteaf PUBLIC
            ORTEAF_EMBED_HAS_FATBIN=${ORTEAF_CUDA_EMBED_HAS_FATBIN}
            ORTEAF_EMBED_HAS_CUBIN=${ORTEAF_CUDA_EMBED_HAS_CUBIN}
            ORTEAF_EMBED_HAS_PTX=${ORTEAF_CUDA_EMBED_HAS_PTX}
        )
        if(TARGET orteaf_extension)
            target_compile_definitions(orteaf_extension INTERFACE
                ORTEAF_EMBED_HAS_FATBIN=${ORTEAF_CUDA_EMBED_HAS_FATBIN}
                ORTEAF_EMBED_HAS_CUBIN=${ORTEAF_CUDA_EMBED_HAS_CUBIN}
                ORTEAF_EMBED_HAS_PTX=${ORTEAF_CUDA_EMBED_HAS_PTX}
            )
        endif()
    endif()

    if(ORTEAF_METAL_EMBED_SOURCE AND TARGET orteaf_metal_kernel_binaries)
        add_dependencies(orteaf orteaf_metal_kernel_binaries)
    endif()
endif()

if(ORTEAF_ENABLE_TEST)
    add_subdirectory(tests)
    if(TARGET orteaf_tests)
        add_dependencies(orteaf_tests generate_backends generate_ops generate_dtypes generate_architectures generate_devices)
    endif()
endif()
