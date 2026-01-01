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
// Private Data
//=====================================================================================================================

static TIM_TypeDef *s_encoderTimers[ENCODER_COUNT]   = { NULL };
static int32_t      s_encoderOverflow[ENCODER_COUNT] = { 0 };
static int32_t      s_lastCount[ENCODER_COUNT]       = { 0 };
static uint32_t     s_lastTimestamp[ENCODER_COUNT]   = { 0 };
static bool         s_homed[ENCODER_COUNT]           = { false };

//=====================================================================================================================
// Functions
//=====================================================================================================================

void bspEncoderInit(const SEncoderConfig_t *pEncoderConfig)
{
    if (!pEncoderConfig || pEncoderConfig->encoderId >= ENCODER_COUNT) return;

    TIM_TypeDef *pTimer                        = pEncoderConfig->pTimer;
    s_encoderTimers[pEncoderConfig->encoderId] = pTimer;

    // Enable timer clock
    if (pTimer == TIM2)
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
    else if (pTimer == TIM3)
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

    // Configure GPIO pins as alternate function inputs
    LL_GPIO_InitTypeDef gpioInit = { 0 };
    gpioInit.Mode                = LL_GPIO_MODE_FLOATING;
    gpioInit.Speed               = LL_GPIO_SPEED_FREQ_HIGH;

    // A channel
    gpioInit.Pin = pEncoderConfig->aPin.pinPort.pin;
    LL_GPIO_Init(pEncoderConfig->aPin.pinPort.port, &gpioInit);

    // B channel
    gpioInit.Pin = pEncoderConfig->bPin.pinPort.pin;
    LL_GPIO_Init(pEncoderConfig->bPin.pinPort.port, &gpioInit);

    // Configure timer in encoder mode
    LL_TIM_SetEncoderMode(pTimer, LL_TIM_ENCODERMODE_X4_TI12);

    // Configure input channels
    LL_TIM_IC_SetActiveInput(pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetActiveInput(pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI);

    LL_TIM_IC_SetPolarity(pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetPolarity(pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);

    LL_TIM_IC_SetFilter(pTimer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1_N2);
    LL_TIM_IC_SetFilter(pTimer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1_N2);

    // Set counter to mid-range to detect both directions
    LL_TIM_SetAutoReload(pTimer, 0xFFFF);
    LL_TIM_SetCounter(pTimer, 0x8000);

    // Enable update interrupt for overflow tracking
    LL_TIM_EnableIT_UPDATE(pTimer);

    if (pTimer == TIM2)
        NVIC_EnableIRQ(TIM2_IRQn);
    else if (pTimer == TIM3)
        NVIC_EnableIRQ(TIM3_IRQn);

    // Configure Z-index pin for homing (if present)
    if (pEncoderConfig->zPin.pinPort.pin != 0)
    {
        LL_GPIO_InitTypeDef zInit = { 0 };
        zInit.Pin                 = pEncoderConfig->zPin.pinPort.pin;
        zInit.Mode                = LL_GPIO_MODE_INPUT;
        LL_GPIO_Init(pEncoderConfig->zPin.pinPort.port, &zInit);

        // Enable EXTI for rising edge (Z pulse)
        if (pEncoderConfig->zPin.pinPort.pin == LL_GPIO_PIN_0)
        {
            LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_0);
            LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_0);
            NVIC_EnableIRQ(EXTI0_IRQn);
        }
        else if (pEncoderConfig->zPin.pinPort.pin == LL_GPIO_PIN_5)
        {
            LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_5);
            LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_5);
            NVIC_EnableIRQ(EXTI9_5_IRQn);
        }
    }

    // Start encoder
    LL_TIM_EnableCounter(pTimer);
}

int32_t bspEncoderGetCount(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !s_encoderTimers[encoder]) return 0;

    TIM_TypeDef *pTimer = s_encoderTimers[encoder];
    uint32_t     cnt    = LL_TIM_GetCounter(pTimer);

    // Combine overflow counter with hardware counter
    int32_t fullCount = (s_encoderOverflow[encoder] << 16) | (cnt & 0xFFFF);

    // Subtract initial offset (0x8000) to make zero-centered
    return fullCount - 0x8000;
}

void bspEncoderReset(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !s_encoderTimers[encoder]) return;

    s_encoderOverflow[encoder] = 0;
    s_lastCount[encoder]       = 0;
    s_homed[encoder]           = false;
    LL_TIM_SetCounter(s_encoderTimers[encoder], 0x8000);
}

int16_t bspEncoderGetVelocity(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !s_encoderTimers[encoder]) return 0;

    int32_t  currentCount    = bspEncoderGetCount(encoder);
    uint32_t currentTime     = LL_TIM_GetCounter(TIM4); // Assuming TIM4 is 1ms tick

    int32_t  deltaCounts     = currentCount - s_lastCount[encoder];
    int32_t  deltaTime       = (int32_t)(currentTime - s_lastTimestamp[encoder]);

    s_lastCount[encoder]     = currentCount;
    s_lastTimestamp[encoder] = currentTime;

    if (deltaTime == 0) return 0;

    // Return counts per millisecond
    return (int16_t)(deltaCounts / deltaTime);
}

//=====================================================================================================================
// IRQ Handlers (prototypes in startup file)
//=====================================================================================================================

void __attribute__((used)) TIM2_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
    {
        LL_TIM_ClearFlag_UPDATE(TIM2);

        // Track overflow/underflow
        if (LL_TIM_GetDirection(TIM2) == LL_TIM_COUNTERDIRECTION_UP)
            s_encoderOverflow[ENCODER_HEAD]++;
        else
            s_encoderOverflow[ENCODER_HEAD]--;
    }
}

void __attribute__((used)) TIM3_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(TIM3))
    {
        LL_TIM_ClearFlag_UPDATE(TIM3);

        // Track overflow/underflow
        if (LL_TIM_GetDirection(TIM3) == LL_TIM_COUNTERDIRECTION_UP)
            s_encoderOverflow[ENCODER_COLUMN]++;
        else
            s_encoderOverflow[ENCODER_COLUMN]--;
    }
}

void __attribute__((used)) EXTI0_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);

        // Z-index pulse detected on head encoder (PA0)
        // Reset counter to zero for homing
        s_encoderOverflow[ENCODER_HEAD] = 0;
        LL_TIM_SetCounter(TIM2, 0x8000);
        s_homed[ENCODER_HEAD] = true;
    }
}

void __attribute__((used)) EXTI9_5_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);

        // Z-index pulse detected on column encoder (PA5)
        // Reset counter to zero for homing
        s_encoderOverflow[ENCODER_COLUMN] = 0;
        LL_TIM_SetCounter(TIM3, 0x8000);
        s_homed[ENCODER_COLUMN] = true;
    }
}
