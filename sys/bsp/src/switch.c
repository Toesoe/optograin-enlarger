/**
 * @file switch.c
 * @author Toesoe (thijs@nbtg.dev, github.com/Toesoe)
 * @brief switch reading and debouncing for lens selection and limit switches
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
#include "stm32f1xx_ll_gpio.h"

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
 * @return true if initialization was successful
 * @return false if initialization failed or invalid parameters
 */
bool bspSwitchInit(ESwitchId_t switch_id, const SGenericGPIOPin_t *pin)
{
    if (switch_id >= SWITCH_COUNT || pin == NULL)
    {
        return false;
    }

    LL_GPIO_InitTypeDef gpioInit = {0};

    gpioInit.Pin   = pin->pinPort.pin;
    gpioInit.Mode  = LL_GPIO_MODE_FLOATING;
    gpioInit.Speed = LL_GPIO_SPEED_FREQ_LOW;
    LL_GPIO_Init(pin->pinPort.port, &gpioInit);

    switch_states[switch_id].pin = pin;
    switch_states[switch_id].initialized = true;

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

    if (stateA && stateB) return 0; // No lens
    if (!stateA && stateB) return 1;
    if (stateA && !stateB) return 2;
    if (!stateA && !stateB) return 3;
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
