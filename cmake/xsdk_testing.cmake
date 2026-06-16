include_guard(GLOBAL)

# Keep testing enabled unconditionally so QEMU add_test() calls register even
# when host tests are disabled.
enable_testing()
if(xSDK_BUILD_TESTS)
    include(CTest)
endif()

function(xsdk_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBS;INCLUDES;DEFS")
    add_executable(${name} ${ARG_SOURCES})
    if(ARG_INCLUDES)
        target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    endif()
    if(ARG_DEFS)
        target_compile_definitions(${name} PRIVATE ${ARG_DEFS})
    endif()
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} unity)
    xsdk_apply_target_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
