/**
 * @file crc.c
 * @author Toesoe (thijs@nbtg.dev, github.com/Toesoe)
 * @brief crc32 bindings
 * @version 0.1
 * @date 15-02-2026
 * 
 * @copyright Copyright (c) 2026
 * 
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "crc.h"

#include <stdbool.h>
#include <stm32f1xx_ll_crc.h>
#include <stm32f1xx_ll_bus.h>
#include <string.h>
#include "stm32f103xb.h"

//=====================================================================================================================
// Private types
//=====================================================================================================================
//=====================================================================================================================
// Private data
//=====================================================================================================================

//=====================================================================================================================
// Public functions
//=====================================================================================================================

void bspInitCRC(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);
}

void bspStartCRCBlock(void)
{
    LL_CRC_ResetCRCCalculationUnit(CRC);
    LL_CRC_Write_IDR(CRC, 0);
}

void bspFeedCRCBlock(const uint8_t *pData, size_t size)
{
    for (size_t index = 0; index < size / 4; index++)
    {
        uint32_t data = (uint32_t)((pData[4 * index + 3] << 24) | (pData[4 * index + 2] << 16) | (pData[4 * index + 1] << 8) | pData[4 * index]);
        LL_CRC_FeedData32(CRC, data);
    }
}

uint32_t bspCalculateCRCBlock(void)
{
    return LL_CRC_ReadData32(CRC);
}

uint32_t bspCalculateCRCData(const uint8_t *pData, size_t size)
{
    bspStartCRCBlock();
    bspFeedCRCBlock(pData, size);
    return bspCalculateCRCBlock();
}