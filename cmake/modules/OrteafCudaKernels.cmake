#[=======================================================================[.rst:
OrteafCudaKernels
-----------------

Compile CUDA kernels under ``orteaf/src/extension/kernel/cuda/impl`` into
embedded binaries and emit a registry translation unit that can be linked into
ORTEAF targets. Inspired by the BITsai pipeline but adapted to the ORTEAF tree.
#]=======================================================================]

function(_orteaf_cuda_detect_formats out_has_fatbin out_has_cubin out_has_ptx)
    set(_formats "${ORTEAF_CUDA_KERNEL_FORMATS}")
    if(_formats STREQUAL "")
        set(_formats "fatbin")
    endif()

    set(_has_fatbin OFF)
    set(_has_cubin OFF)
    set(_has_ptx OFF)

    foreach(fmt ${_formats})
        string(TOLOWER "${fmt}" _fmt_lower)
        if(_fmt_lower STREQUAL "fatbin")
            set(_has_fatbin ON)
        elseif(_fmt_lower STREQUAL "cubin")
            set(_has_cubin ON)
        elseif(_fmt_lower STREQUAL "ptx")
            set(_has_ptx ON)
        else()
            message(WARNING "Unknown CUDA kernel format '${fmt}' ignored (expected fatbin/cubin/ptx)")
        endif()
    endforeach()

    if(NOT _has_fatbin AND NOT _has_cubin AND NOT _has_ptx)
        message(FATAL_ERROR "ORTEAF_CUDA_KERNEL_FORMATS must include fatbin, cubin, or ptx")
    endif()

    foreach(pair
            "_has_fatbin;${out_has_fatbin}"
            "_has_cubin;${out_has_cubin}"
            "_has_ptx;${out_has_ptx}")
        list(GET pair 0 _flag_var)
        list(GET pair 1 _out_var)
        if(${_flag_var})
            set(${_out_var} 1 PARENT_SCOPE)
        else()
            set(${_out_var} 0 PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

function(orteaf_add_cuda_kernel_binaries)
    if(NOT ENABLE_CUDA)
        set(ORTEAF_CUDA_EMBED_SOURCE "" PARENT_SCOPE)
        set(ORTEAF_CUDA_EMBED_OBJECTS "" PARENT_SCOPE)
        set(ORTEAF_CUDA_GENERATED_BINARIES "" PARENT_SCOPE)
        set(ORTEAF_CUDA_EMBED_HAS_FATBIN 0 PARENT_SCOPE)
        set(ORTEAF_CUDA_EMBED_HAS_CUBIN 0 PARENT_SCOPE)
        set(ORTEAF_CUDA_EMBED_HAS_PTX 0 PARENT_SCOPE)
        return()
    endif()

    if(NOT DEFINED ORTEAF_CUDA_KERNEL_FORMATS)
        set(ORTEAF_CUDA_KERNEL_FORMATS "fatbin" CACHE STRING
            "Semicolon-separated list of CUDA kernel formats to embed (fatbin;cubin;ptx)")
    endif()

    _orteaf_cuda_detect_formats(
        _has_fatbin_int
        _has_cubin_int
        _has_ptx_int
    )

    find_program(LLVM_OBJCOPY_EXECUTABLE llvm-objcopy REQUIRED)

    set(_kernel_root "${ORTEAF_SOURCE_ROOT}/src/extension/kernel/cuda/impl")
    file(GLOB_RECURSE CUDA_KERNEL_SOURCES
        "${_kernel_root}/*.cu"
    )
    if(NOT CUDA_KERNEL_SOURCES)
        message(STATUS "[ORTEAF][CUDA] No kernel sources under ${_kernel_root}")
    endif()

    set(_kernel_bin_dir "${BACKEND_GEN_DIR}/cuda/kernels")
    file(MAKE_DIRECTORY "${_kernel_bin_dir}")

    set(ALL_BINARIES)
    foreach(cu_file IN LISTS CUDA_KERNEL_SOURCES)
        get_filename_component(kernel_raw "${cu_file}" NAME_WE)
        string(REPLACE "-" "_" kernel_norm "${kernel_raw}")

        if(_has_fatbin_int EQUAL 1)
            set(fatbin_file "${_kernel_bin_dir}/${kernel_norm}.fatbin")
            add_custom_command(
                OUTPUT "${fatbin_file}"
                COMMAND "${CMAKE_CUDA_COMPILER}"
                    --fatbin
                    --std=c++20
                    -I"${CMAKE_SOURCE_DIR}"
                    "${cu_file}"
                    -o "${fatbin_file}"
                DEPENDS "${cu_file}"
                COMMENT "[ORTEAF][CUDA] Generating FATBIN ${kernel_norm}.fatbin"
                VERBATIM
            )
            list(APPEND ALL_BINARIES "${fatbin_file}")
        endif()

        if(_has_cubin_int EQUAL 1)
            set(cubin_file "${_kernel_bin_dir}/${kernel_norm}.cubin")
            add_custom_command(
                OUTPUT "${cubin_file}"
                COMMAND "${CMAKE_CUDA_COMPILER}"
                    --cubin
                    -arch=sm_80
                    --std=c++20
                    -I"${CMAKE_SOURCE_DIR}"
                    "${cu_file}"
                    -o "${cubin_file}"
                DEPENDS "${cu_file}"
                COMMENT "[ORTEAF][CUDA] Generating CUBIN ${kernel_norm}.cubin"
                VERBATIM
            )
            list(APPEND ALL_BINARIES "${cubin_file}")
        endif()

        if(_has_ptx_int EQUAL 1)
            set(ptx_file "${_kernel_bin_dir}/${kernel_norm}.ptx")
            add_custom_command(
                OUTPUT "${ptx_file}"
                COMMAND "${CMAKE_CUDA_COMPILER}"
                    --ptx
                    -arch=compute_70
                    --std=c++20
                    -I"${CMAKE_SOURCE_DIR}"
                    "${cu_file}"
                    -o "${ptx_file}"
                DEPENDS "${cu_file}"
                COMMENT "[ORTEAF][CUDA] Generating PTX ${kernel_norm}.ptx"
                VERBATIM
            )
            list(APPEND ALL_BINARIES "${ptx_file}")
        endif()
    endforeach()

    if(NOT ALL_BINARIES)
        message(STATUS "[ORTEAF][CUDA] No kernel binaries generated (empty registry will be produced)")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
            set(_obj_target "mach-o-arm64")
        else()
            set(_obj_target "mach-o-x86-64")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
            set(_obj_target "elf64-littleaarch64")
        else()
            set(_obj_target "elf64-x86-64")
        endif()
    else()
        message(FATAL_ERROR "[ORTEAF][CUDA] Unsupported host system ${CMAKE_SYSTEM_NAME}")
    endif()

    set(EMBEDDED_OBJECTS)
    set(KERNEL_RECORDS)
    foreach(bin_file IN LISTS ALL_BINARIES)
        get_filename_component(bin_name "${bin_file}" NAME)
        string(REGEX MATCH "^(.*)\\.(fatbin|cubin|ptx)$" _ "${bin_name}")
        set(kernel_raw "${CMAKE_MATCH_1}")
        set(fmt "${CMAKE_MATCH_2}")
        string(REPLACE "-" "_" kernel_norm "${kernel_raw}")
        string(REPLACE "." "_" fmt_norm "${fmt}")
        set(obj_file "${bin_file}.o")

        string(REPLACE "/" "_" rel_base "${bin_file}")
        string(REPLACE "." "_" rel_base "${rel_base}")
        set(old_prefix "_binary_${rel_base}")
        set(new_prefix "orteaf_kernels_cuda_${kernel_norm}_${fmt_norm}")

        add_custom_command(
            OUTPUT "${obj_file}"
            COMMAND "${LLVM_OBJCOPY_EXECUTABLE}"
                --input-target=binary
                --output-target=${_obj_target}
                "${bin_file}" "${obj_file}"
            COMMAND "${LLVM_OBJCOPY_EXECUTABLE}"
                --redefine-sym "${old_prefix}_start=${new_prefix}_start"
                --redefine-sym "${old_prefix}_end=${new_prefix}_end"
                --redefine-sym "${old_prefix}_size=${new_prefix}_size"
                "${obj_file}"
            DEPENDS "${bin_file}"
            COMMENT "[ORTEAF][CUDA] Embedding ${kernel_raw}.${fmt}"
            VERBATIM
        )
        list(APPEND EMBEDDED_OBJECTS "${obj_file}")
        list(APPEND KERNEL_RECORDS "${kernel_raw}:${kernel_norm}:${fmt}:${new_prefix}:${obj_file}")
    endforeach()

    string(REPLACE ";" "|" KERNEL_RECORDS_SERIALIZED "${KERNEL_RECORDS}")
    set(GENERATED_SOURCE "${BACKEND_GEN_DIR}/cuda/kernel_registry.cpp")
    add_custom_command(
        OUTPUT "${GENERATED_SOURCE}"
        COMMAND "${CMAKE_COMMAND}"
            -DOUTPUT:PATH="${GENERATED_SOURCE}"
            -DKERNEL_RECORDS:STRING="${KERNEL_RECORDS_SERIALIZED}"
            -P "${CMAKE_SOURCE_DIR}/cmake/modules/OrteafKernelEmbedGenerate.cmake"
        DEPENDS
            ${EMBEDDED_OBJECTS}
            "${CMAKE_SOURCE_DIR}/cmake/modules/OrteafKernelEmbedGenerate.cmake"
        COMMENT "[ORTEAF][CUDA] Generating kernel registry source"
        VERBATIM
    )

    add_custom_target(orteaf_cuda_kernel_binaries
        DEPENDS ${EMBEDDED_OBJECTS} "${GENERATED_SOURCE}"
    )

    set(ORTEAF_CUDA_EMBED_SOURCE "${GENERATED_SOURCE}" PARENT_SCOPE)
    set(ORTEAF_CUDA_EMBED_OBJECTS "${EMBEDDED_OBJECTS}" PARENT_SCOPE)
    set(ORTEAF_CUDA_GENERATED_BINARIES "${ALL_BINARIES}" PARENT_SCOPE)
    set(ORTEAF_CUDA_EMBED_HAS_FATBIN ${_has_fatbin_int} PARENT_SCOPE)
    set(ORTEAF_CUDA_EMBED_HAS_CUBIN ${_has_cubin_int} PARENT_SCOPE)
    set(ORTEAF_CUDA_EMBED_HAS_PTX ${_has_ptx_int} PARENT_SCOPE)
endfunction()
