set(ORTEAF_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ORTEAF_INCLUDE_ROOT ${ORTEAF_SOURCE_ROOT}/include/orteaf)

set(ORTEAF_USER_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/user)
set(ORTEAF_EXTENSION_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/extension)
set(ORTEAF_INTERNAL_INCLUDE_DIR ${ORTEAF_INCLUDE_ROOT}/internal)
set(ORTEAF_VERSION_INCLUDE_DIR ${ORTEAF_GENERATED_INCLUDE_DIR}/orteaf/version)

add_library(orteaf_user INTERFACE)
target_include_directories(orteaf_user INTERFACE
    ${ORTEAF_USER_INCLUDE_DIR}
    ${ORTEAF_GENERATED_INCLUDE_DIR}
)

add_library(orteaf_extension INTERFACE)
target_include_directories(orteaf_extension INTERFACE
    ${ORTEAF_EXTENSION_INCLUDE_DIR}
    ${ORTEAF_USER_INCLUDE_DIR}
    ${ORTEAF_GENERATED_INCLUDE_DIR}
)
target_link_libraries(orteaf_extension INTERFACE orteaf_user)
target_compile_definitions(orteaf_extension INTERFACE
    ORTEAF_ENABLE_CPU=${ORTEAF_ENABLE_CPU_INT}
    ORTEAF_ENABLE_CUDA=${ORTEAF_ENABLE_CUDA_INT}
    ORTEAF_ENABLE_MPS=${ORTEAF_ENABLE_MPS_INT}
    ORTEAF_ENABLE_TEST=${ORTEAF_ENABLE_TEST_INT}
    ORTEAF_LOG_LEVEL_GLOBAL_VALUE=${ORTEAF_LOG_LEVEL_GLOBAL_VALUE_NUMERIC}
    ORTEAF_LOG_LEVEL_CORE_VALUE=${ORTEAF_LOG_LEVEL_CORE_VALUE_NUMERIC}
    ORTEAF_LOG_LEVEL_TENSOR_VALUE=${ORTEAF_LOG_LEVEL_TENSOR_VALUE_NUMERIC}
    ORTEAF_LOG_LEVEL_CUDA_VALUE=${ORTEAF_LOG_LEVEL_CUDA_VALUE_NUMERIC}
    ORTEAF_LOG_LEVEL_MPS_VALUE=${ORTEAF_LOG_LEVEL_MPS_VALUE_NUMERIC}
    ORTEAF_LOG_LEVEL_IO_VALUE=${ORTEAF_LOG_LEVEL_IO_VALUE_NUMERIC}
    ORTEAF_STATS_LEVEL_GLOBAL_VALUE=${ORTEAF_STATS_LEVEL_GLOBAL_VALUE_NUMERIC}
    ORTEAF_STATS_LEVEL_CPU_VALUE=${ORTEAF_STATS_LEVEL_CPU_VALUE_NUMERIC}
    ORTEAF_STATS_LEVEL_CUDA_VALUE=${ORTEAF_STATS_LEVEL_CUDA_VALUE_NUMERIC}
    ORTEAF_STATS_LEVEL_MPS_VALUE=${ORTEAF_STATS_LEVEL_MPS_VALUE_NUMERIC}
    ORTEAF_STATS_LEVEL_CORE_VALUE=${ORTEAF_STATS_LEVEL_CORE_VALUE_NUMERIC}
)

# Collect source files by backend
# Common files (always compiled) - all .cpp files
file(GLOB_RECURSE ORTEAF_SOURCES_COMMON CONFIGURE_DEPENDS
    ${ORTEAF_SOURCE_ROOT}/src/*.cpp
)

# CUDA backend sources - all .cu files in src/
set(ORTEAF_SOURCES_CUDA "")
if(ORTEAF_ENABLE_CUDA_INT)
    file(GLOB_RECURSE ORTEAF_SOURCES_CUDA CONFIGURE_DEPENDS
        ${ORTEAF_SOURCE_ROOT}/src/*.cu
    )
endif()

# MPS backend sources - all .mm files in src/
set(ORTEAF_SOURCES_MPS "")
if(ORTEAF_ENABLE_MPS_INT)
    file(GLOB_RECURSE ORTEAF_SOURCES_MPS CONFIGURE_DEPENDS
        ${ORTEAF_SOURCE_ROOT}/src/*.mm
    )
endif()

# Combine all sources
set(ORTEAF_SOURCES
    ${ORTEAF_SOURCES_COMMON}
    ${ORTEAF_SOURCES_CUDA}
    ${ORTEAF_SOURCES_MPS}
)

if(ORTEAF_SOURCES)
    add_library(orteaf STATIC ${ORTEAF_SOURCES})
else()
    add_library(orteaf STATIC)
endif()

target_link_libraries(orteaf
    PUBLIC
        orteaf_extension
)

# Link CUDA Runtime when CUDA is enabled
if(ORTEAF_ENABLE_CUDA_INT)
    find_package(CUDAToolkit REQUIRED)
    
    # Ensure the target inherits whatever CUDA architectures the parent configured.
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
        if(CMAKE_VERSION VERSION_LESS "3.24")
            set(_orteaf_default_cuda_archs "75;80;86;89;90")
        else()
            set(_orteaf_default_cuda_archs "all-major")
        endif()
        set(CMAKE_CUDA_ARCHITECTURES "${_orteaf_default_cuda_archs}" CACHE STRING "CUDA architectures to build for" FORCE)
    endif()
    set_property(TARGET orteaf PROPERTY CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}")
    
    target_link_libraries(orteaf PUBLIC CUDA::cudart CUDA::cuda_driver)
endif()

# macOS-specific frameworks
if(APPLE)
    target_link_libraries(orteaf
        PUBLIC
            "-framework CoreFoundation"
            "-framework IOKit"
    )
    if(ORTEAF_ENABLE_MPS_INT)
    target_link_libraries(orteaf
        PUBLIC
            "-framework Metal"
            "-framework MetalPerformanceShaders"
            "-framework MetalPerformanceShadersGraph"
            "-framework Foundation"
    )
endif()
endif()

target_include_directories(orteaf
    PRIVATE
        ${ORTEAF_INCLUDE_ROOT}
        ${ORTEAF_SOURCE_ROOT}/include
        ${ORTEAF_GENERATED_INCLUDE_DIR}
)
target_compile_definitions(orteaf
    PUBLIC
        ORTEAF_ENABLE_CPU=${ORTEAF_ENABLE_CPU_INT}
        ORTEAF_ENABLE_CUDA=${ORTEAF_ENABLE_CUDA_INT}
        ORTEAF_ENABLE_MPS=${ORTEAF_ENABLE_MPS_INT}
        ORTEAF_ENABLE_TEST=${ORTEAF_ENABLE_TEST_INT}
        ORTEAF_LOG_LEVEL_GLOBAL_VALUE=${ORTEAF_LOG_LEVEL_GLOBAL_VALUE_NUMERIC}
        ORTEAF_LOG_LEVEL_CORE_VALUE=${ORTEAF_LOG_LEVEL_CORE_VALUE_NUMERIC}
        ORTEAF_LOG_LEVEL_TENSOR_VALUE=${ORTEAF_LOG_LEVEL_TENSOR_VALUE_NUMERIC}
        ORTEAF_LOG_LEVEL_CUDA_VALUE=${ORTEAF_LOG_LEVEL_CUDA_VALUE_NUMERIC}
        ORTEAF_LOG_LEVEL_MPS_VALUE=${ORTEAF_LOG_LEVEL_MPS_VALUE_NUMERIC}
        ORTEAF_LOG_LEVEL_IO_VALUE=${ORTEAF_LOG_LEVEL_IO_VALUE_NUMERIC}
        ORTEAF_STATS_LEVEL_GLOBAL_VALUE=${ORTEAF_STATS_LEVEL_GLOBAL_VALUE_NUMERIC}
        ORTEAF_STATS_LEVEL_CPU_VALUE=${ORTEAF_STATS_LEVEL_CPU_VALUE_NUMERIC}
        ORTEAF_STATS_LEVEL_CUDA_VALUE=${ORTEAF_STATS_LEVEL_CUDA_VALUE_NUMERIC}
        ORTEAF_STATS_LEVEL_MPS_VALUE=${ORTEAF_STATS_LEVEL_MPS_VALUE_NUMERIC}
        ORTEAF_STATS_LEVEL_CORE_VALUE=${ORTEAF_STATS_LEVEL_CORE_VALUE_NUMERIC}
)

target_compile_features(orteaf PUBLIC cxx_std_20)

if(ORTEAF_CUDA_EMBED_SOURCE)
    set_source_files_properties(${ORTEAF_CUDA_EMBED_SOURCE} PROPERTIES GENERATED TRUE)
    target_sources(orteaf PRIVATE ${ORTEAF_CUDA_EMBED_SOURCE})
    if(ORTEAF_CUDA_EMBED_OBJECTS)
        foreach(obj ${ORTEAF_CUDA_EMBED_OBJECTS})
            set_source_files_properties(${obj}
                PROPERTIES
                    GENERATED TRUE
                    EXTERNAL_OBJECT TRUE
            )
        endforeach()
        target_sources(orteaf PRIVATE ${ORTEAF_CUDA_EMBED_OBJECTS})
    endif()
endif()

if(ORTEAF_METAL_EMBED_SOURCE)
    set_source_files_properties(${ORTEAF_METAL_EMBED_SOURCE} PROPERTIES GENERATED TRUE)
    target_sources(orteaf PRIVATE ${ORTEAF_METAL_EMBED_SOURCE})
    if(ORTEAF_METAL_EMBED_OBJECTS)
        foreach(obj ${ORTEAF_METAL_EMBED_OBJECTS})
            set_source_files_properties(${obj}
                PROPERTIES
                    GENERATED TRUE
                    EXTERNAL_OBJECT TRUE
            )
        endforeach()
        target_sources(orteaf PRIVATE ${ORTEAF_METAL_EMBED_OBJECTS})
    endif()
endif()
