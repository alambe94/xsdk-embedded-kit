cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" xSDK_VERSION_RAW)
string(STRIP "${xSDK_VERSION_RAW}" xSDK_VERSION)

project(xsdk
    VERSION ${xSDK_VERSION}
    DESCRIPTION "eXtensible Embedded SDK"
    LANGUAGES C
)

add_compile_definitions(
    xSDK_VERSION_STRING="${xSDK_VERSION}"
    xSDK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    xSDK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    xSDK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/xsdk_options.cmake)
include(cmake/xsdk_instrumentation.cmake)
include(cmake/xsdk_warnings.cmake)
include(cmake/xsdk_testing.cmake)

include(cmake/xsdk_qemu.cmake)

add_subdirectory(src)
