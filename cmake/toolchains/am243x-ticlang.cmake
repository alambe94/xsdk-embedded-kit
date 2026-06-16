# AM243x R5FSS0-0 bare-metal toolchain - TI ARM Clang (tiarmclang)
#
# Extends r5-ticlang with AM243x SoC identification so CMake can
# conditionally include AM243x port files and applications.
#
# Usage:
#   cmake -S . -B build/am243x-ticlang \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/am243x-ticlang.cmake

include("${CMAKE_CURRENT_LIST_DIR}/r5-ticlang.cmake")

set(xSDK_SOC_AM243X TRUE CACHE BOOL "Build for TI AM243x SoC" FORCE)
