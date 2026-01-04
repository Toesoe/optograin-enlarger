/**
 * @file encoder.c
 * @brief Quadrature encoder implementation
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "encoder.h"

#include <stddef.h>
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_exti.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>

//=====================================================================================================================
// Private types
//=====================================================================================================================

typedef struct
{
    const SEncoderConfig_t *pConfig;
    volatile int32_t        overflowCount; // extends 16-bit timer to 32-bit
    volatile bool           hasZIndexRef;
    volatile bool           autoResetOnZ; // Auto-reset counter on Z pulse (for homing)
    int32_t                 lastCount;
    uint32_t                lastTimestamp;
} SEncoderState_t;

//=====================================================================================================================
// Private data
//=====================================================================================================================

static SEncoderState_t gs_encoderStates[ENCODER_COUNT] = { 0 };

//=====================================================================================================================
// Public functions
//=====================================================================================================================

void bspEncoderInit(EEncoderId_t encoderId, const SEncoderConfig_t *pEncoderConfig)
{
    if (!pEncoderConfig || (encoderId >= ENCODER_COUNT)) return;

    gs_encoderStates[encoderId].pConfig      = pEncoderConfig;
    gs_encoderStates[encoderId].autoResetOnZ = true; // Enable auto-reset by default

    if (gs_encoderStates[encoderId].pConfig->pTimer == TIM2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
    }
    else if (gs_encoderStates[encoderId].pConfig->pTimer == TIM3)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    }

    LL_GPIO_InitTypeDef gpioInit = { 0 };
    gpioInit.Mode                = LL_GPIO_MODE_FLOATING; // PU external
    gpioInit.Speed               = LL_GPIO_SPEED_FREQ_HIGH;

    gpioInit.Pin                 = pEncoderConfig->aPin.pinPort.pin;
    LL_GPIO_Init(pEncoderConfig->aPin.pinPort.port, &gpioInit);

    gpioInit.Pin = pEncoderConfig->bPin.pinPort.pin;
    LL_GPIO_Init(pEncoderConfig->bPin.pinPort.port, &gpioInit);

    LL_TIM_DisableCounter(pEncoderConfig->pTimer);

    // Configure timer base
    LL_TIM_SetPrescaler(pEncoderConfig->pTimer, 0);       // No prescaler for encoder mode
    LL_TIM_SetAutoReload(pEncoderConfig->pTimer, 0xFFFF); // 16-bit full range
    LL_TIM_SetClockDivision(pEncoderConfig->pTimer, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetCounterMode(pEncoderConfig->pTimer, LL_TIM_COUNTERMODE_UP);

    // Configure timer in encoder mode
    LL_TIM_SetEncoderMode(pEncoderConfig->pTimer, LL_TIM_ENCODERMODE_X4_TI12);

    // Configure input channels
    LL_TIM_IC_SetActiveInput(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetActiveInput(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPolarity(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetPolarity(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);

    LL_TIM_IC_SetFilter(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1_N2);
    LL_TIM_IC_SetFilter(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1_N2);

    LL_TIM_CC_EnableChannel(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH1);
    LL_TIM_CC_EnableChannel(pEncoderConfig->pTimer, LL_TIM_CHANNEL_CH2);

    // Set counter to mid-range to detect both directions
    LL_TIM_SetCounter(pEncoderConfig->pTimer, 0x8000);

    LL_TIM_SetUpdateSource(pEncoderConfig->pTimer, LL_TIM_UPDATESOURCE_COUNTER);
    LL_TIM_ClearFlag_UPDATE(pEncoderConfig->pTimer);

    // Enable update interrupt for overflow tracking
    LL_TIM_EnableIT_UPDATE(pEncoderConfig->pTimer);

    if (pEncoderConfig->pTimer == TIM2)
    {
        NVIC_SetPriority(TIM2_IRQn, 2);
        NVIC_EnableIRQ(TIM2_IRQn);
    }
    else if (pEncoderConfig->pTimer == TIM3)
    {
        NVIC_SetPriority(TIM3_IRQn, 2);
        NVIC_EnableIRQ(TIM3_IRQn);
    }

    gpioInit.Pin   = pEncoderConfig->zPin.pinPort.pin;
    gpioInit.Mode  = LL_GPIO_MODE_FLOATING;
    gpioInit.Speed = LL_GPIO_SPEED_FREQ_LOW;
    LL_GPIO_Init(pEncoderConfig->zPin.pinPort.port, &gpioInit);

    uint32_t extiSrc = (pEncoderConfig->zPin.pinPort.port == GPIOA) ? LL_GPIO_AF_EXTI_PORTA : LL_GPIO_AF_EXTI_PORTB;

    if (pEncoderConfig->zPin.extiLine == LL_EXTI_LINE_2)
    {
        LL_GPIO_AF_SetEXTISource(extiSrc, LL_GPIO_AF_EXTI_LINE2);
    }
    else if (pEncoderConfig->zPin.extiLine == LL_EXTI_LINE_5)
    {
        LL_GPIO_AF_SetEXTISource(extiSrc, LL_GPIO_AF_EXTI_LINE5);
    }
    else
    {
        // add pin here
        __asm("bkpt #0");
        while (true);
    }

    // enable exti on falling edge. NPN sensor assumed with PU
    LL_EXTI_EnableIT_0_31(pEncoderConfig->zPin.extiLine);
    LL_EXTI_EnableFallingTrig_0_31(pEncoderConfig->zPin.extiLine);
    LL_EXTI_DisableRisingTrig_0_31(pEncoderConfig->zPin.extiLine); // prevent duplicate triggers

    if (pEncoderConfig->zPin.extiLine == LL_EXTI_LINE_2)
    {
        NVIC_SetPriority(EXTI2_IRQn, 1);
        NVIC_EnableIRQ(EXTI2_IRQn);
    }
    else if (pEncoderConfig->zPin.extiLine == LL_EXTI_LINE_5)
    {
        NVIC_SetPriority(EXTI9_5_IRQn, 1);
        NVIC_EnableIRQ(EXTI9_5_IRQn);
    }

    LL_TIM_GenerateEvent_UPDATE(pEncoderConfig->pTimer);
    LL_TIM_ClearFlag_UPDATE(pEncoderConfig->pTimer);
    LL_TIM_EnableCounter(pEncoderConfig->pTimer);
}

int32_t bspEncoderGetCount(EEncoderId_t encoder)
{
    if ((encoder >= ENCODER_COUNT) || !gs_encoderStates[encoder].pConfig)
    {
        return 0;
    }

    // combine overflow with current timer count, subtract mid-point offset
    return ((gs_encoderStates[encoder].overflowCount << 16) |
           (uint16_t)LL_TIM_GetCounter(gs_encoderStates[encoder].pConfig->pTimer)) - 0x8000;
}

void bspEncoderReset(EEncoderId_t encoder)
{
    if ((encoder >= ENCODER_COUNT) || !gs_encoderStates[encoder].pConfig->pTimer) return;

    gs_encoderStates[encoder].overflowCount = 0;
    gs_encoderStates[encoder].lastCount     = 0;
    gs_encoderStates[encoder].hasZIndexRef  = false;
    LL_TIM_SetCounter(gs_encoderStates[encoder].pConfig->pTimer, 0x8000);
}

int32_t bspEncoderGetVelocity(EEncoderId_t encoder)
{
    if ((encoder >= ENCODER_COUNT) || !gs_encoderStates[encoder].pConfig->pTimer) return 0;

    int32_t  currentCount                   = bspEncoderGetCount(encoder);
    uint32_t currentTime                    = getCurrentSystick();

    int32_t  deltaCounts                    = currentCount - gs_encoderStates[encoder].lastCount;
    int32_t  deltaTime                      = (int32_t)(currentTime - gs_encoderStates[encoder].lastTimestamp);

    gs_encoderStates[encoder].lastCount     = currentCount;
    gs_encoderStates[encoder].lastTimestamp = currentTime;

    // Return counts per millisecond
    return (deltaTime == 0) ? 0 : (int16_t)(deltaCounts / deltaTime);
}

bool bspEncoderHasZIndexReference(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT) return false;

    return gs_encoderStates[encoder].hasZIndexRef;
}

void bspEncoderClearZIndexReference(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT) return;

    gs_encoderStates[encoder].hasZIndexRef = false;
}

void bspEncoderSetAutoResetOnZ(EEncoderId_t encoder, bool enable)
{
    if (encoder >= ENCODER_COUNT) return;

    gs_encoderStates[encoder].autoResetOnZ = enable;
}

//=====================================================================================================================
// IRQ Handlers (prototypes in startup file)
//=====================================================================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

void __attribute__((used)) TIM2_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
    {
        LL_TIM_ClearFlag_UPDATE(TIM2);

        // Track overflow/underflow
        if (LL_TIM_GetDirection(TIM2) == LL_TIM_COUNTERDIRECTION_UP)
        {
            gs_encoderStates[ENCODER_HEAD].overflowCount++;
        }
        else
        {
            gs_encoderStates[ENCODER_HEAD].overflowCount--;
        }
    }
}

void __attribute__((used)) TIM3_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM3))
    {
        LL_TIM_ClearFlag_UPDATE(TIM3);

        // Track overflow/underflow
        if (LL_TIM_GetDirection(TIM3) == LL_TIM_COUNTERDIRECTION_UP)
        {
            gs_encoderStates[ENCODER_COLUMN].overflowCount++;
        }
        else
        {
            gs_encoderStates[ENCODER_COLUMN].overflowCount--;
        }
    }
}

void __attribute__((used)) EXTI2_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_2))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_2);

        // Set homed flag
        gs_encoderStates[ENCODER_HEAD].hasZIndexRef = true;

        // Only reset counter if auto-reset is enabled (for homing)
        if (gs_encoderStates[ENCODER_HEAD].autoResetOnZ)
        {
            gs_encoderStates[ENCODER_HEAD].overflowCount = 0;
            LL_TIM_SetCounter(TIM2, 0x8000);
        }
    }
}

void __attribute__((used)) EXTI9_5_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);

        // Z-index pulse detected on column encoder (PA5)
        // Set homed flag
        gs_encoderStates[ENCODER_COLUMN].hasZIndexRef = true;

        // Only reset counter if auto-reset is enabled (for homing)
        if (gs_encoderStates[ENCODER_COLUMN].autoResetOnZ)
        {
            gs_encoderStates[ENCODER_COLUMN].overflowCount = 0;
            LL_TIM_SetCounter(TIM3, 0x8000);
        }
    }
}

#pragma GCC diagnostic pop