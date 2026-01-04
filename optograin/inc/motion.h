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

// Position tracking
float motionGetColumnPosition_mm(void);
float motionGetHeadPosition_mm(void);
int32_t motionGetColumnCount(void);
int32_t motionGetHeadCount(void);

// Velocity
float motionGetColumnVelocity_mmps(void);
float motionGetHeadVelocity_mmps(void);

// Calibration
void motionStartCalibration(void);
void motionSetCalibrationStartHead(int32_t count);
void motionSetCalibrationStartColumn(int32_t count);
void motionCalibrateColumn_mm(float distance_mm);
void motionCalibrateHead_mm(float distance_mm);
void motionSaveCalibration(void);
bool motionLoadCalibration(void);
bool motionIsCalibrated(void);

// Position limits
void motionSetColumnMaxPosition(int32_t counts);
void motionSetHeadMaxPosition(int32_t counts);
int32_t motionGetColumnMaxPosition(void);
int32_t motionGetHeadMaxPosition(void);

#ifdef __cplusplus
}
#endif
#endif //!_MOTION_H_
