cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

set(TARGET_CPU "cortex-m3")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ${TARGET_CPU})
set(CMAKE_GENERATOR Ninja)

if (NOT DEFINED ENV{ARM_TC_PATH})
    if (CMAKE_HOST_UNIX)
        set(TOOLCHAIN_PATH /usr/bin)
    endif()

    if (CMAKE_HOST_WIN32)
        set(TOOLCHAIN_PATH C:/msys64/mingw64/bin)
    endif()

    message(WARNING "ARM_TC_PATH environment variable is not set. Using default path ${TOOLCHAIN_PATH}")
else()
    message(STATUS "Using toolchain located at $ENV{ARM_TC_PATH}.")
    file(TO_CMAKE_PATH $ENV{ARM_TC_PATH} TOOLCHAIN_PATH)
endif()

if (CMAKE_HOST_WIN32)
    set(TOOL_SUFFIX .exe)
    set(CMAKE_COLOR_MAKEFILE OFF)
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # we need gnu ext for asm calls
#set(CMAKE_VERBOSE_MAKEFILE ON) # enable this if you want to debug make issues

set(CMAKE_C_COMPILER_WORKS TRUE) # prevent errors on initial test compile

set(CMAKE_C_COMPILER            ${TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOL_SUFFIX})
set(CMAKE_C_COMPILER_LINKER     ${CMAKE_C_COMPILER})
set(CMAKE_AR                    ${TOOLCHAIN_PATH}/arm-none-eabi-ar${TOOL_SUFFIX})
set(CMAKE_RANLIB                ${TOOLCHAIN_PATH}/arm-none-eabi-ranlib${TOOL_SUFFIX})
set(CMAKE_ASM_COMPILER          ${TOOLCHAIN_PATH}/arm-none-eabi-gcc${TOOL_SUFFIX})
set(CMAKE_SIZE_UTIL             ${TOOLCHAIN_PATH}/arm-none-eabi-size${TOOL_SUFFIX})
set(CMAKE_OBJCOPY               ${TOOLCHAIN_PATH}/arm-none-eabi-objcopy${TOOL_SUFFIX})

set(COMMON_FLAGS      "-ffreestanding -mcpu=${TARGET_CPU} -mthumb -fmax-errors=5 -msoft-float -mfloat-abi=soft")
set(WARN_FLAGS        "-Wall -Wextra -Wpointer-arith -Wformat -Wno-unused-local-typedefs -Wno-unused-parameter -Wfloat-equal \
                       -Wshadow -Wwrite-strings -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wconversion")
set(C_FLAGS           "-ffunction-sections -fdata-sections -fno-common")

set(CMAKE_C_FLAGS_INIT          "${COMMON_FLAGS} ${C_FLAGS} ${WARN_FLAGS} -MMD -MP" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_INIT        "${COMMON_FLAGS} -x assembler-with-cpp"        CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -Wl,--gc-sections -Wl,--print-memory-usage -Wl,--defsym=_init=0 -Wl,--defsym=_fini=0 --specs=nano.specs --specs=nosys.specs"    CACHE STRING "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_EXPORT_COMPILE_COMMANDS             ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES    ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES   ON)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS     ON)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE           ON)

option(BUILD_DOC   "Build documentation" OFF)
option(BUILD_TESTS "Build test programs" OFF)

set(DISABLE_LIB_TESTS ON)