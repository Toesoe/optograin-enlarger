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
    volatile bool           isHomed;
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

    gs_encoderStates[encoderId].pConfig = pEncoderConfig;
    // Enable timer clock
    if (gs_encoderStates[encoderId].pConfig->pTimer == TIM2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
    }
    else if (gs_encoderStates[encoderId].pConfig->pTimer == TIM3)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    }

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
    LL_TIM_SetEncoderMode(pEncoderConfig->pTimer, LL_TIM_ENCODERMODE_X4_TI12);

    // Configure input channels
    LL_TIM_IC_SetActiveInput(pEncoderConfig->pTimer, pEncoderConfig->aChannel, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetActiveInput(pEncoderConfig->pTimer, pEncoderConfig->bChannel, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPolarity(pEncoderConfig->pTimer, pEncoderConfig->aChannel, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetPolarity(pEncoderConfig->pTimer, pEncoderConfig->bChannel, LL_TIM_IC_POLARITY_RISING);

    LL_TIM_IC_SetFilter(pEncoderConfig->pTimer, pEncoderConfig->aChannel, LL_TIM_IC_FILTER_FDIV1_N2);
    LL_TIM_IC_SetFilter(pEncoderConfig->pTimer, pEncoderConfig->bChannel, LL_TIM_IC_FILTER_FDIV1_N2);

    // Set counter to mid-range to detect both directions
    LL_TIM_SetAutoReload(pEncoderConfig->pTimer, 0xFFFF);
    LL_TIM_SetCounter(pEncoderConfig->pTimer, 0x8000);

    // Enable update interrupt for overflow tracking
    LL_TIM_EnableIT_UPDATE(pEncoderConfig->pTimer);

    if (pEncoderConfig->pTimer == TIM2)
    {
        NVIC_EnableIRQ(TIM2_IRQn);
    }
    else if (pEncoderConfig->pTimer == TIM3)
    {
        NVIC_EnableIRQ(TIM3_IRQn);
    }

    // Configure Z-index pin for homing (if present)
    if (pEncoderConfig->zPin.pinPort.pin != 0)
    {
        LL_GPIO_InitTypeDef zInit = { 0 };
        zInit.Pin                 = pEncoderConfig->zPin.pinPort.pin;
        zInit.Mode                = LL_GPIO_MODE_INPUT;
        LL_GPIO_Init(pEncoderConfig->zPin.pinPort.port, &zInit);

        // Enable EXTI for rising edge (Z pulse)
        uint32_t extiLine = gpioPinToExtiLine(pEncoderConfig->zPin.pinPort.pin);
        LL_EXTI_EnableIT_0_31(extiLine);
        LL_EXTI_EnableRisingTrig_0_31(extiLine);
        
        // Enable appropriate EXTI IRQ
        if (pEncoderConfig->zPin.pinPort.pin == LL_GPIO_PIN_0)
        {
            NVIC_EnableIRQ(EXTI0_IRQn);
        }
        else if (pEncoderConfig->zPin.pinPort.pin >= LL_GPIO_PIN_5 && 
                 pEncoderConfig->zPin.pinPort.pin <= LL_GPIO_PIN_9)
        {
            NVIC_EnableIRQ(EXTI9_5_IRQn);
        }
    }

    // Start encoder
    LL_TIM_EnableCounter(pEncoderConfig->pTimer);
}

int32_t bspEncoderGetCount(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !gs_encoderStates[encoder].pConfig->pTimer) return 0;

    TIM_TypeDef *pTimer = gs_encoderStates[encoder].pConfig->pTimer;
    uint32_t     cnt    = LL_TIM_GetCounter(pTimer);

    // Combine overflow counter with hardware counter
    int32_t fullCount = ((gs_encoderStates[encoder].overflowCount << 16) | (int32_t)(cnt & 0xFFFF));

    // Subtract initial offset (0x8000) to make zero-centered
    return fullCount - 0x8000;
}

void bspEncoderReset(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !gs_encoderStates[encoder].pConfig->pTimer) return;

    gs_encoderStates[encoder].overflowCount = 0;
    gs_encoderStates[encoder].lastCount       = 0;
    gs_encoderStates[encoder].isHomed           = false;
    LL_TIM_SetCounter(gs_encoderStates[encoder].pConfig->pTimer, 0x8000);
}

int16_t bspEncoderGetVelocity(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT || !gs_encoderStates[encoder].pConfig->pTimer) return 0;

    int32_t  currentCount    = bspEncoderGetCount(encoder);
    uint32_t currentTime     = LL_TIM_GetCounter(TIM4); // Assuming TIM4 is 1ms tick

    int32_t  deltaCounts     = currentCount - gs_encoderStates[encoder].lastCount;
    int32_t  deltaTime       = (int32_t)(currentTime - gs_encoderStates[encoder].lastTimestamp);

    gs_encoderStates[encoder].lastCount     = currentCount;
    gs_encoderStates[encoder].lastTimestamp = currentTime;
    if (deltaTime == 0) return 0;

    // Return counts per millisecond
    return (int16_t)(deltaCounts / deltaTime);
}

bool bspEncoderIsHomed(EEncoderId_t encoder)
{
    if (encoder >= ENCODER_COUNT)
        return false;
    
    return gs_encoderStates[encoder].isHomed;
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

void __attribute__((used)) EXTI0_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);

        // Z-index pulse detected on head encoder (PA0)
        // Reset counter to zero for homing
        gs_encoderStates[ENCODER_HEAD].overflowCount = 0;
        LL_TIM_SetCounter(TIM2, 0x8000);
        gs_encoderStates[ENCODER_HEAD].isHomed = true;
    }
}

void __attribute__((used)) EXTI9_5_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);

        // Z-index pulse detected on column encoder (PA5)
        // Reset counter to zero for homing
        gs_encoderStates[ENCODER_COLUMN].overflowCount = 0;
        LL_TIM_SetCounter(TIM3, 0x8000);
        gs_encoderStates[ENCODER_COLUMN].isHomed = true;
    }
}

#pragma GCC diagnostic pop