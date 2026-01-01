/**
 * @file systick.h
 * @brief System timebase for delays and timing
 * 
 * Hardware: TIM4 (1kHz interrupt)
 * Also handles software PWM for fan on PB14
 */

#ifndef _SYSTICK_H_
#define _SYSTICK_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief System timer callback function pointer */
typedef void (*fnSystickCallback_t)(void *userCtx);

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief Initialize system timebase
 */
void bspSystickInit(void);

/**
 * @brief Get milliseconds since initialization
 * @return Millisecond count (32-bit, wraps after ~49 days)
 */
uint32_t bspSystickGetMs(void);

/**
 * @brief Get microseconds since initialization
 * @return Microsecond count
 */
uint32_t bspSystickGetUs(void);

/**
 * @brief Blocking delay in milliseconds
 * @param ms Delay duration
 */
void bspSystickDelayMs(uint32_t ms);

/**
 * @brief Blocking delay in microseconds
 * @param us Delay duration
 */
void bspSystickDelayUs(uint32_t us);

/**
 * @brief Register periodic callback for control loops
 * @param callback Function to call periodically
 * @param userCtx User context pointer passed to callback
 * @param period_ms Period in milliseconds
 */
void bspSystickRegisterCallback(FnSystickCallback_t callback, void *userCtx, uint32_t period_ms);

#ifdef __cplusplus
}
#endif
#endif //!_SYSTICK_H_
