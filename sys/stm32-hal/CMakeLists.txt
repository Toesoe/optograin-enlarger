add_library(stm32f1xx_ll
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_adc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_crc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_dac.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_dma.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_exti.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_fsmc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_gpio.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_i2c.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_pwr.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_rcc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_rtc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_sdmmc.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_spi.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_tim.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usart.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_utils.c

    STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s
    STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
)

add_library(stm32f1xx_cmsis INTERFACE)

target_include_directories(stm32f1xx_ll PUBLIC
    STM32CubeF1/Drivers/STM32F1xx_HAL_Driver/Inc
)

target_include_directories(stm32f1xx_cmsis INTERFACE
    STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    STM32CubeF1/Drivers/CMSIS/Include
)

# don't enforce warnings on external libraries
target_compile_options(stm32f1xx_ll PRIVATE
    -w  # -w disables all gcc warnings
)

target_link_libraries(stm32f1xx_ll PUBLIC
    stm32f1xx_sys # stm32f1xx.h
)

# Specify the linker script
target_link_options(stm32f1xx_ll INTERFACE
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/linker/STM32F103XB_FLASH.ld
)

