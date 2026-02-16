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

#include "switch.h"
#include "uart.h"

#include "SEGGER_RTT.h"

#include <stm32f1xx.h>
#include <stm32f1xx_ll_rcc.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_system.h>
#include <stm32f1xx_ll_utils.h>
#include <stm32f1xx_ll_exti.h>
#include <stm32f1xx_ll_crc.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SYS_CLK_FREQ_HZ (64000000UL)  // 64 MHz from HSE+PLL

//=====================================================================================================================
// Hardware Configuration
//=====================================================================================================================

const SEncoderConfig_t g_encoderHeadConfig = {
    .pTimer = TIM2,
    .aPin = {{ LL_GPIO_PIN_0, GPIOA }, false },
    .bPin = {{ LL_GPIO_PIN_1, GPIOA }, false },
    .zPin = {{ LL_GPIO_PIN_2, GPIOA }, LL_EXTI_LINE_2 },
};

const SEncoderConfig_t g_encoderColumnConfig = {
    .pTimer = TIM3,
    .aPin = {{ LL_GPIO_PIN_7, GPIOA }, false },
    .bPin = {{ LL_GPIO_PIN_6, GPIOA }, false },
    .zPin = {{ LL_GPIO_PIN_5, GPIOA }, LL_EXTI_LINE_5 },
};

const SMotorConfig_t g_motorColumnConfig = {
    .pTimer = TIM1,
    .pwmFrequency = 25000,
    .in1Pin = {{ LL_GPIO_PIN_8, GPIOA }, false },
    .in2Pin = {{ LL_GPIO_PIN_9, GPIOA }, false },
    .in1Channel = LL_TIM_CHANNEL_CH1,
    .in2Channel = LL_TIM_CHANNEL_CH2,
    .useRemapPins = false,
    .invertDirection = false,
    .minDutyPercent = 30, // TBD
};

const SMotorConfig_t g_motorHeadConfig = {
    .pTimer = TIM1,
    .pwmFrequency = 25000,
    .in1Pin = {{ LL_GPIO_PIN_10, GPIOA }, false },
    .in2Pin = {{ LL_GPIO_PIN_11, GPIOA }, false },
    .in1Channel = LL_TIM_CHANNEL_CH3,
    .in2Channel = LL_TIM_CHANNEL_CH4,
    .useRemapPins = false,
    .invertDirection = false,
    .minDutyPercent = 17
};

const SLampConfig_t g_lampConfig = {
    .pTimer = TIM4,
    .controlPin = {{ LL_GPIO_PIN_12, GPIOB }, true },
    .fanConfig = {
        .pwmPin = {{ LL_GPIO_PIN_14, GPIOB }, true },
        .tachPin = {{ LL_GPIO_PIN_13, GPIOB }, false },
    }
};

const SFanConfig_t g_fanConfig = {
    .pwmPin = {{ LL_GPIO_PIN_13, GPIOB }, true },
    .tachPin = {{ LL_GPIO_PIN_14, GPIOB }, false },
};

const SUartConfig_t g_uart1Config = {
    .pUsart = USART1,
    .txPin = { { LL_GPIO_PIN_6, GPIOB }, false },
    .rxPin = { { LL_GPIO_PIN_7, GPIOB }, false },
    .remapAlternateFunction = true, // use USART1 AF pins PB6/PB7
    .baudRate = 460800,
    .dataBits = 8,
    .stopBits = 1
};

// plain old GPIOs
SGenericGPIOPin_t g_turretSwitchA = { { LL_GPIO_PIN_1, GPIOB }, false }; 
SGenericGPIOPin_t g_turretSwitchB = { { LL_GPIO_PIN_15, GPIOB }, false };

SEXTIGPIOPin_t g_headLimitTop = { { LL_GPIO_PIN_0, GPIOB }, LL_EXTI_LINE_0 };
SGenericGPIOPin_t g_headLimitBottom = { { LL_GPIO_PIN_2, GPIOB }, false };
SEXTIGPIOPin_t g_columnLimitTop = { { LL_GPIO_PIN_12, GPIOA }, LL_EXTI_LINE_12 };
SGenericGPIOPin_t g_columnLimitBottom = { { LL_GPIO_PIN_11, GPIOB }, false };

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

    SEGGER_RTT_Init();

    bspUartInit(&g_uart1Config);

    bspEncoderInit(ENCODER_HEAD, &g_encoderHeadConfig);
    bspEncoderInit(ENCODER_COLUMN, &g_encoderColumnConfig);
    bspMotorInit(MOTOR_COLUMN, &g_motorColumnConfig);
    bspMotorInit(MOTOR_HEAD, &g_motorHeadConfig);
    bspLampInit(&g_lampConfig);

    bspSwitchInit(SWITCH_LENS_INDEX_A, &g_turretSwitchA, nullptr);
    bspSwitchInit(SWITCH_LENS_INDEX_B, &g_turretSwitchB, nullptr);

    bspSwitchInit(SWITCH_HEAD_TOP_LIMIT, nullptr, &g_headLimitTop);
    bspSwitchInit(SWITCH_HEAD_BOTTOM_LIMIT, &g_headLimitBottom, nullptr);
    bspSwitchInit(SWITCH_COLUMN_TOP_LIMIT, nullptr, &g_columnLimitTop);
    bspSwitchInit(SWITCH_COLUMN_BOTTOM_LIMIT, &g_columnLimitBottom, nullptr);

    bspMotorEnable(true);

    SEGGER_RTT_WriteString(0, "BSP init complete");
}

void hwDelayMs(uint32_t ms)
{
    LL_mDelay(ms);
}

uint32_t getCurrentSystick(void)
{
    return SysTick->VAL;
}


void getCRC32(const uint8_t *pData)
{

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