/**
 * @file motion.c
 * @brief Motion control implementation
 *
 * Homing flow: run head to top limit switch, back off from switch slightly, find Z index pulse. Use this as home.
 * Maximum top position is limit switch (and home after homing). Bottom position is software limit only, stored in EEPROM.
 * There is a bottom limit switch on the column but the column is too tall to reach it unless the darkroom is hugely tall, so it can be disabled when the user requests it.
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "motion.h"
#include "encoder.h"
#include "motor.h"
#include "switch.h"

#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define COUNTS_PER_REV      (400)  // 100 PPR * 4 (X4 mode)
#define DEFAULT_COUNTS_PER_MM (50.0f)
#define FLOAT_EPSILON       (1e-6f)  // Epsilon for float comparisons

//=====================================================================================================================
// Private Functions - Helpers
//=====================================================================================================================

/**
 * @brief Compare two floats with epsilon tolerance
 * @param a First float
 * @param b Second float
 * @return -1 if a < b, 0 if equal, 1 if a > b
 */
static inline int fltcmp(float a, float b)
{
    float diff = a - b;
    if (diff > FLOAT_EPSILON) return 1;
    if (diff < -FLOAT_EPSILON) return -1;
    return 0;
}

//=====================================================================================================================
// Private Data
//=====================================================================================================================

static SMotionCalibration_t sg_motionState = {
    .columnCountsPerMm = DEFAULT_COUNTS_PER_MM,
    .headCountsPerMm = DEFAULT_COUNTS_PER_MM,
    .columnMaxPosition = 0,
    .headMaxPosition = 0,
    .pulsesPerRevolutionColumn = 0,
    .pulsesPerRevolutionHead = 0,
    .crc32 = 0,
    .columnBottomLimitEnabled = false
};

static bool s_isCalibrated = false;
static int32_t s_calibrationStartColumn = 0;
static int32_t s_calibrationStartHead = 0;

//=====================================================================================================================
// Private Functions
//=====================================================================================================================

static void motionLimitSwitchCallback(void *);

//=====================================================================================================================
// Public Functions
//=====================================================================================================================

void motionInit(void)
{
    // Try to load calibration from EEPROM
    s_isCalibrated = motionLoadCalibration();
    
    // If no valid calibration, use defaults
    if (!s_isCalibrated)
    {
        sg_motionState.columnCountsPerMm = DEFAULT_COUNTS_PER_MM;
        sg_motionState.headCountsPerMm = DEFAULT_COUNTS_PER_MM;
    }

    bspSwitchAttachCallback(SWITCH_COLUMN_TOP_LIMIT, motionLimitSwitchCallback, (void *)(uintptr_t)SWITCH_COLUMN_TOP_LIMIT);
    bspSwitchAttachCallback(SWITCH_HEAD_TOP_LIMIT, motionLimitSwitchCallback, (void *)(uintptr_t)SWITCH_HEAD_TOP_LIMIT);
}

float motionGetPosition_mm(EEncoderId_t encoder)
{
    return (float)bspEncoderGetCount(encoder) / ((encoder == ENCODER_COLUMN) ? sg_motionState.columnCountsPerMm : sg_motionState.headCountsPerMm);
}

int32_t motionGetCount(EEncoderId_t encoder)
{
    return bspEncoderGetCount(encoder);
}

float motionGetVelocity_mmps(EEncoderId_t encoder)
{
    int32_t velocity_cpm = bspEncoderGetVelocity(encoder);
    return (float)velocity_cpm / 
           ((encoder == ENCODER_COLUMN) ? sg_motionState.columnCountsPerMm : sg_motionState.headCountsPerMm);
}

void motionStartCalibration(EEncoderId_t encoder)
{
    // Record starting positions
    if (encoder == ENCODER_COLUMN)
    {
        s_calibrationStartColumn = bspEncoderGetCount(ENCODER_COLUMN);
    }
    else if (encoder == ENCODER_HEAD)
    {
        s_calibrationStartHead = bspEncoderGetCount(ENCODER_HEAD);
    }
}

void motionCalibrate_mm(EEncoderId_t encoder, float distance_mm)
{
    int32_t currentCount = bspEncoderGetCount(encoder);
    int32_t deltaCounts = 0;

    if (encoder == ENCODER_COLUMN)
    {
        deltaCounts = currentCount - s_calibrationStartColumn;
    }
    else if (encoder == ENCODER_HEAD)
    {
        deltaCounts = currentCount - s_calibrationStartHead;
    }

    if (fltcmp(distance_mm, 0.1f) > 0)  // Sanity check
    {
        if (encoder == ENCODER_COLUMN)
        {
            sg_motionState.columnCountsPerMm = (float)deltaCounts / distance_mm;
        }
        else if (encoder == ENCODER_HEAD)
        {
            sg_motionState.headCountsPerMm = (float)deltaCounts / distance_mm;
        }
    }
}

void motionSaveCalibration(void)
{
    // Calculate CRC excluding the CRC field itself
    sg_motionState.crc32 = 0x0;
    
    // TODO: Write to EEPROM
    // For now, just mark as calibrated
    s_isCalibrated = true;
}

bool motionLoadCalibration(void)
{
    // TODO: Read from EEPROM
    // For now, return false (no valid calibration)
    
    // When implemented:
    // 1. Read calibration struct from EEPROM
    // 2. Calculate CRC
    // 3. Compare with stored CRC
    // 4. Return true if valid
    
    return false;
}

bool motionIsCalibrated(EEncoderId_t encoder)
{
    if (encoder == ENCODER_COLUMN)
    {
        return (fltcmp(sg_motionState.columnCountsPerMm, 0.0f) > 0 && 
                fltcmp(sg_motionState.columnCountsPerMm, DEFAULT_COUNTS_PER_MM) != 0);
    }
    else if (encoder == ENCODER_HEAD)
    {
        return (fltcmp(sg_motionState.headCountsPerMm, 0.0f) > 0 && 
                fltcmp(sg_motionState.headCountsPerMm, DEFAULT_COUNTS_PER_MM) != 0);
    }

    return false;
}

void motionSetMaxPosition(EEncoderId_t encoder, int32_t counts)
{
    if (encoder == ENCODER_COLUMN)
    {
        sg_motionState.columnMaxPosition = counts;
    }
    else if (encoder == ENCODER_HEAD)
    {
        sg_motionState.headMaxPosition = counts;
    }
}

int32_t motionGetMaxPosition(EEncoderId_t encoder)
{
    if (encoder == ENCODER_COLUMN)
    {
        return sg_motionState.columnMaxPosition;
    }
    else if (encoder == ENCODER_HEAD)
    {
        return sg_motionState.headMaxPosition;
    }

    return 0;
}


static void motionLimitSwitchCallback(void *userCtx)
{
    // This callback is called when a limit switch is triggered during motion
    // We can use this to stop the motor immediately and set the current position as the limit

    ESwitchId_t switch_id = (ESwitchId_t)(uintptr_t)userCtx;

    // Stop motor immediately
    if (switch_id == SWITCH_COLUMN_TOP_LIMIT)
    {
        bspMotorStop(MOTOR_COLUMN, MOTOR_BRAKE_ACTIVE);
    }
    else if (sg_motionState.columnBottomLimitEnabled && (switch_id == SWITCH_COLUMN_BOTTOM_LIMIT))
    {
        bspMotorStop(MOTOR_COLUMN, MOTOR_BRAKE_ACTIVE);
        sg_motionState.columnMaxPosition = bspEncoderGetCount(ENCODER_COLUMN);
    }
    else if (switch_id == SWITCH_HEAD_TOP_LIMIT)
    {
        bspMotorStop(MOTOR_HEAD, MOTOR_BRAKE_ACTIVE);
    }
    else if (switch_id == SWITCH_HEAD_BOTTOM_LIMIT)
    {
        bspMotorStop(MOTOR_HEAD, MOTOR_BRAKE_ACTIVE);
        sg_motionState.headMaxPosition = bspEncoderGetCount(ENCODER_HEAD);
    }
}