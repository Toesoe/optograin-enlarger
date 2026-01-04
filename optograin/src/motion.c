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
#include "board.h"

#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define COUNTS_PER_REV      (400)  // 100 PPR * 4 (X4 mode)

// Default calibration values (conservative estimates)
#define DEFAULT_COLUMN_COUNTS_PER_MM    (40.0f)
#define DEFAULT_HEAD_COUNTS_PER_MM      (80.0f)

//=====================================================================================================================
// Private Data
//=====================================================================================================================

static SMotionCalibration_t s_calibration = {
    .columnCountsPerMm = DEFAULT_COLUMN_COUNTS_PER_MM,
    .headCountsPerMm = 50.0f,
    .columnMaxPosition = 0,
    .headMaxPosition = 0,
    .pulsesPerRevolutionColumn = 0,
    .pulsesPerRevolutionHead = 0,
    .crc32 = 0,
};

static bool s_isCalibrated = false;
static int32_t s_calibrationStartColumn = 0;
static int32_t s_calibrationStartHead = 0;

//=====================================================================================================================
// Private Functions
//=====================================================================================================================

static uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

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
        s_calibration.columnCountsPerMm = DEFAULT_COLUMN_COUNTS_PER_MM;
        s_calibration.headCountsPerMm = DEFAULT_HEAD_COUNTS_PER_MM;
    }
}

float motionGetColumnPosition_mm(void)
{
    int32_t counts = bspEncoderGetCount(ENCODER_COLUMN);
    return (float)counts / s_calibration.columnCountsPerMm;
}

float motionGetHeadPosition_mm(void)
{
    int32_t counts = bspEncoderGetCount(ENCODER_HEAD);
    
    extern void SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...);
    SEGGER_RTT_printf(0, "DEBUG: counts=%d, countsPerMm=%f\n", counts, (double)s_calibration.headCountsPerMm);
    
    return (float)counts / s_calibration.headCountsPerMm;
}

int32_t motionGetColumnCount(void)
{
    return bspEncoderGetCount(ENCODER_COLUMN);
}

int32_t motionGetHeadCount(void)
{
    return bspEncoderGetCount(ENCODER_HEAD);
}

float motionGetColumnVelocity_mmps(void)
{
    int32_t velocity_cpm = bspEncoderGetVelocity(ENCODER_COLUMN);
    return (float)velocity_cpm / s_calibration.columnCountsPerMm;
}

float motionGetHeadVelocity_mmps(void)
{
    int32_t velocity_cpm = bspEncoderGetVelocity(ENCODER_HEAD);
    return (float)velocity_cpm / s_calibration.headCountsPerMm;
}

void motionStartCalibration(void)
{
    // Record starting positions
    s_calibrationStartColumn = bspEncoderGetCount(ENCODER_COLUMN);
    s_calibrationStartHead = bspEncoderGetCount(ENCODER_HEAD);
}

void motionSetCalibrationStartHead(int32_t count)
{
    s_calibrationStartHead = count;
}

void motionSetCalibrationStartColumn(int32_t count)
{
    s_calibrationStartColumn = count;
}

void motionCalibrateColumn_mm(float distance_mm)
{
    int32_t currentCount = bspEncoderGetCount(ENCODER_COLUMN);
    int32_t deltaCounts = currentCount - s_calibrationStartColumn;
    
    if (distance_mm > 0.1f)  // Sanity check
    {
        s_calibration.columnCountsPerMm = (float)deltaCounts / distance_mm;
    }
}

void motionCalibrateHead_mm(float distance_mm)
{
    int32_t currentCount = bspEncoderGetCount(ENCODER_HEAD);
    int32_t deltaCounts = currentCount - s_calibrationStartHead;
    
    extern void SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...);
    SEGGER_RTT_printf(0, "DEBUG: current=%d, start=%d, delta=%d, dist=%f\n", 
                      currentCount, s_calibrationStartHead, deltaCounts, (double)distance_mm);
    
    if (distance_mm > 0.1f)  // Sanity check
    {
        s_calibration.headCountsPerMm = (float)deltaCounts / distance_mm;
        SEGGER_RTT_printf(0, "DEBUG: Set headCountsPerMm = %f\n", (double)s_calibration.headCountsPerMm);
    }
}

void motionSaveCalibration(void)
{
    // Calculate CRC excluding the CRC field itself
    s_calibration.crc32 = calculateCRC32(
        (uint8_t *)&s_calibration, 
        sizeof(SMotionCalibration_t) - sizeof(uint32_t)
    );
    
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

bool motionIsCalibrated(void)
{
    return s_isCalibrated;
}

bool motionIsHeadCalibrated(void)
{
    // Head is always calibrated with fixed 50 counts/mm
    return true;
}

bool motionIsColumnCalibrated(void)
{
    // Check if column has non-default calibration
    return (s_calibration.columnCountsPerMm > 0.0f && 
            s_calibration.columnCountsPerMm != DEFAULT_COLUMN_COUNTS_PER_MM);
}

float motionGetHeadCountsPerMm(void)
{
    return s_calibration.headCountsPerMm;
}

float motionGetColumnCountsPerMm(void)
{
    return s_calibration.columnCountsPerMm;
}

void motionSetColumnMaxPosition(int32_t counts)
{
    s_calibration.columnMaxPosition = counts;
}

void motionSetHeadMaxPosition(int32_t counts)
{
    s_calibration.headMaxPosition = counts;
}

int32_t motionGetColumnMaxPosition(void)
{
    return s_calibration.columnMaxPosition;
}

int32_t motionGetHeadMaxPosition(void)
{
    return s_calibration.headMaxPosition;
}