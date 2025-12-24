#[=======================================================================[.rst:
OrteafMetalKernels
------------------

Compile Metal kernels under ``orteaf/src/extension/kernel/mps`` into metallib
blobs and expose them to ORTEAF targets.
#]=======================================================================]

function(orteaf_add_metal_kernel_binaries)
    if(NOT ENABLE_MPS)
        set(ORTEAF_METAL_EMBED_SOURCE "" PARENT_SCOPE)
        set(ORTEAF_METAL_EMBED_OBJECTS "" PARENT_SCOPE)
        set(ORTEAF_METAL_GENERATED_LIBS "" PARENT_SCOPE)
        return()
    endif()

    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        message(WARNING "[ORTEAF][Metal] Embedding is supported only on macOS")
        add_custom_target(orteaf_metal_kernel_binaries)
        set(ORTEAF_METAL_EMBED_SOURCE "" PARENT_SCOPE)
        set(ORTEAF_METAL_EMBED_OBJECTS "" PARENT_SCOPE)
        set(ORTEAF_METAL_GENERATED_LIBS "" PARENT_SCOPE)
        return()
    endif()

    find_program(XCRUN_EXECUTABLE xcrun REQUIRED)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env TOOLCHAINS=metal "${XCRUN_EXECUTABLE}" -sdk macosx -find metal
        RESULT_VARIABLE _metal_find_result
        OUTPUT_VARIABLE _metal_tool
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _metal_find_result EQUAL 0 OR _metal_tool STREQUAL "")
        message(WARNING "[ORTEAF][Metal] 'xcrun metal' unavailable; skipping embedded kernels. Run scripts/setup-mps.sh to install the Metal toolchain.")
        add_custom_target(orteaf_metal_kernel_binaries)
        set(ORTEAF_METAL_EMBED_SOURCE "" PARENT_SCOPE)
        set(ORTEAF_METAL_EMBED_OBJECTS "" PARENT_SCOPE)
        set(ORTEAF_METAL_GENERATED_LIBS "" PARENT_SCOPE)
        return()
    endif()

    set(_kernel_root "${ORTEAF_SOURCE_ROOT}/src/extension/kernel/mps/impl")
    file(GLOB_RECURSE METAL_KERNEL_SOURCES
        "${_kernel_root}/*.metal"
    )
    if(NOT METAL_KERNEL_SOURCES)
        message(STATUS "[ORTEAF][Metal] No .metal sources under ${_kernel_root}")
    endif()

    set(_output_dir "${BACKEND_KERNEL_GEN_DIR}/mps/kernels")
    set(_module_cache_dir "${BACKEND_KERNEL_GEN_DIR}/mps/module_cache")
    file(MAKE_DIRECTORY "${_output_dir}")
    file(MAKE_DIRECTORY "${_module_cache_dir}")

    set(ALL_METALLIBS)
    foreach(metal_file IN LISTS METAL_KERNEL_SOURCES)
        get_filename_component(kernel_raw "${metal_file}" NAME_WE)
        string(REPLACE "-" "_" kernel_norm "${kernel_raw}")
        set(air_file "${_output_dir}/${kernel_norm}.air")
        set(metallib_file "${_output_dir}/${kernel_norm}.metallib")

        add_custom_command(
            OUTPUT "${metallib_file}"
            COMMAND ${CMAKE_COMMAND} -E env TOOLCHAINS=metal "${XCRUN_EXECUTABLE}" -sdk macosx metal
                -fmodules
                -fmodules-cache-path="${_module_cache_dir}"
                -c "${metal_file}"
                -o "${air_file}"
                -I"$<SHELL_PATH:${CMAKE_SOURCE_DIR}>"
            COMMAND ${CMAKE_COMMAND} -E env TOOLCHAINS=metal "${XCRUN_EXECUTABLE}" -sdk macosx metallib
                "${air_file}"
                -o "${metallib_file}"
            DEPENDS "${metal_file}"
            COMMENT "[ORTEAF][Metal] Generating metallib ${kernel_norm}.metallib"
            VERBATIM
        )
        list(APPEND ALL_METALLIBS "${metallib_file}")
    endforeach()

    if(NOT ALL_METALLIBS)
        message(STATUS "[ORTEAF][Metal] No metallib binaries generated (empty registry will be produced)")
    endif()

    set(EMBEDDED_OBJECTS)
    set(KERNEL_RECORDS)
    foreach(metallib_file IN LISTS ALL_METALLIBS)
        get_filename_component(bin_base "${metallib_file}" NAME_WE)
        string(REPLACE "-" "_" kernel_norm "${bin_base}")
        set(obj_file "${metallib_file}.o")

        set(asm_file "${metallib_file}.S")
        file(WRITE  "${asm_file}" ".section __DATA,__orteaf_kernels,regular,no_dead_strip\n")
        file(APPEND "${asm_file}" ".p2align 4\n")
        file(APPEND "${asm_file}" ".globl _orteaf_kernels_mps_${kernel_norm}_start\n")
        file(APPEND "${asm_file}" "_orteaf_kernels_mps_${kernel_norm}_start:\n")
        file(APPEND "${asm_file}" ".incbin \"${metallib_file}\"\n")
        file(APPEND "${asm_file}" ".globl _orteaf_kernels_mps_${kernel_norm}_end\n")
        file(APPEND "${asm_file}" "_orteaf_kernels_mps_${kernel_norm}_end:\n")

        add_custom_command(
            OUTPUT "${obj_file}"
            COMMAND "${XCRUN_EXECUTABLE}" -sdk macosx clang -c "${asm_file}" -o "${obj_file}"
            DEPENDS "${metallib_file}" "${asm_file}"
            COMMENT "[ORTEAF][Metal] Embedding ${bin_base}.metallib"
            VERBATIM
        )
        list(APPEND EMBEDDED_OBJECTS "${obj_file}")
        list(APPEND KERNEL_RECORDS "${kernel_norm}:orteaf_kernels_mps_${kernel_norm}:${obj_file}")
    endforeach()

    string(REPLACE ";" "|" KERNEL_RECORDS_SERIALIZED "${KERNEL_RECORDS}")
    get_filename_component(GENERATED_SOURCE_CANONICAL
        "${BACKEND_KERNEL_GEN_DIR}/mps/metal_kernel_registry_entries.h"
        ABSOLUTE
    )
    add_custom_command(
        OUTPUT
            "${GENERATED_SOURCE_CANONICAL}"
        COMMAND "${CMAKE_COMMAND}"
            -DOUTPUT:PATH=${GENERATED_SOURCE_CANONICAL}
            -DKERNEL_RECORDS:STRING=${KERNEL_RECORDS_SERIALIZED}
            -P "${CMAKE_SOURCE_DIR}/cmake/modules/OrteafMetalKernelEmbedGenerate.cmake"
        DEPENDS
            ${EMBEDDED_OBJECTS}
            "${CMAKE_SOURCE_DIR}/cmake/modules/OrteafMetalKernelEmbedGenerate.cmake"
        COMMENT "[ORTEAF][Metal] Generating kernel registry source"
        VERBATIM
    )

    add_custom_target(orteaf_metal_kernel_binaries
        DEPENDS
            ${EMBEDDED_OBJECTS}
            "${GENERATED_SOURCE_CANONICAL}"
    )

    set(ORTEAF_METAL_EMBED_SOURCE "${CMAKE_SOURCE_DIR}/orteaf/src/internal/execution/mps/platform/wrapper/metal_kernel_embed_registry.mm" PARENT_SCOPE)
    set(ORTEAF_METAL_EMBED_HEADER "${GENERATED_SOURCE_CANONICAL}" PARENT_SCOPE)
    set(ORTEAF_METAL_EMBED_OBJECTS "${EMBEDDED_OBJECTS}" PARENT_SCOPE)
    set(ORTEAF_METAL_GENERATED_LIBS "${ALL_METALLIBS}" PARENT_SCOPE)
endfunction()
