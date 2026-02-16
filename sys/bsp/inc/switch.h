/**
 * @file switch.h
 * @author Toesoe (thijs@nbtg.dev, github.com/Toesoe)
 * @brief switch reading and debouncing for lens selection and limit switches
 * @version 0.1
 * @date 15-02-2026
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef _SWITCH_H_
#define _SWITCH_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "board.h"

#include <stdint.h>
#include <stddef.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef enum
{
    SWITCH_LENS_INDEX_A = 0,
    SWITCH_LENS_INDEX_B,
    SWITCH_COLUMN_TOP_LIMIT,
    SWITCH_COLUMN_BOTTOM_LIMIT,
    SWITCH_HEAD_TOP_LIMIT,
    SWITCH_HEAD_BOTTOM_LIMIT,
    SWITCH_COUNT
} ESwitchId_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

bool bspSwitchInit(ESwitchId_t, const SGenericGPIOPin_t *);
bool bspSwitchRead(ESwitchId_t);
int  bspSwitchGetCurrentLensIndex(void);

void bspSwitchAttachCallback(ESwitchId_t, void (*)(void *), void *);

#ifdef __cplusplus
}
#endif
#endif //!_SWITCH_H_
