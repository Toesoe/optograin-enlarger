add_library(optograin_bsp
    src/board.c
    src/uart.c

    src/motor.c
    src/encoder.c
)

target_include_directories(optograin_bsp PUBLIC
    inc
)

target_link_libraries(optograin_bsp PUBLIC
    stm32f1xx_ll
)