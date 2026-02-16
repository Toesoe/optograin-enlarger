/**
 * @file switch.c
 * @author Toesoe (thijs@nbtg.dev, github.com/Toesoe)
 * @brief switch reading for lens selection and limit switches
 * @version 0.1
 * @date 15-02-2026
 * 
 * @copyright Copyright (c) 2026
 * 
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "switch.h"
#include "stm32f1xx_ll_exti.h"
#include "stm32f1xx_ll_gpio.h"
#include "board.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    const SGenericGPIOPin_t *pin;
    void (*callback)(void *);
    void *user_data;
    bool initialized;
} SSwitchState_t;

//=====================================================================================================================
// Static variables
//=====================================================================================================================

static SSwitchState_t switch_states[SWITCH_COUNT] = {0};

//=====================================================================================================================
// Public Functions
//=====================================================================================================================

/**
 * @brief Initialize a switch with the given GPIO pin
 * 
 * @param switch_id The switch identifier
 * @param pin Pointer to the GPIO pin configuration
 * @param pExtiPin Pointer to the EXTI pin configuration (optional, for interrupt-capable switches)
 * @return true if initialization was successful
 * @return false if initialization failed or invalid parameters
 *
 * @note either pin or pExtiPin must be provided, but not both. EXTI pins will be configured for interrupt handling.
 */
bool bspSwitchInit(ESwitchId_t switch_id, const SGenericGPIOPin_t *pin, const SEXTIGPIOPin_t *pExtiPin)
{
    if (switch_id >= SWITCH_COUNT || (pin == NULL && pExtiPin == NULL))
    {
        return false;
    }

    LL_GPIO_InitTypeDef gpioInit = {0};

    gpioInit.Pin   = pin != nullptr ? pin->pinPort.pin : pExtiPin->pinPort.pin;
    gpioInit.Mode  = LL_GPIO_MODE_INPUT;
    gpioInit.Speed = LL_GPIO_SPEED_FREQ_LOW;
    LL_GPIO_Init(pin != nullptr ? pin->pinPort.port : pExtiPin->pinPort.port, &gpioInit);

    switch_states[switch_id].pin = pin != nullptr ? pin : (const SGenericGPIOPin_t *)pExtiPin; // Store as generic pin for reading; both types have pinPort at position 1
    switch_states[switch_id].initialized = true;

    if (pExtiPin != nullptr)
    {
        uint32_t extiSrc = (pExtiPin->pinPort.port == GPIOA) ? LL_GPIO_AF_EXTI_PORTA : LL_GPIO_AF_EXTI_PORTB;

        if (pExtiPin->extiLine == LL_EXTI_LINE_0)
        {
            LL_GPIO_AF_SetEXTISource(extiSrc, LL_GPIO_AF_EXTI_LINE0);
        }
        else if (pExtiPin->extiLine == LL_EXTI_LINE_12)
        {
            LL_GPIO_AF_SetEXTISource(extiSrc, LL_GPIO_AF_EXTI_LINE12);
        }
    }
    
    return true;
}

/**
 * @brief Read the current state of a switch
 * 
 * @param switch_id The switch identifier
 * @return true if switch is pressed/active
 * @return false if switch is not pressed/inactive or not initialized
 */
bool bspSwitchRead(ESwitchId_t switch_id)
{
    if ((switch_id >= SWITCH_COUNT) || (switch_id < SWITCH_COLUMN_TOP_LIMIT))
    {
        return false;
    }

    if (!switch_states[switch_id].initialized || (switch_states[switch_id].pin == NULL))
    {
        return false;
    }

    return LL_GPIO_IsInputPinSet(switch_states[switch_id].pin->pinPort.port, switch_states[switch_id].pin->pinPort.pin);
}

int bspSwitchGetCurrentLensIndex(void)
{
    bool stateA = LL_GPIO_IsInputPinSet(switch_states[SWITCH_LENS_INDEX_A].pin->pinPort.port, switch_states[SWITCH_LENS_INDEX_A].pin->pinPort.pin);
    bool stateB = LL_GPIO_IsInputPinSet(switch_states[SWITCH_LENS_INDEX_B].pin->pinPort.port, switch_states[SWITCH_LENS_INDEX_B].pin->pinPort.pin);

    if (!stateA && stateB) return 1;
    if (stateA && !stateB) return 2;
    if (!stateA && !stateB) return 3;

    return 0; // no lens selected
}

/**
 * @brief Attach a callback function to a switch event
 * 
 * @param switch_id The switch identifier
 * @param callback Function pointer to call on switch event
 * @param user_data User data to pass to the callback
 */
void bspSwitchAttachCallback(ESwitchId_t switch_id, void (*callback)(void *), void *user_data)
{
    if (switch_id >= SWITCH_COUNT)
    {
        return;
    }

    switch_states[switch_id].callback = callback;
    switch_states[switch_id].user_data = user_data;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

void __attribute__((used)) EXTI0_IRQHandler(void)
{
    // line 0 = head top limit switch
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);

        if (switch_states[SWITCH_HEAD_TOP_LIMIT].callback)
        {
            switch_states[SWITCH_HEAD_TOP_LIMIT].callback(switch_states[SWITCH_HEAD_TOP_LIMIT].user_data);
        }
    }
}

void __attribute__((used)) EXTI15_10_IRQHandler(void)
{
    // line 12 = column top limit switch
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_12))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_12);

        if (switch_states[SWITCH_COLUMN_TOP_LIMIT].callback)
        {
            switch_states[SWITCH_COLUMN_TOP_LIMIT].callback(switch_states[SWITCH_COLUMN_TOP_LIMIT].user_data);
        }
    }
}

#pragma GCC diagnostic pop