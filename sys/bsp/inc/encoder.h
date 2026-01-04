/**
 * @file encoder.h
 * @brief quadrature encoder interface for position tracking
 * 
 * z-index signals (PA0, PA5) not currently used
 */

#ifndef _ENCODER_H_
#define _ENCODER_H_

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
#include <stm32f1xx_ll_tim.h>

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief Encoder identifiers */
typedef enum
{
    ENCODER_COLUMN = 0,
    ENCODER_HEAD,
    ENCODER_COUNT
} EEncoderId_t;

typedef struct
{
    TIM_TypeDef *pTimer;

    SGenericGPIOPin_t aPin;
    SGenericGPIOPin_t bPin;
    SEXTIGPIOPin_t    zPin;
} SEncoderConfig_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief Initialize encoder timer in quadrature decoder mode
 * @param pEncoderConfig Encoder channel to initialize
 */
void bspEncoderInit(EEncoderId_t encoder, const SEncoderConfig_t *pEncoderConfig);

/**
 * @brief Get current encoder position
 * @param encoder Encoder channel
 * @return Signed 32-bit encoder count (negative = reverse direction)
 */
int32_t bspEncoderGetCount(EEncoderId_t encoder);

/**
 * @brief Reset encoder counter to zero
 * @param encoder Encoder channel
 */
void bspEncoderReset(EEncoderId_t encoder);

/**
 * @brief Get encoder velocity (for motion profiling)
 * @param encoder Encoder channel
 * @return Velocity in counts per millisecond
 */
int32_t bspEncoderGetVelocity(EEncoderId_t encoder);

/**
 * @brief Check if Z-index reference has been found
 * @param encoder Encoder channel
 * @return true if Z-index pulse has been detected since init/reset
 * @note This only indicates encoder sync, not machine homing status
 */
bool bspEncoderHasZIndexReference(EEncoderId_t encoder);

/**
 * @brief Clear Z-index reference flag without resetting counter
 * @param encoder Encoder channel
 */
void bspEncoderClearZIndexReference(EEncoderId_t encoder);

/**
 * @brief Enable/disable auto-reset of counter on Z pulse
 * @param encoder Encoder channel
 * @param enable true = reset counter on Z pulse (for homing), false = only set flag
 */
void bspEncoderSetAutoResetOnZ(EEncoderId_t encoder, bool enable);

#ifdef __cplusplus
}
#endif
#endif //!_ENCODER_H_
