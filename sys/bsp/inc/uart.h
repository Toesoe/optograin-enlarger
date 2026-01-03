/**
 * @file uart.h
 *
 * @brief uart functionality
 */

#ifndef _UART_H_
#define _UART_H_

#include "board.h"
#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "stm32f1xx.h"


//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    USART_TypeDef *pUsart;
    uint32_t baudRate;
    uint8_t dataBits;
    uint8_t stopBits;

    SGenericGPIOPin_t txPin;
    SGenericGPIOPin_t rxPin;
    bool remapAlternateFunction;

} SUartConfig_t;

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Functions
//=====================================================================================================================

void bspUartInit(const SUartConfig_t *);

void toggleUsartRX(bool);

//void setConsoleInputQueue(QueueHandle_t *);

void consolePutchar(char);

#ifdef __cplusplus
}
#endif
#endif //!_UART_H_