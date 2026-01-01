/**
 * @file board.c
 *
 * @brief board-specific functionality for nbtgTimer
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "board.h"
#include "encoder.h"
#include "lamp.h"
#include "motor.h"

#include <stm32f1xx.h>
#include <stm32f1xx_ll_rcc.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_system.h>
#include <stm32f1xx_ll_utils.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SYS_CLK_FREQ_HZ (64000000UL)  // 64 MHz from HSE+PLL

//=====================================================================================================================
// Hardware Configuration
//=====================================================================================================================

const SEncoderConfig_t g_encoderHeadConfig = {
    .encoderId = ENCODER_HEAD,
    .pTimer = TIM2,  // PA1 (TIM2_CH2/B), PA2 (TIM2_CH3/A),
    .aPin = {{ LL_GPIO_PIN_2, GPIOA }, false },
    .bPin = {{ LL_GPIO_PIN_1, GPIOA }, false },
    .zPin = {{ LL_GPIO_PIN_0, GPIOA }, false },
};

const SEncoderConfig_t g_encoderColumnConfig = {
    .encoderId = ENCODER_COLUMN,
    .pTimer = TIM3,  // PA6 (TIM3_CH1/A), PA7 (TIM3_CH2/B)
    .aPin = {{ LL_GPIO_PIN_7, GPIOA }, false },
    .bPin = {{ LL_GPIO_PIN_6, GPIOA }, false },
    .zPin = {{ LL_GPIO_PIN_5, GPIOA }, false },
};

const SMotorConfig_t g_motorColumnConfig = {
    .motorId = MOTOR_COLUMN,
    .pTimer = TIM1,
    .pwmFrequency = 25000,        // 25 kHz
    .in1Pin = {{ LL_GPIO_PIN_8, GPIOA }, false },
    .in2Pin = {{ LL_GPIO_PIN_9, GPIOA }, false },
    .useRemapPins = false,
    .invertDirection = false,
};

const SMotorConfig_t g_motorHeadConfig = {
    .motorId = MOTOR_HEAD,
    .pTimer = TIM1,
    .pwmFrequency = 25000,        // 25 kHz
    .in1Pin = {{ LL_GPIO_PIN_10, GPIOA }, false },
    .in2Pin = {{ LL_GPIO_PIN_11, GPIOA }, false },
    .useRemapPins = false,
    .invertDirection = false,
};

const SLampConfig_t g_lampConfig = {
    .pTimer = TIM4,
    .controlPin = {{ LL_GPIO_PIN_12, GPIOB }, true }, // PB12
};

// plain old GPIOs
SGenericGPIOPin_t g_fanTachPin = { { LL_GPIO_PIN_14, GPIOB}, false };
SGenericGPIOPin_t g_fanPwmPin = { { LL_GPIO_PIN_13, GPIOB }, true };

SGenericGPIOPin_t g_turretSwitchA = { { LL_GPIO_PIN_8, GPIOB }, false }; // note: rewired, find correct pin
SGenericGPIOPin_t g_turretSwitchB = { { LL_GPIO_PIN_9, GPIOB }, false };

SGenericGPIOPin_t g_headLimitTop = { { LL_GPIO_PIN_15, GPIOA }, false }; // note: rewired, find correct pin
SGenericGPIOPin_t g_headLimitBottom = { { LL_GPIO_PIN_15, GPIOA }, false }; // note: rewired, find correct pin
SGenericGPIOPin_t g_columnLimitTop = { { LL_GPIO_PIN_15, GPIOA }, false };
SGenericGPIOPin_t g_columnLimitBottom = { { LL_GPIO_PIN_15, GPIOA }, false }; // note: rewired, find correct pin

//=====================================================================================================================
// Protos
//=====================================================================================================================

static void initSysclock(void);

//=====================================================================================================================
// Functions
//=====================================================================================================================


void initBoard(void)
{
#ifdef DEBUG
    // Halt timers during debug
    DBGMCU->CR |= DBGMCU_CR_DBG_TIM1_STOP | DBGMCU_CR_DBG_TIM2_STOP | 
                  DBGMCU_CR_DBG_TIM3_STOP | DBGMCU_CR_DBG_TIM4_STOP;
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP | DBGMCU_CR_DBG_WWDG_STOP;
#endif

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    initSysclock();

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOB);

    bspEncoderInit(&g_encoderHeadConfig);
    bspEncoderInit(&g_encoderColumnConfig);
    bspMotorInit(&g_motorColumnConfig);
    bspMotorInit(&g_motorHeadConfig);

    while (true);
    // bspLampInit(&g_lampConfig);
}

void hwDelayMs(uint32_t ms)
{
    LL_mDelay(ms);
}

//=====================================================================================================================
// Statics
//=====================================================================================================================

/**
 * @brief Configure system clock to 64 MHz
 * 
 * Tries HSE first (8 MHz crystal), falls back to HSI if unavailable
 * 
 * Clock tree:
 * - HSE: 8 MHz crystal (X50328MSB2GI, 20pF load) OR HSI: 8 MHz internal RC
 * - PLL: Source * multiplier = 64 MHz
 * - SYSCLK: 64 MHz
 * - AHB: 64 MHz (no prescaler)
 * - APB1: 32 MHz (DIV2, max is 36 MHz)
 * - APB2: 64 MHz (no prescaler)
 */
static void initSysclock(void)
{
    // Configure flash latency for 64 MHz (2 wait states)
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);

    // Try HSE first (8 MHz external crystal)
    LL_RCC_HSE_Enable();
    
    // Wait with timeout
    uint32_t timeout = 100000;
    while (LL_RCC_HSE_IsReady() != 1 && --timeout > 0);
    
    if (timeout > 0)
    {
        // HSE ready - use crystal
        LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_8);
    }
    else
    {
        // HSE failed - fall back to HSI
        LL_RCC_HSE_Disable();
        LL_RCC_HSI_Enable();
        while (LL_RCC_HSI_IsReady() != 1);
        LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_16);
    }
    
    // Configure and enable PLL
    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1);

    // Set prescalers before switching SYSCLK
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);    // AHB = 64 MHz
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);     // APB1 = 32 MHz (max 36 MHz)
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);     // APB2 = 64 MHz

    // Switch SYSCLK to PLL
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    // Update SystemCoreClock variable and configure SysTick
    LL_SetSystemCoreClock(SYS_CLK_FREQ_HZ);
    LL_Init1msTick(SYS_CLK_FREQ_HZ);
}