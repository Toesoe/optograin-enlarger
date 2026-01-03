/**
 * @file lamp.c
 * @brief Enlarger lamp control and exposure timing
 * 
 * Hardware:
 * - PB12: INA input to UCC27424DR gate driver channel A
 * - Gate driver OUTA drives IRLZ44N N-channel MOSFET through 10Ω resistor
 * - TIM4: Exposure timer (1ms tick)
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "lamp.h"

#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>
#include <stm32f1xx_ll_bus.h>
#include <stddef.h>

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    const SLampConfig_t *pConfig;
    fnLampCallback_t callback;
    void *userCtx;
    uint32_t remainingTicks;
    bool exposureActive;
} SLampState_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SLampState_t gs_lampState = { 0 };

//=====================================================================================================================
// Functions
//=====================================================================================================================

void bspLampInit(const SLampConfig_t *pLampConfig)
{
    gs_lampState.pConfig = pLampConfig;
    gs_lampState.exposureActive = false;
    gs_lampState.remainingTicks = 0;
    gs_lampState.callback = NULL;

    // Configure control pin as output push-pull
    LL_GPIO_InitTypeDef gpio = {
        .Pin = pLampConfig->controlPin.pinPort.pin,
        .Mode = LL_GPIO_MODE_OUTPUT,
        .Speed = LL_GPIO_SPEED_FREQ_LOW,
        .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
    };
    LL_GPIO_Init(pLampConfig->controlPin.pinPort.port, &gpio);

    // Start with lamp off
    LL_GPIO_ResetOutputPin(pLampConfig->controlPin.pinPort.port, pLampConfig->controlPin.pinPort.pin);

    // Configure TIM4 for 1ms tick (APB1 = 32 MHz, timer clock = 64 MHz due to x2 multiplier)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);

    // Timer clock = 64 MHz (APB1 with x2 multiplier when APB1 prescaler != 1)
    // For 1 kHz (1ms tick): prescaler = 64000 - 1, period = 1000 - 1
    LL_TIM_SetPrescaler(pLampConfig->pTimer, 64 - 1);       // 64 MHz / 64 = 1 MHz
    LL_TIM_SetAutoReload(pLampConfig->pTimer, 1000 - 1);    // 1 MHz / 1000 = 1 kHz (1ms)
    LL_TIM_SetCounterMode(pLampConfig->pTimer, LL_TIM_COUNTERMODE_UP);

    // Enable update interrupt
    LL_TIM_EnableIT_UPDATE(pLampConfig->pTimer);
    
    // Enable TIM4 interrupt in NVIC
    NVIC_SetPriority(TIM4_IRQn, 1);
    NVIC_EnableIRQ(TIM4_IRQn);

    // Timer starts disabled - only enabled during exposure
}

void bspLampSet(bool on)
{
    if (gs_lampState.pConfig == NULL)
    {
        return;
    }

    if (on)
    {
        LL_GPIO_SetOutputPin(gs_lampState.pConfig->controlPin.pinPort.port,
                             gs_lampState.pConfig->controlPin.pinPort.pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(gs_lampState.pConfig->controlPin.pinPort.port,
                               gs_lampState.pConfig->controlPin.pinPort.pin);
    }
}

bool bspLampGetState(void)
{
    if (gs_lampState.pConfig == NULL)
    {
        return false;
    }

   return LL_GPIO_IsOutputPinSet(gs_lampState.pConfig->controlPin.pinPort.port,
                                           gs_lampState.pConfig->controlPin.pinPort.pin);
}

void bspLampStartExposure(uint32_t duration_ms, fnLampCallback_t callback, void *userCtx)
{
    if (gs_lampState.pConfig == NULL || duration_ms == 0)
    {
        return;
    }

    // Cancel any existing exposure
    bspLampCancelExposure();

    // Set up new exposure
    gs_lampState.remainingTicks = duration_ms;
    gs_lampState.callback = callback;
    gs_lampState.userCtx = userCtx;
    gs_lampState.exposureActive = true;

    // Turn on lamp
    bspLampSet(true);

    // Start timer
    LL_TIM_SetCounter(gs_lampState.pConfig->pTimer, 0);
    LL_TIM_EnableCounter(gs_lampState.pConfig->pTimer);
}

void bspLampCancelExposure(void)
{
    if (gs_lampState.pConfig == NULL)
    {
        return;
    }

    // Stop timer
    LL_TIM_DisableCounter(gs_lampState.pConfig->pTimer);

    // Turn off lamp
    bspLampSet(false);

    gs_lampState.exposureActive = false;
    gs_lampState.remainingTicks = 0;
    gs_lampState.callback = NULL;
}

bool bspLampIsExposureActive(void)
{
    return gs_lampState.exposureActive;
}

uint32_t bspLampGetRemainingTime(void)
{
    return gs_lampState.remainingTicks;
}

//=====================================================================================================================
// ISR
//=====================================================================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

void TIM4_IRQHandler(void)
{
    if (LL_TIM_IsActiveFlag_UPDATE(gs_lampState.pConfig->pTimer))
    {
        LL_TIM_ClearFlag_UPDATE(gs_lampState.pConfig->pTimer);

        if (gs_lampState.exposureActive && gs_lampState.remainingTicks > 0)
        {
            gs_lampState.remainingTicks--;

            if (gs_lampState.remainingTicks == 0)
            {
                // Exposure complete - turn off lamp
                bspLampSet(false);
                LL_TIM_DisableCounter(gs_lampState.pConfig->pTimer);
                gs_lampState.exposureActive = false;

                // Call callback if registered
                if (gs_lampState.callback != NULL)
                {
                    gs_lampState.callback(gs_lampState.userCtx);
                }
            }
        }
    }
}

#pragma GCC diagnostic pop