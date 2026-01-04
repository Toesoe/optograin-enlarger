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
    SGPIOPin_t pinPort;
    uint32_t extiLine;
} SEXTIGPIOPin_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initBoard(void);
void hwDelayMs(uint32_t);

uint32_t getCurrentSystick(void);

#ifdef __cplusplus
}
#endif
#endif //!_BOARD_H_