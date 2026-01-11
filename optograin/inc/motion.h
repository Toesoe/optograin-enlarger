/**
 * @file motion.h
 * @brief Motion control and position tracking for enlarger axes
 */

#ifndef _MOTION_H_
#define _MOTION_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdbool.h>
#include <stdint.h>
#include "encoder.h"

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    float columnCountsPerMm;
    float headCountsPerMm;
    int32_t columnMaxPosition;
    int32_t headMaxPosition;
    uint32_t crc32;
    uint16_t pulsesPerRevolutionColumn;
    uint16_t pulsesPerRevolutionHead;
} SMotionCalibration_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

void motionInit(void);
float motionGetPosition_mm(EEncoderId_t encoder);
int32_t motionGetCount(EEncoderId_t encoder);
float motionGetVelocity_mmps(EEncoderId_t encoder);
void motionStartCalibration(EEncoderId_t encoder);
void motionCalibrate_mm(EEncoderId_t encoder, float distance_mm);
void motionSaveCalibration(void);
bool motionLoadCalibration(void);
bool motionIsCalibrated(EEncoderId_t encoder);
void motionSetMaxPosition(EEncoderId_t encoder, int32_t counts);
int32_t motionGetMaxPosition(EEncoderId_t encoder);

#ifdef __cplusplus
}
#endif
#endif //!_MOTION_H_
