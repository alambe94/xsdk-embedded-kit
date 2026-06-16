add_library(xe_sdk_xtrace STATIC
    src/xtrace.c
    src/xtrace_cobs.c
)

add_library(xsdk::xtrace ALIAS xe_sdk_xtrace)

target_include_directories(xe_sdk_xtrace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xe_sdk_xtrace PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xe_sdk_xtrace)

# -- Dictionary generation integration -----------------------------------------

if(xSDK_GENERATE_TRACE_DICTIONARIES)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(XTRACE_GEN_DIR "${CMAKE_BINARY_DIR}/generated/xtrace_dictionaries")
    set(XRTOS_GEN_DICT "${XTRACE_GEN_DIR}/xrtos_dictionary.json")
    set(XFS_GEN_DICT "${XTRACE_GEN_DIR}/xfs_dictionary.json")
    set(XUSBD_GEN_DICT "${XTRACE_GEN_DIR}/xusbd_dictionary.json")
    set(XSDK_GEN_DICT "${XTRACE_GEN_DIR}/sdk_dictionary.json")
    set(XTRACE_DICT_MANIFEST "${CMAKE_SOURCE_DIR}/tools/xtrace/dictionaries.json")
    set(XTRACE_DICT_SET_TOOL "${CMAKE_SOURCE_DIR}/tools/xtrace/xtrace_dictionary_set.py")

    add_custom_command(
        OUTPUT "${XRTOS_GEN_DICT}" "${XFS_GEN_DICT}" "${XUSBD_GEN_DICT}" "${XSDK_GEN_DICT}"
        COMMAND Python3::Interpreter "${XTRACE_DICT_SET_TOOL}" generate
                --manifest "${XTRACE_DICT_MANIFEST}"
                --output-dir "${XTRACE_GEN_DIR}"
        DEPENDS "${XTRACE_DICT_SET_TOOL}"
                "${XTRACE_DICT_MANIFEST}"
                "${CMAKE_SOURCE_DIR}/tools/xtrace/xtrace_generator.py"
                "${CMAKE_SOURCE_DIR}/src/components/xrtos/include/xrtos_trace.h"
                "${CMAKE_SOURCE_DIR}/tools/xtrace/examples/xrtos_overlay.json"
                "${CMAKE_SOURCE_DIR}/src/components/xfs/include/xfs_trace.h"
                "${CMAKE_SOURCE_DIR}/tools/xtrace/examples/xfs_overlay.json"
                "${CMAKE_SOURCE_DIR}/src/components/xusb/device/include/xusbd_trace.h"
                "${CMAKE_SOURCE_DIR}/tools/xtrace/examples/xusbd_overlay.json"
                "${CMAKE_SOURCE_DIR}/src/components/xutil/xtrace/include/xtrace_registry.h"
        COMMENT "Generating xSDK trace dictionaries"
        VERBATIM
    )

    add_custom_target(xtrace_generate_dictionaries
        DEPENDS "${XRTOS_GEN_DICT}" "${XFS_GEN_DICT}" "${XUSBD_GEN_DICT}" "${XSDK_GEN_DICT}"
    )
endif()

if(xSDK_BUILD_TESTS)
    add_subdirectory(tests)
endif()
