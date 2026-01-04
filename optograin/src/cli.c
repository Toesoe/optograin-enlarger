/**
 * @file cli.c
 * @brief Command-line interface over RTT for hardware testing
 * 
 * Commands:
 *   help                    - Show available commands
 *   enc                     - Get encoder positions
 *   motor head <speed>      - Set head motor speed (-1000 to 1000)
 *   motor column <speed>    - Set column motor speed (-1000 to 1000)
 *   motor stop              - Stop all motors
 *   lamp on                 - Turn lamp on
 *   lamp off                - Turn lamp off
 *   lamp <ms>               - Timed exposure in milliseconds
 *   home head               - Home head axis
 *   home column             - Home column axis
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "cli.h"
#include "encoder.h"
#include "motor.h"
#include "lamp.h"
#include "motion.h"

#include <SEGGER_RTT.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define CLI_MAX_CMD_LEN 64
#define CLI_PROMPT "> "

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    char buffer[CLI_MAX_CMD_LEN];
    uint32_t pos;
    bool ready;
} SCliState_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SCliState_t gs_cli = { 0 };

//=====================================================================================================================
// Prototypes
//=====================================================================================================================

static void cliShowHelp(void);
static void cliProcessCommand(const char *cmd);
static void cliHandleEncoders(void);
static void cliHandleMotor(const char *args);
static void cliHandleLamp(const char *args);
static void cliHandleHome(const char *args);
static void cliHandleCalibrate(const char *args);
static void cliHandleMove(const char *args);
static void cliCalibrateAxis(EEncoderId_t encoder, EMotorId_t motor, const char *axisName);
static void exposureCallback(void *ctx);
static float parseFloat(const char *str);

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief Parse a float from a string (replacement for atof which doesn't work with newlib-nano)
 * @param str String to parse
 * @return Parsed float value
 */
static float parseFloat(const char *str)
{
    float result = 0.0f;
    float sign = 1.0f;
    bool hasDecimal = false;
    float divisor = 1.0f;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t') str++;
    
    // Check for sign
    if (*str == '-')
    {
        sign = -1.0f;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }
    
    // Parse digits
    while (*str)
    {
        if (*str >= '0' && *str <= '9')
        {
            if (hasDecimal)
            {
                divisor *= 10.0f;
                result = result + (*str - '0') / divisor;
            }
            else
            {
                result = result * 10.0f + (*str - '0');
            }
        }
        else if (*str == '.' && !hasDecimal)
        {
            hasDecimal = true;
        }
        else
        {
            break;
        }
        str++;
    }
    
    return result * sign;
}

void cliInit(void)
{
    SEGGER_RTT_WriteString(0, "\n\n");
    SEGGER_RTT_WriteString(0, "====================================\n");
    SEGGER_RTT_WriteString(0, "  OptoGrain Enlarger Test CLI\n");
    SEGGER_RTT_WriteString(0, "====================================\n");
    SEGGER_RTT_WriteString(0, "Type 'help' for available commands\n\n");
    SEGGER_RTT_WriteString(0, CLI_PROMPT);
    
    gs_cli.pos = 0;
    gs_cli.ready = false;
}

void cliProcess(void)
{
    char c;
    unsigned int numRead = SEGGER_RTT_Read(0, &c, 1);
    
    if (numRead <= 0)
    {
        return;
    }
    
    // Echo character
    SEGGER_RTT_Write(0, &c, 1);
    
    // Handle special characters
    if (c == '\r' || c == '\n')
    {
        SEGGER_RTT_WriteString(0, "\n");
        
        if (gs_cli.pos > 0)
        {
            gs_cli.buffer[gs_cli.pos] = '\0';
            cliProcessCommand(gs_cli.buffer);
            gs_cli.pos = 0;
        }
        
        SEGGER_RTT_WriteString(0, CLI_PROMPT);
    }
    else if (c == '\b' || c == 127)  // Backspace or DEL
    {
        if (gs_cli.pos > 0)
        {
            gs_cli.pos--;
            SEGGER_RTT_WriteString(0, " \b");  // Erase character
        }
    }
    else if (c >= 32 && c < 127)  // Printable characters
    {
        if (gs_cli.pos < CLI_MAX_CMD_LEN - 1)
        {
            gs_cli.buffer[gs_cli.pos++] = c;
        }
    }
}

//=====================================================================================================================
// Command Processing
//=====================================================================================================================

static void cliProcessCommand(const char *cmd)
{
    // Skip leading whitespace
    while (*cmd == ' ' || *cmd == '\t')
    {
        cmd++;
    }
    
    if (*cmd == '\0')
    {
        return;
    }
    
    // Parse command
    if (strcmp(cmd, "help") == 0)
    {
        cliShowHelp();
    }
    else if (strcmp(cmd, "enc") == 0)
    {
        cliHandleEncoders();
    }
    else if (strncmp(cmd, "motor ", 6) == 0)
    {
        cliHandleMotor(cmd + 6);
    }
    else if (strncmp(cmd, "lamp ", 5) == 0)
    {
        cliHandleLamp(cmd + 5);
    }
    else if (strcmp(cmd, "lamp on") == 0)
    {
        cliHandleLamp("on");
    }
    else if (strcmp(cmd, "lamp off") == 0)
    {
        cliHandleLamp("off");
    }
    else if (strncmp(cmd, "home ", 5) == 0)
    {
        cliHandleHome(cmd + 5);
    }
    else if (strncmp(cmd, "cal ", 4) == 0)
    {
        cliHandleCalibrate(cmd + 4);
    }
    else if (strncmp(cmd, "move ", 5) == 0)
    {
        cliHandleMove(cmd + 5);
    }
    else
    {
        SEGGER_RTT_printf(0, "Unknown command: %s\n", cmd);
        SEGGER_RTT_WriteString(0, "Type 'help' for available commands\n");
    }
}

static void cliShowHelp(void)
{
    SEGGER_RTT_WriteString(0, "\nAvailable commands:\n");
    SEGGER_RTT_WriteString(0, "  help                      - Show this help\n");
    SEGGER_RTT_WriteString(0, "  enc                       - Get encoder positions (counts and mm)\n");
    SEGGER_RTT_WriteString(0, "  motor head <speed>        - Set head motor speed (-1000 to 1000)\n");
    SEGGER_RTT_WriteString(0, "  motor column <speed>      - Set column motor speed (-1000 to 1000)\n");
    SEGGER_RTT_WriteString(0, "  motor stop                - Stop all motors\n");
    SEGGER_RTT_WriteString(0, "  motor test <head|column>  - Find minimum duty cycle\n");
    SEGGER_RTT_WriteString(0, "  move head <mm>            - Move head to relative position (mm)\n");
    SEGGER_RTT_WriteString(0, "  move column <mm>          - Move column to relative position (mm)\n");
    SEGGER_RTT_WriteString(0, "  lamp on                   - Turn lamp on\n");
    SEGGER_RTT_WriteString(0, "  lamp off                  - Turn lamp off\n");
    SEGGER_RTT_WriteString(0, "  lamp <ms>                 - Timed exposure (milliseconds)\n");
    SEGGER_RTT_WriteString(0, "  home head                 - Home head axis\n");
    SEGGER_RTT_WriteString(0, "  home column               - Home column axis\n");
    SEGGER_RTT_WriteString(0, "  cal head                  - Calibrate head (interactive Z-pulse method)\n");
    SEGGER_RTT_WriteString(0, "  cal column                - Calibrate column (interactive Z-pulse method)\n");
    SEGGER_RTT_WriteString(0, "  cal status                - Show calibration status\n");
    SEGGER_RTT_WriteString(0, "\n");
}

static void cliHandleEncoders(void)
{
    int32_t headPos = bspEncoderGetCount(ENCODER_HEAD);
    int32_t columnPos = bspEncoderGetCount(ENCODER_COLUMN);
    bool headZRef = bspEncoderHasZIndexReference(ENCODER_HEAD);
    bool columnZRef = bspEncoderHasZIndexReference(ENCODER_COLUMN);
    
    SEGGER_RTT_printf(0, "Head:   %6d counts %s\n", headPos, headZRef ? "[Z-REF]" : "");
    SEGGER_RTT_printf(0, "Column: %6d counts %s\n", columnPos, columnZRef ? "[Z-REF]" : "");
    
    // Show position in mm for calibrated axes
    bool headCalibrated = motionIsHeadCalibrated();
    bool columnCalibrated = motionIsColumnCalibrated();
    
    if (headCalibrated || columnCalibrated)
    {
        SEGGER_RTT_WriteString(0, "\nCalibrated positions:\n");
        
        if (headCalibrated)
        {
            float headMm = motionGetHeadPosition_mm();
            SEGGER_RTT_printf(0, "Head:   %.2f mm\n", headMm);
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Head:   (not calibrated)\n");
        }
        
        if (columnCalibrated)
        {
            float columnMm = motionGetColumnPosition_mm();
            SEGGER_RTT_printf(0, "Column: %.2f mm\n", columnMm);
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Column: (not calibrated)\n");
        }
    }
    else
    {
        SEGGER_RTT_WriteString(0, "\n(Not calibrated - run 'cal head <mm>' and 'cal column <mm>')\n");
    }
}

static void cliHandleMotor(const char *args)
{
    if (strncmp(args, "head ", 5) == 0)
    {
        int32_t speed = atoi(args + 5);
        if (speed < -1000 || speed > 1000)
        {
            SEGGER_RTT_WriteString(0, "Error: Speed must be -1000 to 1000\n");
            return;
        }
        bspMotorSetSpeed(MOTOR_HEAD, (int16_t)speed);
        SEGGER_RTT_printf(0, "Head motor set to %d\n", speed);
    }
    else if (strncmp(args, "column ", 7) == 0)
    {
        int32_t speed = atoi(args + 7);
        if (speed < -1000 || speed > 1000)
        {
            SEGGER_RTT_WriteString(0, "Error: Speed must be -1000 to 1000\n");
            return;
        }
        bspMotorSetSpeed(MOTOR_COLUMN, (int16_t)speed);
        SEGGER_RTT_printf(0, "Column motor set to %d\n", speed);
    }
    else if (strcmp(args, "stop") == 0)
    {
        bspMotorStop(MOTOR_HEAD, MOTOR_BRAKE_COAST);
        bspMotorStop(MOTOR_COLUMN, MOTOR_BRAKE_COAST);
        SEGGER_RTT_WriteString(0, "All motors stopped\n");
    }
    else if (strncmp(args, "test ", 5) == 0)
    {
        // Test minimum duty cycle: motor test <head|column>
        const char *motorName = args + 5;
        EMotorId_t motor;
        uint32_t ch1;
        
        if (strcmp(motorName, "head") == 0)
        {
            motor = MOTOR_HEAD;
            ch1 = LL_TIM_CHANNEL_CH3;
        }
        else if (strcmp(motorName, "column") == 0)
        {
            motor = MOTOR_COLUMN;
            ch1 = LL_TIM_CHANNEL_CH1;
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Usage: motor test <head|column>\n");
            return;
        }
        
        SEGGER_RTT_printf(0, "\nTesting minimum duty cycle for %s motor\n", motorName);
        SEGGER_RTT_WriteString(0, "Starting at 5%% duty, incrementing by 1%% every 500ms\n");
        SEGGER_RTT_WriteString(0, "Watch for when motor starts moving, then press any key\n\n");
        
        extern void hwDelayMs(uint32_t ms);
        
        // Test from 5% to 60% duty
        for (uint8_t duty = 50; duty <= 100; duty++)
        {
            // Calculate raw PWM duty (bypass normal speed scaling)
            uint32_t arr = LL_TIM_GetAutoReload(TIM1);
            uint32_t dutyValue = (arr * duty) / 100;
            
            // Set PWM directly (forward direction)
            switch (ch1)
            {
                case LL_TIM_CHANNEL_CH1:
                    LL_TIM_OC_SetCompareCH1(TIM1, dutyValue);
                    LL_TIM_OC_SetCompareCH2(TIM1, 0);
                    break;
                case LL_TIM_CHANNEL_CH3:
                    LL_TIM_OC_SetCompareCH3(TIM1, dutyValue);
                    LL_TIM_OC_SetCompareCH4(TIM1, 0);
                    break;
            }
            
            SEGGER_RTT_printf(0, "Duty: %d%%\r", duty);
            hwDelayMs(500);
            
            // Check if user pressed a key
            char c;
            if (SEGGER_RTT_Read(0, &c, 1) > 0)
            {
                SEGGER_RTT_printf(0, "\nMinimum duty cycle found: ~%d%%\n", duty);
                SEGGER_RTT_WriteString(0, "Update minDutyPercent in board.c to this value\n");
                break;
            }
        }
        
        bspMotorStop(motor, MOTOR_BRAKE_COAST);
        SEGGER_RTT_WriteString(0, "\nTest complete\n");
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: motor <head|column> <speed> or motor stop or motor test <head|column>\n");
    }
}

static void cliHandleLamp(const char *args)
{
    if (strcmp(args, "on") == 0)
    {
        bspLampSet(true);
        SEGGER_RTT_WriteString(0, "Lamp ON\n");
    }
    else if (strcmp(args, "off") == 0)
    {
        bspLampSet(false);
        SEGGER_RTT_WriteString(0, "Lamp OFF\n");
    }
    else
    {
        // Try to parse as exposure time
        int32_t time_ms = atoi(args);
        if (time_ms > 0 && time_ms <= 300000)  // Max 5 minutes
        {
            bspLampStartExposure((uint32_t)time_ms, exposureCallback, NULL);
            SEGGER_RTT_printf(0, "Starting %d ms exposure\n", time_ms);
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Usage: lamp <on|off|time_ms>\n");
        }
    }
}

static void cliHandleHome(const char *args)
{
    if (strcmp(args, "head") == 0)
    {
        SEGGER_RTT_WriteString(0, "Homing head axis...\n");
        bspEncoderReset(ENCODER_HEAD);
        SEGGER_RTT_WriteString(0, "Head axis homed (position reset to 0)\n");
    }
    else if (strcmp(args, "column") == 0)
    {
        SEGGER_RTT_WriteString(0, "Homing column axis...\n");
        bspEncoderReset(ENCODER_COLUMN);
        SEGGER_RTT_WriteString(0, "Column axis homed (position reset to 0)\n");
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: home <head|column>\n");
    }
}

static void exposureCallback(void *ctx)
{
    (void)ctx;
    SEGGER_RTT_WriteString(0, "\nExposure complete\n");
    SEGGER_RTT_WriteString(0, CLI_PROMPT);
}

static void cliHandleMove(const char *args)
{
    if (strncmp(args, "head ", 5) == 0)
    {
        if (!motionIsHeadCalibrated())
        {
            SEGGER_RTT_WriteString(0, "Error: Head axis not calibrated\n");
            return;
        }
        
        float distance = parseFloat(args + 5);
        
        if (distance < -500.0f || distance > 500.0f)
        {
            SEGGER_RTT_WriteString(0, "Error: Distance must be -500 to 500 mm\n");
            return;
        }
        
        int32_t startPos = bspEncoderGetCount(ENCODER_HEAD);
        float startMm = motionGetHeadPosition_mm();
        float targetMm = startMm + distance;
        
        // Calculate target counts
        float countsPerMm = motionGetHeadCountsPerMm();
        int32_t targetCounts = startPos + (int32_t)(distance * countsPerMm);
        
        SEGGER_RTT_printf(0, "Moving head %.2f mm (from %.2f to %.2f mm)\n", 
                          (double)distance, (double)startMm, (double)targetMm);
        
        // Simple proportional control
        int16_t speed = (distance > 0) ? 300 : -300;
        bspMotorSetSpeed(MOTOR_HEAD, speed);
        
        // Wait until target reached
        while (true)
        {
            int32_t currentPos = bspEncoderGetCount(ENCODER_HEAD);
            int32_t error = targetCounts - currentPos;
            
            if (abs(error) < 10) // Within 10 counts
            {
                break;
            }
            
            // Slow down as we approach target
            if (abs(error) < 100)
            {
                speed = (error > 0) ? 150 : -150;
                bspMotorSetSpeed(MOTOR_HEAD, speed);
            }
            
            hwDelayMs(10);
        }
        
        bspMotorStop(MOTOR_HEAD, MOTOR_BRAKE_ACTIVE);
        
        float finalMm = motionGetHeadPosition_mm();
        SEGGER_RTT_printf(0, "Move complete. Position: %.2f mm\n", (double)finalMm);
    }
    else if (strncmp(args, "column ", 7) == 0)
    {
        if (!motionIsColumnCalibrated())
        {
            SEGGER_RTT_WriteString(0, "Error: Column axis not calibrated\n");
            return;
        }
        
        float distance = parseFloat(args + 7);
        
        if (distance < -500.0f || distance > 500.0f)
        {
            SEGGER_RTT_WriteString(0, "Error: Distance must be -500 to 500 mm\n");
            return;
        }
        
        int32_t startPos = bspEncoderGetCount(ENCODER_COLUMN);
        float startMm = motionGetColumnPosition_mm();
        float targetMm = startMm + distance;
        
        // Calculate target counts
        float countsPerMm = motionGetColumnCountsPerMm();
        int32_t targetCounts = startPos + (int32_t)(distance * countsPerMm);
        
        SEGGER_RTT_printf(0, "Moving column %.2f mm (from %.2f to %.2f mm)\n", 
                          (double)distance, (double)startMm, (double)targetMm);
        
        // Simple proportional control
        int16_t speed = (distance > 0) ? 300 : -300;
        bspMotorSetSpeed(MOTOR_COLUMN, speed);
        
        // Wait until target reached
        while (true)
        {
            int32_t currentPos = bspEncoderGetCount(ENCODER_COLUMN);
            int32_t error = targetCounts - currentPos;
            
            if (abs(error) < 10) // Within 10 counts
            {
                break;
            }
            
            // Slow down as we approach target
            if (abs(error) < 100)
            {
                speed = (error > 0) ? 150 : -150;
                bspMotorSetSpeed(MOTOR_COLUMN, speed);
            }
            
            hwDelayMs(10);
        }
        
        bspMotorStop(MOTOR_COLUMN, MOTOR_BRAKE_ACTIVE);
        
        float finalMm = motionGetColumnPosition_mm();
        SEGGER_RTT_printf(0, "Move complete. Position: %.2f mm\n", (double)finalMm);
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: move <head|column> <mm>\n");
    }
}

static void cliHandleCalibrate(const char *args)
{
    if (strcmp(args, "status") == 0)
    {
        bool calibrated = motionIsCalibrated();
        SEGGER_RTT_printf(0, "Calibration status: %s\n", calibrated ? "CALIBRATED" : "NOT CALIBRATED");
        
        if (calibrated)
        {
            float headPos = motionGetHeadPosition_mm();
            float columnPos = motionGetColumnPosition_mm();
            SEGGER_RTT_printf(0, "Head position:   %.2f mm\n", headPos);
            SEGGER_RTT_printf(0, "Column position: %.2f mm\n", columnPos);
        }
    }
    else if (strcmp(args, "head") == 0)
    {
        cliCalibrateAxis(ENCODER_HEAD, MOTOR_HEAD, "head");
    }
    else if (strcmp(args, "column") == 0)
    {
        cliCalibrateAxis(ENCODER_COLUMN, MOTOR_COLUMN, "column");
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: cal <status|head|column>\n");
    }
}

static void cliCalibrateAxis(EEncoderId_t encoder, EMotorId_t motor, const char *axisName)
{
    SEGGER_RTT_printf(0, "\nCalibrating %s axis (100 PPR encoder)\n", axisName);
    SEGGER_RTT_WriteString(0, "This will move the motor through one encoder revolution\n");
    SEGGER_RTT_WriteString(0, "\n");
    
    int16_t speed = 150;
    
    // Try forward first, reverse if stalled
    for (int attempt = 0; attempt < 2; attempt++)
    {
        if (attempt == 1)
        {
            SEGGER_RTT_WriteString(0, "Stall detected, reversing direction...\n");
            speed = (int16_t)(-speed);
        }
        
        // Clear Z-index reference
        bspEncoderClearZIndexReference(encoder);
        
        // Disable auto-reset on Z pulse during calibration
        bspEncoderSetAutoResetOnZ(encoder, false);
        
        // Start motor
        SEGGER_RTT_printf(0, "Starting motor at speed %d...\n", speed);
        bspMotorSetSpeed(motor, speed);
        
        // Check for stall after 200ms
        int32_t startPos = bspEncoderGetCount(encoder);
        hwDelayMs(200);
        int32_t checkPos = bspEncoderGetCount(encoder);
        
        if (checkPos == startPos)
        {
            SEGGER_RTT_WriteString(0, "Motor stalled (no movement detected)\n");
            bspMotorStop(motor, MOTOR_BRAKE_COAST);
            if (attempt == 1)
            {
                SEGGER_RTT_WriteString(0, "Error: Motor stalled in both directions\n");
                return;
            }
            continue;
        }
        
        // Motor is moving - wait for first Z-index pulse
        SEGGER_RTT_WriteString(0, "Waiting for first Z-index pulse...\n");
        uint32_t timeout = 10000; // 10 second timeout
        while (!bspEncoderHasZIndexReference(encoder) && timeout-- > 0)
        {
            hwDelayMs(1);
        }
        
        if (!bspEncoderHasZIndexReference(encoder))
        {
            SEGGER_RTT_WriteString(0, "Error: Z-index timeout (check encoder wiring)\n");
            bspMotorStop(motor, MOTOR_BRAKE_COAST);
            return;
        }
        
        // Stop at first Z-index
        bspMotorStop(motor, MOTOR_BRAKE_ACTIVE);
        hwDelayMs(100);
        
        SEGGER_RTT_WriteString(0, "\nFirst Z-index found! Motor stopped.\n");
        SEGGER_RTT_WriteString(0, "Mark this position and press Enter to continue...\n");
        
        // Wait for user to press Enter
        while (true)
        {
            char c;
            if (SEGGER_RTT_Read(0, &c, 1) > 0 && (c == '\r' || c == '\n'))
            {
                break;
            }
            hwDelayMs(10);
        }
        
        // Record starting position and clear Z-index
        int32_t startCount = bspEncoderGetCount(encoder);
        bspEncoderClearZIndexReference(encoder);
        
        SEGGER_RTT_WriteString(0, "\nContinuing to next Z-index pulse...\n");
        
        // Start motor again
        bspMotorSetSpeed(motor, speed);
        
        // Wait for next Z-index (one full revolution)
        timeout = 20000; // 20 second timeout
        while (!bspEncoderHasZIndexReference(encoder) && timeout-- > 0)
        {
            hwDelayMs(1);
        }
        
        if (!bspEncoderHasZIndexReference(encoder))
        {
            SEGGER_RTT_WriteString(0, "Error: Z-index timeout\n");
            bspMotorStop(motor, MOTOR_BRAKE_COAST);
            return;
        }
        
        // Stop at second Z-index
        bspMotorStop(motor, MOTOR_BRAKE_ACTIVE);
        hwDelayMs(100);
        int32_t endCount = bspEncoderGetCount(encoder);
        
        int32_t countDiff = endCount - startCount;
        if (countDiff < 0) countDiff = -countDiff; // Absolute value
        
        SEGGER_RTT_WriteString(0, "\nSecond Z-index found! Motor stopped.\n");
        SEGGER_RTT_printf(0, "Encoder moved %d counts\n", countDiff);
        SEGGER_RTT_WriteString(0, "\nEnter the distance traveled in mm: ");
        
        // Read distance from user
        char distBuf[16] = {0};
        uint8_t distPos = 0;
        
        while (true)
        {
            char c;
            if (SEGGER_RTT_Read(0, &c, 1) > 0)
            {
                if (c == '\r' || c == '\n')
                {
                    SEGGER_RTT_WriteString(0, "\n");
                    break;
                }
                else if (c == '\b' || c == 127)
                {
                    if (distPos > 0)
                    {
                        distPos--;
                        distBuf[distPos] = 0;
                        SEGGER_RTT_WriteString(0, "\b \b");
                    }
                }
                else if ((c >= '0' && c <= '9') || c == '.')
                {
                    if (distPos < 15)
                    {
                        distBuf[distPos++] = c;
                        SEGGER_RTT_Write(0, &c, 1);
                    }
                }
            }
            hwDelayMs(10);
        }
        
        // Null terminate the buffer
        distBuf[distPos] = '\0';
        
        double distance_d = atof(distBuf);
        float distance = (float)distance_d;
        
        SEGGER_RTT_printf(0, "DEBUG: distBuf='%s' (len=%d), distance_d=%f, distance=%f\n", 
                          distBuf, (int)distPos, distance_d, (double)distance);
        
        if (distance > 0.0f && distance < 1000.0f)
        {
            float countsPerMm = (float)countDiff / distance;
            SEGGER_RTT_printf(0, "Calibration: %.2f counts/mm\n", countsPerMm);
            
            // Apply calibration - set start position then calculate
            if (encoder == ENCODER_HEAD)
            {
                motionSetCalibrationStartHead(startCount);
                motionCalibrateHead_mm(distance);
            }
            else
            {
                motionSetCalibrationStartColumn(startCount);
                motionCalibrateColumn_mm(distance);
            }
            
            SEGGER_RTT_WriteString(0, "Calibration complete!\n");
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Error: Invalid distance\n");
        }
        
        // Re-enable auto-reset for normal operation
        bspEncoderSetAutoResetOnZ(encoder, true);
        
        bspMotorStop(motor, MOTOR_BRAKE_COAST);
        return;
    }
}
