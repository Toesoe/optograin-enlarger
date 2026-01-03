/**
 * @file uart.c
 *
 * @brief uart functionality
 * 
 * TODO: verify ISR execution time with dual ifs
 * TODO: add DMA when the above TODO is done
 * TODO: timeouts on transmission (when TXNE stays high e.g. data is not flushed out)
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "uart.h"

#include <stm32f1xx_ll_bus.h>
#include <stm32f1xx_ll_gpio.h>
#include <stm32f1xx_ll_usart.h>
#include <stm32f1xx_ll_rcc.h>
#include <stm32f1xx_ll_dma.h>
#include "stm32f103xb.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================


//=====================================================================================================================
// External functions
//=====================================================================================================================

/**
 * @brief initialize UART peripheral
 * 
 * @param pPeripheral USART peripheral (USART1/USART2/USART3)
 * @param baudrate baudrate to use (modbus recommended 9600, console 115200)
 * 
 * @note USARTs have their RX channel disabled on startup to prevent firing RX interrupts
 */
void bspUartInit(const SUartConfig_t *pPeripheralConfig)
{
    USART_TypeDef *pPeripheral = pPeripheralConfig->pUsart;
    uint32_t baudrate = pPeripheralConfig->baudRate;

    // Enable GPIO clocks
    if (pPeripheral == USART1)
    {
        LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
        if (pPeripheralConfig->remapAlternateFunction)
        {
            LL_GPIO_AF_EnableRemap_USART1();
        }
    }
    else if (pPeripheral == USART2)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
        if (pPeripheralConfig->remapAlternateFunction)
        {
            LL_GPIO_AF_EnableRemap_USART2();
        }
    }
    else if (pPeripheral == USART3)
    {
        LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
        if (pPeripheralConfig->remapAlternateFunction)
        {
            LL_GPIO_AF_EnableRemap_USART3();
        }
    }

    // Configure TX pin
    LL_GPIO_SetPinMode(pPeripheralConfig->txPin.pinPort.port, pPeripheralConfig->txPin.pinPort.pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinSpeed(pPeripheralConfig->txPin.pinPort.port, pPeripheralConfig->txPin.pinPort.pin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinOutputType(pPeripheralConfig->txPin.pinPort.port, pPeripheralConfig->txPin.pinPort.pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(pPeripheralConfig->txPin.pinPort.port, pPeripheralConfig->txPin.pinPort.pin, LL_GPIO_PULL_UP);

    // Configure RX pin
    LL_GPIO_SetPinMode(pPeripheralConfig->rxPin.pinPort.port, pPeripheralConfig->rxPin.pinPort.pin, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinSpeed(pPeripheralConfig->rxPin.pinPort.port, pPeripheralConfig->rxPin.pinPort.pin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(pPeripheralConfig->rxPin.pinPort.port, pPeripheralConfig->rxPin.pinPort.pin, LL_GPIO_PULL_UP);

    // USART1 is APB2 at 64MHz, USART2/3 are APB1 at 32MHz
    if (pPeripheral == USART1)
    {
        NVIC_EnableIRQ(USART1_IRQn);
        LL_USART_SetBaudRate(pPeripheral, SystemCoreClock, baudrate);
    }
    else if (pPeripheral == USART2)
    {
        NVIC_EnableIRQ(USART2_IRQn);
        LL_USART_SetBaudRate(pPeripheral, SystemCoreClock/2, baudrate);
    }
    else if (pPeripheral == USART3)
    {
        NVIC_EnableIRQ(USART3_IRQn);
        LL_USART_SetBaudRate(pPeripheral, SystemCoreClock/2, baudrate);
    }

    LL_USART_SetTransferDirection(pPeripheral, LL_USART_DIRECTION_TX_RX);
    LL_USART_ConfigCharacter(pPeripheral, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1); // 8 bits, no parity, 1 start & stop bit

    LL_USART_Enable(pPeripheral);

    LL_USART_DisableDirectionRx(pPeripheral);

    LL_USART_ClearFlag_ORE(pPeripheral);
    LL_USART_EnableIT_RXNE(pPeripheral);
    LL_USART_EnableIT_ERROR(pPeripheral);
}

void toggleUsartRX(bool enable)
{
    // enable ? LL_USART_EnableDirectionRx(g_pConsoleUsart) : LL_USART_DisableDirectionRx(g_pConsoleUsart);
    // while (g_pConsoleUsart->ISR & USART_ISR_RXNE_RXFNE) { (void)g_pConsoleUsart->RDR; } // flush FIFO
}

/**
 * @brief blocking sendchar function for console; waits for TXE flag to be set
 * 
 * @param c char to send
 */
void consolePutchar(char c)
{
    // while (!LL_USART_IsActiveFlag_TXE(g_pConsoleUsart)) { /* infinite wait */ }
    // LL_USART_TransmitData8(g_pConsoleUsart, c);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes" // ignored: no proto, but spawned in startup file

/**
 * @brief ST-named irq handler for usart1. posts current char directly onto queue via ISR-safe routine
 * TODO: handle IRQ errors
 */
__attribute__((interrupt)) void USART1_IRQHandler(void)
{
    uint8_t val = 0;
    LL_USART_ClearFlag_FE(USART1);
    LL_USART_ClearFlag_ORE(USART1);
    LL_USART_ClearFlag_NE(USART1);
    if (LL_USART_IsActiveFlag_RXNE(USART1))
    {
        val = LL_USART_ReceiveData8(USART1); // inlined
        //xQueueSendToBackFromISR(*pConsoleRXQueue, &val, NULL);
    }
}

#pragma GCC diagnostic pop

//=====================================================================================================================
// Statics
//=====================================================================================================================