/**
 * @file lamp.h
 * @brief Enlarger lamp control and exposure timing
 * 
 * Hardware: 
 * - GPIO MOSFET control (on/off)
 * - TIM4 exposure timer
 */

#ifndef _LAMP_H_
#define _LAMP_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "board.h"

#include <stdbool.h>
#include <stdint.h>

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef void (*fnLampCallback_t)(void *userCtx);

typedef struct
{
    SGenericGPIOPin_t pwmPin;
    SGenericGPIOPin_t tachPin;
} SFanConfig_t;

typedef struct
{
    TIM_TypeDef *pTimer;
    SGenericGPIOPin_t controlPin;
    SFanConfig_t fanConfig;
} SLampConfig_t;

typedef struct
{
    uint32_t fanSpeedRPM;
    uint8_t dutyCyclePercent;
    bool hasError;
} SFanStatus_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

// Lamp control
void bspLampInit(const SLampConfig_t *pLampConfig);
void bspLampSet(bool on);
bool bspLampGetState(void);

void bspLampStartExposure(uint32_t duration_ms, fnLampCallback_t callback, void *userCtx);
void bspLampCancelExposure(void);
bool bspLampIsExposureActive(void);
uint32_t bspLampGetRemainingTime(void);

// Fan PWM control
void bspFanInit(const SFanConfig_t *pFanConfig);
void bspFanSetDuty(uint8_t percent);
SFanStatus_t bspFanGetStatus(void);

// Fan tach (prepared for future use)
void bspFanTachInit(void);

#ifdef __cplusplus
}
#endif
#endif //!_LAMP_H_
