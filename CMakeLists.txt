cmake_minimum_required(VERSION 3.20)

project(orteaf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Runtime enable flags
option(ENABLE_CPU "Enable CPU runtime" ON)
option(ENABLE_CUDA "Enable CUDA runtime" OFF)
option(ENABLE_MPS "Enable Metal (MPS) runtime" OFF)

# Enable CTest at the top level so `ctest` from the build dir works.
enable_testing()

# Statistics level configuration (shared by library and tests)
set(ORTEAF_RUNTIME_STATS_LEVEL 0 CACHE STRING
    "Default statistics level for runtimes and allocators (0=off, 1=basic, 2=extended)")
set_property(CACHE ORTEAF_RUNTIME_STATS_LEVEL PROPERTY STRINGS 0 1 2)

foreach(_component CPU CUDA MPS ALLOCATOR)
    set(ORTEAF_${_component}_STATS_LEVEL "AUTO" CACHE STRING
        "Statistics level override for ${_component} (AUTO inherits ORTEAF_RUNTIME_STATS_LEVEL)")
    set_property(CACHE ORTEAF_${_component}_STATS_LEVEL PROPERTY STRINGS AUTO 0 1 2)
endforeach()

function(orteaf_resolve_stats_level out_var override_value)
    if("${override_value}" STREQUAL "AUTO")
        set(${out_var} ${ORTEAF_RUNTIME_STATS_LEVEL} PARENT_SCOPE)
    else()
        set(${out_var} ${override_value} PARENT_SCOPE)
    endif()
endfunction()

orteaf_resolve_stats_level(ORTEAF_CPU_STATS_LEVEL_EFFECTIVE "${ORTEAF_CPU_STATS_LEVEL}")
orteaf_resolve_stats_level(ORTEAF_CUDA_STATS_LEVEL_EFFECTIVE "${ORTEAF_CUDA_STATS_LEVEL}")
orteaf_resolve_stats_level(ORTEAF_MPS_STATS_LEVEL_EFFECTIVE "${ORTEAF_MPS_STATS_LEVEL}")
orteaf_resolve_stats_level(ORTEAF_ALLOCATOR_STATS_LEVEL_EFFECTIVE "${ORTEAF_ALLOCATOR_STATS_LEVEL}")

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
