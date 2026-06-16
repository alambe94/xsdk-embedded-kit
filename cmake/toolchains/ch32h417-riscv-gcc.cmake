# WCH CH32H417 bare-metal toolchain - riscv-none-elf-gcc
#
# Targets: CH32H417 V5F (RISC-V QingKeV5F, rv32imacb)
#
# Usage:
#   cmake -S . -B build/ch32h417-riscv-gcc \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ch32h417-riscv-gcc.cmake
#
# Windows: xsdk.bat setup riscv-gcc  (installs xPack RISC-V GCC to tools\riscv_gcc\)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)

get_filename_component(_XSDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_TC_PREFIX "riscv-none-elf-")
set(_TC_SUFFIX "")
set(_TC_BIN_DIR "${_XSDK_ROOT}/tools/riscv_gcc/bin")

if(EXISTS "${_TC_BIN_DIR}/${_TC_PREFIX}gcc.exe")
    set(_TC_PREFIX "${_TC_BIN_DIR}/${_TC_PREFIX}")
    set(_TC_SUFFIX ".exe")
endif()

set(CMAKE_C_COMPILER   "${_TC_PREFIX}gcc${_TC_SUFFIX}"     CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_TC_PREFIX}g++${_TC_SUFFIX}"     CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_ASM_COMPILER "${_TC_PREFIX}gcc${_TC_SUFFIX}"     CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_AR           "${_TC_PREFIX}ar${_TC_SUFFIX}"      CACHE FILEPATH "Archiver" FORCE)
set(CMAKE_RANLIB       "${_TC_PREFIX}ranlib${_TC_SUFFIX}"  CACHE FILEPATH "Ranlib" FORCE)
set(CMAKE_OBJCOPY      "${_TC_PREFIX}objcopy${_TC_SUFFIX}" CACHE FILEPATH "objcopy" FORCE)
set(CMAKE_SIZE         "${_TC_PREFIX}size${_TC_SUFFIX}"    CACHE FILEPATH "size" FORCE)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(xSDK_SOC_CH32H417 TRUE CACHE BOOL "Build for WCH CH32H417 SoC" FORCE)

set(RV_CPU_FLAGS "-march=rv32imacb_zicsr_zifencei -mabi=ilp32 -mcmodel=medlow -msmall-data-limit=8 -g")

set(CMAKE_C_FLAGS_INIT   "${RV_CPU_FLAGS}" CACHE STRING "")
set(CMAKE_ASM_FLAGS_INIT "${RV_CPU_FLAGS}" CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
