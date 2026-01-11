/**
 * @file motor.h
 * @brief Motor control interface for enlarger positioning
 *
 * Hardware:
 * - 2x DRV8871DA brushed DC motor drivers (H-bridge)
 * - TIM1 PWM generation (all 4 channels)
 *
 */

#ifndef _MOTOR_H_
#define _MOTOR_H_

#include "stm32f103xb.h"
#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>

#include "board.h"

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief Motor identifiers */
typedef enum
{
    MOTOR_COLUMN = 0, /**< Column motor (TIM1_CH1/CH2 → DRV8871 #1) */
    MOTOR_HEAD,       /**< Head/lens motor (TIM1_CH3/CH4 → DRV8871 #2) */
    MOTOR_COUNT
} EMotorId_t;

/** @brief Motor brake mode */
typedef enum
{
    MOTOR_BRAKE_COAST = 0, /**< Low-side coast (both IN pins LOW) */
    MOTOR_BRAKE_ACTIVE,    /**< Active brake (both IN pins HIGH) */
} EMotorBrakeMode_t;

/** @brief Motor hardware configuration */
typedef struct
{
    TIM_TypeDef      *pTimer;
    uint32_t          pwmFrequency;

    SGenericGPIOPin_t in1Pin;
    SGenericGPIOPin_t in2Pin;
    uint32_t          in1Channel;
    uint32_t          in2Channel;
    bool              useRemapPins;

    bool              invertDirection;
    uint8_t           minDutyPercent; /**< Minimum duty cycle (0-100) to overcome deadband/friction */
} SMotorConfig_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief Initialize motor PWM outputs
 * Configures TIM1 CH1-4 for DRV8871 control
 * @param config Motor configuration (defined in board.h)
 * Default: Motors disabled, coast mode
 */
void bspMotorInit(EMotorId_t motorId, const SMotorConfig_t *config);

/**
 * @brief Set motor speed and direction
 * @param motor Motor channel
 * @param speed Speed value: -1000 (full reverse) to +1000 (full forward), 0 = stop
 */
void bspMotorSetSpeed(EMotorId_t motor, int16_t speed);

/**
 * @brief Stop specific motor with brake mode
 * @param motor Motor channel
 * @param brakeMode Coast or active brake
 */
void bspMotorStop(EMotorId_t motor, EMotorBrakeMode_t brakeMode);

/**
 * @brief Emergency stop - immediately halt all motors
 * Uses active brake mode
 */
void bspMotorEmergencyStop(void);

/**
 * @brief Enable/disable motor driver outputs
 * @param enable true to enable, false to disable (high-Z state)
 */
void bspMotorEnable(bool enable);

#ifdef __cplusplus
}
#endif
#endif //!_MOTOR_H_
