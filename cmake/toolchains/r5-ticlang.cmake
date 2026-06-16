# Cortex-R5 bare-metal toolchain - TI ARM Clang (tiarmclang)
#
# Targets: AM64x / AM243x Cortex-R5F (hard-float, little-endian)
#
# Usage:
#   cmake -S . -B build/r5-ticlang -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/r5-ticlang.cmake
#
# tiarmclang bin directory must be on PATH before invoking cmake.
# Install: download ti_cgt_armllvm_<version>_linux-x64_installer.bin from
#   https://www.ti.com/tool/download/ARM-CGT-CLANG
# and run:  ./ti_cgt_armllvm_<version>_linux-x64_installer.bin --mode unattended --prefix $HOME/tiarmclang
# then:     export PATH=$HOME/tiarmclang/bin:$PATH

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_ARMClang_CMP0123 NEW)

# Tools resolved from PATH - add tiarmclang bin to PATH before invoking cmake.
set(CMAKE_C_COMPILER   tiarmclang   CACHE STRING "C compiler")
set(CMAKE_CXX_COMPILER tiarmclang++ CACHE STRING "C++ compiler")
set(CMAKE_ASM_COMPILER tiarmclang   CACHE STRING "ASM compiler")
set(CMAKE_AR           tiarmar      CACHE STRING "Archiver")

# Skip linker sanity-check - bare-metal has no OS entry point.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# tiarmclang requires --target=arm-ti-none-eabi (TI vendor triple).
# Using arm-none-eabi triggers "unsupported target triple for compiler vendor 'ti'".
# -mcpu, -mfpu, -mfloat-abi match the AM64x Cortex-R5F configuration.
set(R5_CPU_FLAGS "--target=arm-ti-none-eabi -mcpu=cortex-r5 -mfpu=vfpv3-d16 -mfloat-abi=hard -mlittle-endian")

set(CMAKE_C_FLAGS_INIT   "${R5_CPU_FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS_INIT "${R5_CPU_FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS "${R5_CPU_FLAGS}" CACHE STRING "ASM flags" FORCE)

# CMake doesn't set a default for CMAKE_ASM_FLAGS_DEBUG (unlike C/CXX).
# Set it explicitly so Debug/RelWithDebInfo builds emit DWARF for .S files.
set(CMAKE_ASM_FLAGS_DEBUG          "-g"       CACHE STRING "ASM debug flags")
set(CMAKE_ASM_FLAGS_RELWITHDEBINFO "-g -O2"   CACHE STRING "ASM RelWithDebInfo flags")
set(CMAKE_ASM_FLAGS_RELEASE        "-O3"      CACHE STRING "ASM release flags")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
