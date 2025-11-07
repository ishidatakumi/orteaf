cmake_minimum_required(VERSION 3.20)

# Runtime enable flags (must be set before project() for conditional language enable)
option(ENABLE_CPU "Enable CPU runtime" ON)
option(ENABLE_CUDA "Enable CUDA runtime" OFF)
option(ENABLE_MPS "Enable Metal (MPS) runtime" OFF)

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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/orteaf/include/orteaf/version/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/orteaf/version/version.h
    @ONLY
)

# Propagate generated include directory so both library and tests can pick it up.
set(ORTEAF_GENERATED_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated CACHE INTERNAL "")

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

add_subdirectory(orteaf)
add_subdirectory(tests)
