/**
 * @file motor.c
 * @brief Motor control implementation using TIM1 PWM for DRV8871 drivers
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "motor.h"

#include <stddef.h>
#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_tim.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SPEED_MAX  (1000)
#define SPEED_MIN  (-1000)
#define PWM_PERIOD (1000) // 0-1000 PWM range for easy percentage mapping

//=====================================================================================================================
// Private Data
//=====================================================================================================================

static const SMotorConfig_t *s_motorConfigs[MOTOR_COUNT] = { NULL };
static bool                  s_motorsEnabled             = false;

//=====================================================================================================================
// Private Functions
//=====================================================================================================================

static void setChannelDuty(TIM_TypeDef *pTimer, uint32_t channel, uint32_t duty)
{
    switch (channel)
    {
        case LL_TIM_CHANNEL_CH1:
            LL_TIM_OC_SetCompareCH1(pTimer, duty);
            break;
        case LL_TIM_CHANNEL_CH2:
            LL_TIM_OC_SetCompareCH2(pTimer, duty);
            break;
        case LL_TIM_CHANNEL_CH3:
            LL_TIM_OC_SetCompareCH3(pTimer, duty);
            break;
        case LL_TIM_CHANNEL_CH4:
            LL_TIM_OC_SetCompareCH4(pTimer, duty);
            break;
    }
}

//=====================================================================================================================
// Public Functions
//=====================================================================================================================

void bspMotorInit(EMotorId_t motorId, const SMotorConfig_t *pConfig)
{
    if (!pConfig || motorId >= MOTOR_COUNT) return;

    s_motorConfigs[motorId] = pConfig;

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

    LL_GPIO_InitTypeDef gpioInit = { 0 };
    gpioInit.Mode                = LL_GPIO_MODE_ALTERNATE;
    gpioInit.Speed               = LL_GPIO_SPEED_FREQ_HIGH;
    gpioInit.OutputType          = LL_GPIO_OUTPUT_PUSHPULL;
    gpioInit.Pull                = LL_GPIO_PULL_DOWN;

    gpioInit.Pin = pConfig->in1Pin.pinPort.pin;
    LL_GPIO_Init(pConfig->in1Pin.pinPort.port, &gpioInit);

    gpioInit.Pin = pConfig->in2Pin.pinPort.pin;
    LL_GPIO_Init(pConfig->in2Pin.pinPort.port, &gpioInit);

    if (pConfig->useRemapPins)
    {
        // Remap TIM1 pins (partial remap)
        LL_GPIO_AF_RemapPartial_TIM1();
    }

    // Configure timer base (only once for TIM1)
    if (!LL_TIM_IsEnabledAllOutputs(pConfig->pTimer))
    {
        LL_TIM_SetPrescaler(pConfig->pTimer, 0);

        // ARR = (Timer Clock / PWM Frequency) - 1
        // 64 MHz / 25 kHz = 2560
        uint32_t arr = (SystemCoreClock / pConfig->pwmFrequency) - 1;
        LL_TIM_SetAutoReload(pConfig->pTimer, arr);

        LL_TIM_SetCounterMode(pConfig->pTimer, LL_TIM_COUNTERMODE_UP);
        LL_TIM_SetClockDivision(pConfig->pTimer, LL_TIM_CLOCKDIVISION_DIV1);
        LL_TIM_SetRepetitionCounter(pConfig->pTimer, 0);

        LL_TIM_EnableAllOutputs(pConfig->pTimer);
    }

    // Configure PWM channels for this motor
    LL_TIM_OC_InitTypeDef ocInit = { 0 };
    ocInit.OCMode                = LL_TIM_OCMODE_PWM1;
    ocInit.OCState               = LL_TIM_OCSTATE_ENABLE;
    ocInit.OCPolarity            = LL_TIM_OCPOLARITY_HIGH;
    ocInit.OCIdleState           = LL_TIM_OCIDLESTATE_LOW;
    ocInit.CompareValue          = 0; // Start with 0% duty

    LL_TIM_OC_Init(pConfig->pTimer, pConfig->in1Channel, &ocInit);
    LL_TIM_OC_EnablePreload(pConfig->pTimer, pConfig->in1Channel);

    LL_TIM_OC_Init(pConfig->pTimer, pConfig->in2Channel, &ocInit);
    LL_TIM_OC_EnablePreload(pConfig->pTimer, pConfig->in2Channel);

    if (!LL_TIM_IsEnabledCounter(pConfig->pTimer))
    {
        LL_TIM_EnableCounter(pConfig->pTimer);
        LL_TIM_GenerateEvent_UPDATE(pConfig->pTimer);
    }

    // Start in coast mode (both outputs LOW)
    bspMotorStop(motorId, MOTOR_BRAKE_COAST);
}

void bspMotorSetSpeed(EMotorId_t motor, int16_t speed)
{
    if ((motor >= MOTOR_COUNT) || !s_motorConfigs[motor] || !s_motorsEnabled) return;

    // Clamp speed
    if (speed > SPEED_MAX) speed = SPEED_MAX;
    if (speed < SPEED_MIN) speed = SPEED_MIN;

    // Invert if configured
    if (s_motorConfigs[motor]->invertDirection) speed = (int16_t)(-speed);

    uint32_t arr = LL_TIM_GetAutoReload(s_motorConfigs[motor]->pTimer);
    uint32_t minDuty = (arr * s_motorConfigs[motor]->minDutyPercent) / 100;

    if (speed > 0)
    {
        // Forward IN1=HIGH, IN2=PWM (inverted duty)
        uint32_t pwmDuty = minDuty + (((uint32_t)speed * (arr - minDuty)) / SPEED_MAX);

        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in1Channel, arr);
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in2Channel, arr - pwmDuty);
    }
    else if (speed < 0)
    {
        // Reverse IN1=PWM, IN2=HIGH
        uint32_t pwmDuty = minDuty + (((uint32_t)(-speed) * (arr - minDuty)) / SPEED_MAX);

        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in1Channel, arr - pwmDuty);
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in2Channel, arr);
    }
    else
    {
        // Brake IN1=HIGH, IN2=HIGH
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in1Channel, arr);
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in2Channel, arr);
    }
}

void bspMotorStop(EMotorId_t motor, EMotorBrakeMode_t brakeMode)
{
    if ((motor >= MOTOR_COUNT) || !s_motorConfigs[motor]) return;

    if (brakeMode == MOTOR_BRAKE_ACTIVE)
    {
        // Active brake: both HIGH
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in1Channel, LL_TIM_GetAutoReload(s_motorConfigs[motor]->pTimer));
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in2Channel, LL_TIM_GetAutoReload(s_motorConfigs[motor]->pTimer));
    }
    else
    {
        // Coast: both LOW
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in1Channel, 0);
        setChannelDuty(s_motorConfigs[motor]->pTimer, s_motorConfigs[motor]->in2Channel, 0);
    }
}

void bspMotorEmergencyStop(void)
{
    // Active brake on all motors
    for (uint8_t i = 0; i < MOTOR_COUNT; i++)
    {
        bspMotorStop((EMotorId_t)i, MOTOR_BRAKE_ACTIVE);
    }
    s_motorsEnabled = false;
}

void bspMotorEnable(bool enable)
{
    s_motorsEnabled = enable;

    if (!enable)
    {
        // Disable all motors (coast mode)
        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            bspMotorStop((EMotorId_t)i, MOTOR_BRAKE_COAST);
        }
    }
}
