add_library(segger_rtt STATIC
    SEGGER_RTT.c
    SEGGER_RTT_printf.c
    SEGGER_RTT_ASM_ARMv7M.S
)

target_include_directories(segger_rtt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# RTT uses memcpy, needs standard library
target_link_libraries(segger_rtt PUBLIC
    c
    nosys
)

# Optimize for size
target_compile_options(segger_rtt PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)
