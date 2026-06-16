# Cortex-R5 bare-metal toolchain - arm-none-eabi-gcc
#
# Targets: AM64x / AM243x Cortex-R5F (hard-float, little-endian)
#
# Usage:
#   cmake -S . -B build/r5-gcc -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/r5-gcc.cmake
#
# The arm-none-eabi-gcc bin directory must be on PATH before invoking cmake.
# Windows: xsdk.bat setup arm-gcc  (installs xPack toolchain to tools\arm_gcc\)
# Linux CI (apt): sudo apt-get install -y gcc-arm-none-eabi

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Tools are resolved from PATH - add the bin directory to PATH before invoking cmake.
set(_TC_PREFIX "arm-none-eabi-")

set(CMAKE_C_COMPILER   "${_TC_PREFIX}gcc"     CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER "${_TC_PREFIX}g++"     CACHE STRING "C++ compiler")
set(CMAKE_ASM_COMPILER "${_TC_PREFIX}gcc"     CACHE STRING "ASM compiler")
set(CMAKE_AR           "${_TC_PREFIX}ar"      CACHE STRING "Archiver")
set(CMAKE_RANLIB       "${_TC_PREFIX}ranlib"  CACHE STRING "Ranlib")
set(CMAKE_OBJCOPY      "${_TC_PREFIX}objcopy" CACHE STRING "objcopy")
set(CMAKE_SIZE         "${_TC_PREFIX}size"    CACHE STRING "size")

# Skip compiler sanity-check link step - bare-metal has no OS entry point.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cortex-R5F CPU flags (AM64x / AM243x)
set(R5_CPU_FLAGS "-mcpu=cortex-r5 -mfpu=vfpv3-d16 -mfloat-abi=hard -mlittle-endian -g -fno-omit-frame-pointer -mapcs-frame")

set(CMAKE_C_FLAGS_INIT   "${R5_CPU_FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS_INIT "${R5_CPU_FLAGS}" CACHE STRING "")

# Do not link against host libraries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
