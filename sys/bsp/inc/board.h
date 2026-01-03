/**
 * @file board.h
 *
 * @brief board-specific functionality
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stm32f1xx_ll_gpio.h>

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief basic GPIO pin/port definition */
typedef struct
{
    uint32_t pin;
    GPIO_TypeDef *port;
} SGPIOPin_t;

/** @brief GPIO pin/port definition with input/output flag */
typedef struct
{
    SGPIOPin_t pinPort;
    bool isOutput;
} SGenericGPIOPin_t;

/** @brief EXTI mapped GPIO pin/port definition */
typedef struct
{
    SGPIOPin_t gpio;
    uint32_t extiLine;
    uint32_t extiPort;
} SEXTIGPIOPin_t;

//=====================================================================================================================
// Inline Helpers
//=====================================================================================================================

/**
 * @brief Convert LL_GPIO_PIN_x to LL_EXTI_LINE_x
 * @param gpioPin GPIO pin bitmask (LL_GPIO_PIN_0, LL_GPIO_PIN_5, etc.)
 * @return EXTI line value (LL_EXTI_LINE_0, LL_EXTI_LINE_5, etc.)
 */
static inline uint32_t gpioPinToExtiLine(uint32_t gpioPin)
{
    // LL_GPIO_PIN_n is a bitmask (1 << n), find bit position
    uint32_t line = 0;
    while (gpioPin > 1)
    {
        gpioPin >>= 1;
        line++;
    }
    return (1U << line);  // LL_EXTI_LINE_x format
}

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initBoard(void);
void hwDelayMs(uint32_t);

#ifdef __cplusplus
}
#endif
#endif //!_BOARD_H_