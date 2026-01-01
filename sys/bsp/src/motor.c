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

static uint32_t getTimerChannel(EMotorId_t motor, bool isIn2)
{
    // TIM1 channel mapping:
    // MOTOR_COLUMN: CH1 (IN1), CH2 (IN2)
    // MOTOR_HEAD:   CH3 (IN1), CH4 (IN2)

    if (motor == MOTOR_COLUMN)
        return isIn2 ? LL_TIM_CHANNEL_CH2 : LL_TIM_CHANNEL_CH1;
    else if (motor == MOTOR_HEAD)
        return isIn2 ? LL_TIM_CHANNEL_CH4 : LL_TIM_CHANNEL_CH3;

    return LL_TIM_CHANNEL_CH1;
}

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

void bspMotorInit(const SMotorConfig_t *config)
{
    if (!config || config->motorId >= MOTOR_COUNT) return;

    s_motorConfigs[config->motorId] = config;
    TIM_TypeDef *pTimer             = config->pTimer;

    // Enable timer clock (TIM1 is on APB2)
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

    // Configure GPIO pins as alternate function push-pull
    LL_GPIO_InitTypeDef gpioInit = { 0 };
    gpioInit.Mode                = LL_GPIO_MODE_ALTERNATE;
    gpioInit.Speed               = LL_GPIO_SPEED_FREQ_HIGH;
    gpioInit.OutputType          = LL_GPIO_OUTPUT_PUSHPULL;
    gpioInit.Pull                = LL_GPIO_PULL_UP;

    // IN1 pin
    gpioInit.Pin = config->in1Pin.pinPort.pin;
    LL_GPIO_Init(config->in1Pin.pinPort.port, &gpioInit);

    // IN2 pin
    gpioInit.Pin = config->in2Pin.pinPort.pin;
    LL_GPIO_Init(config->in2Pin.pinPort.port, &gpioInit);

    // Configure timer base (only once for TIM1)
    if (config->motorId == MOTOR_COLUMN)
    {
        LL_TIM_SetPrescaler(pTimer, 0); // No prescaler, full 64 MHz

        // ARR = (Timer Clock / PWM Frequency) - 1
        // 64 MHz / 25 kHz = 2560
        uint32_t arr = (64000000UL / config->pwmFrequency) - 1;
        LL_TIM_SetAutoReload(pTimer, arr);

        LL_TIM_SetCounterMode(pTimer, LL_TIM_COUNTERMODE_UP);
        LL_TIM_SetClockDivision(pTimer, LL_TIM_CLOCKDIVISION_DIV1);
        LL_TIM_SetRepetitionCounter(pTimer, 0);

        // Enable main output (required for TIM1/TIM8)
        LL_TIM_EnableAllOutputs(pTimer);
    }

    // Configure PWM channels for this motor
    LL_TIM_OC_InitTypeDef ocInit = { 0 };
    ocInit.OCMode                = LL_TIM_OCMODE_PWM1;
    ocInit.OCState               = LL_TIM_OCSTATE_ENABLE;
    ocInit.OCPolarity            = LL_TIM_OCPOLARITY_HIGH;
    ocInit.OCIdleState           = LL_TIM_OCIDLESTATE_LOW;
    ocInit.CompareValue          = 0; // Start with 0% duty

    // IN1 channel
    uint32_t ch1 = getTimerChannel(config->motorId, false);
    LL_TIM_OC_Init(pTimer, ch1, &ocInit);
    LL_TIM_OC_EnablePreload(pTimer, ch1);

    // IN2 channel
    uint32_t ch2 = getTimerChannel(config->motorId, true);
    LL_TIM_OC_Init(pTimer, ch2, &ocInit);
    LL_TIM_OC_EnablePreload(pTimer, ch2);

    // Enable counter (only once)
    if (config->motorId == MOTOR_COLUMN)
    {
        LL_TIM_EnableCounter(pTimer);
        LL_TIM_GenerateEvent_UPDATE(pTimer);
    }

    // Start in coast mode (both outputs LOW)
    bspMotorStop(config->motorId, MOTOR_BRAKE_COAST);
}

void bspMotorSetSpeed(EMotorId_t motor, int16_t speed)
{
    if (motor >= MOTOR_COUNT || !s_motorConfigs[motor] || !s_motorsEnabled) return;

    const SMotorConfig_t *config = s_motorConfigs[motor];
    TIM_TypeDef          *pTimer = config->pTimer;

    // Clamp speed
    if (speed > SPEED_MAX) speed = SPEED_MAX;
    if (speed < SPEED_MIN) speed = SPEED_MIN;

    // Invert if configured
    if (config->invertDirection) speed = (int16_t)(-speed);

    uint32_t ch1 = getTimerChannel(motor, false); // IN1
    uint32_t ch2 = getTimerChannel(motor, true);  // IN2
    uint32_t arr = LL_TIM_GetAutoReload(pTimer);

    if (speed > 0)
    {
        // Forward: IN1=PWM, IN2=LOW
        uint32_t duty = ((uint32_t)speed * arr) / SPEED_MAX;
        setChannelDuty(pTimer, ch1, duty);
        setChannelDuty(pTimer, ch2, 0);
    }
    else if (speed < 0)
    {
        // Reverse: IN1=LOW, IN2=PWM
        uint32_t duty = ((uint32_t)(-speed) * arr) / SPEED_MAX;
        setChannelDuty(pTimer, ch1, 0);
        setChannelDuty(pTimer, ch2, duty);
    }
    else
    {
        // Stop: coast mode (both LOW)
        setChannelDuty(pTimer, ch1, 0);
        setChannelDuty(pTimer, ch2, 0);
    }
}

void bspMotorStop(EMotorId_t motor, EMotorBrakeMode_t brakeMode)
{
    if (motor >= MOTOR_COUNT || !s_motorConfigs[motor]) return;

    const SMotorConfig_t *config = s_motorConfigs[motor];
    TIM_TypeDef          *pTimer = config->pTimer;
    uint32_t              ch1    = getTimerChannel(motor, false);
    uint32_t              ch2    = getTimerChannel(motor, true);
    uint32_t              arr    = LL_TIM_GetAutoReload(pTimer);

    if (brakeMode == MOTOR_BRAKE_ACTIVE)
    {
        // Active brake: both IN pins HIGH
        setChannelDuty(pTimer, ch1, arr);
        setChannelDuty(pTimer, ch2, arr);
    }
    else
    {
        // Coast: both IN pins LOW
        setChannelDuty(pTimer, ch1, 0);
        setChannelDuty(pTimer, ch2, 0);
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
