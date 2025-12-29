cmake_minimum_required(VERSION 4.0)

cmake_policy(SET CMP0135 NEW)

project(optograin
    DESCRIPTION "OptoGrain enlarger side firmware"
    LANGUAGES C ASM
    VERSION "0.0.1"
)

set(TARGET_FILE "optograin")
add_executable(${TARGET_FILE}.elf)

if (NOT BUILD_TESTS)
    message("\n----------------------------------------------------")
    message("        Building OptoGrain firmware version ${PROJECT_VERSION}.")
    message("             Compiler version: ${CMAKE_C_COMPILER_VERSION}")
    message("----------------------------------------------------\n")
endif()

set(FW_ELF_FILE   "${TARGET_FILE}.elf")
set(FW_HEX_FILE   "${TARGET_FILE}.hex")
set(FW_MAP_FILE   "${TARGET_FILE}.map")

set(CMAKE_C_FLAGS_DEBUG     "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_CXX_FLAGS_DEBUG   "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_C_FLAGS_RELEASE   "-O2 -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fstack-protector -s")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fstack-protector -s")

# cascading definitions
add_compile_definitions(
    _GNU_SOURCE
    STM32F103xx
    USE_FULL_LL_DRIVER
    FW_VERSION=\"${CMAKE_PROJECT_VERSION}\"
    $<$<CONFIG:Debug>:DEBUG>
)

add_subdirectory(optograin)
add_subdirectory(sys)

target_link_libraries(${FW_ELF_FILE} PRIVATE
    stm32f1xx_ll
    stm32f1xx_cmsis
    freertos_kernel
    freertos_config
    bsp
    #tim_drv
)

target_link_options(${FW_ELF_FILE} PRIVATE
    -Wl,-Map=${FW_MAP_FILE},--cref
)


add_custom_command(TARGET ${FW_ELF_FILE} POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/size_mcu.sh ${CMAKE_SIZE_UTIL} ${FW_ELF_FILE} --flash 65535 --ram 20480
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${FW_ELF_FILE} ${FW_HEX_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)