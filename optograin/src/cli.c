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

typedef struct
{
    bool active;
    bool isHead;  // true = head, false = column
    float targetDistance;
    int32_t startCount;
} SCalibrationState_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SCliState_t gs_cli = { 0 };
static SCalibrationState_t gs_calibrationState = { 0 };

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
static void exposureCallback(void *ctx);

//=====================================================================================================================
// Functions
//=====================================================================================================================

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
    SEGGER_RTT_WriteString(0, "  lamp on                   - Turn lamp on\n");
    SEGGER_RTT_WriteString(0, "  lamp off                  - Turn lamp off\n");
    SEGGER_RTT_WriteString(0, "  lamp <ms>                 - Timed exposure (milliseconds)\n");
    SEGGER_RTT_WriteString(0, "  home head                 - Home head axis\n");
    SEGGER_RTT_WriteString(0, "  home column               - Home column axis\n");
    SEGGER_RTT_WriteString(0, "  cal ppr head              - Measure head encoder PPR\n");
    SEGGER_RTT_WriteString(0, "  cal ppr column            - Measure column encoder PPR\n");
    SEGGER_RTT_WriteString(0, "  cal head <mm>             - Calibrate head (call twice: start, then finish)\n");
    SEGGER_RTT_WriteString(0, "  cal column <mm>           - Calibrate column (call twice: start, then finish)\n");
    SEGGER_RTT_WriteString(0, "  cal status                - Show calibration status\n");
    SEGGER_RTT_WriteString(0, "\n");
}

static void cliHandleEncoders(void)
{
    int32_t headPos = bspEncoderGetCount(ENCODER_HEAD);
    int32_t columnPos = bspEncoderGetCount(ENCODER_COLUMN);
    bool headHomed = bspEncoderIsHomed(ENCODER_HEAD);
    bool columnHomed = bspEncoderIsHomed(ENCODER_COLUMN);
    
    SEGGER_RTT_printf(0, "Head:   %6d counts %s\n", headPos, headHomed ? "[HOMED]" : "[NOT HOMED]");
    SEGGER_RTT_printf(0, "Column: %6d counts %s\n", columnPos, columnHomed ? "[HOMED]" : "[NOT HOMED]");
    
    // Show position in mm if calibrated
    if (motionIsCalibrated())
    {
        float headMm = motionGetHeadPosition_mm();
        float columnMm = motionGetColumnPosition_mm();
        SEGGER_RTT_printf(0, "\nCalibrated positions:\n");
        SEGGER_RTT_printf(0, "Head:   %.2f mm\n", headMm);
        SEGGER_RTT_printf(0, "Column: %.2f mm\n", columnMm);
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
    else if (strncmp(args, "ppr ", 4) == 0)
    {
        const char *axis = args + 4;
        EEncoderId_t encoder;
        EMotorId_t motor;
        
        if (strcmp(axis, "head") == 0)
        {
            encoder = ENCODER_HEAD;
            motor = MOTOR_HEAD;
        }
        else if (strcmp(axis, "column") == 0)
        {
            encoder = ENCODER_COLUMN;
            motor = MOTOR_COLUMN;
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Usage: cal ppr <head|column>\n");
            return;
        }
        
        SEGGER_RTT_printf(0, "Measuring %s encoder PPR...\n", axis);
        
        int16_t speed = 150;
        bool success = false;
        
        for (int attempt = 0; attempt < 2 && !success; attempt++)
        {
            if (attempt == 1)
            {
                SEGGER_RTT_WriteString(0, "Stall detected, reversing direction...\n");
                speed = (int16_t)(-speed);
            }
            
            SEGGER_RTT_printf(0, "Starting motor at speed %d\n", speed);
            bspMotorSetSpeed(motor, speed);
            
            // Check for stall - wait 200ms and see if encoder moved
            int32_t startPos = bspEncoderGetCount(encoder);
            SEGGER_RTT_printf(0, "Initial encoder count: %d\n", startPos);
            hwDelayMs(200);
            int32_t checkPos = bspEncoderGetCount(encoder);
            SEGGER_RTT_printf(0, "After 200ms count: %d\n", checkPos);
            
            if (checkPos == startPos)
            {
                SEGGER_RTT_WriteString(0, "Motor stalled (no movement detected)\n");
                bspMotorStop(motor, MOTOR_BRAKE_COAST);
                continue;
            }
            
            SEGGER_RTT_printf(0, "Motor moving, measuring PPR...\n");
            
            // Motor is moving, measure PPR
            uint16_t ppr = (encoder == ENCODER_HEAD) ? motionMeasureHeadPPR() : motionMeasureColumnPPR();
            
            bspMotorStop(motor, MOTOR_BRAKE_COAST);
            
            SEGGER_RTT_printf(0, "%s encoder PPR: %d\n", axis, ppr);
            success = true;
        }
        
        if (!success)
        {
            SEGGER_RTT_WriteString(0, "Error: Motor stalled in both directions\n");
            SEGGER_RTT_WriteString(0, "Check for mechanical obstruction or limit switch\n");
        }
    }
    else if (strncmp(args, "head ", 5) == 0)
    {
        float distance = (float)atof(args + 5);
        if (distance > 0.0f && distance < 1000.0f)
        {
            if (!gs_calibrationState.active || !gs_calibrationState.isHead)
            {
                // First call - start calibration
                gs_calibrationState.active = true;
                gs_calibrationState.isHead = true;
                gs_calibrationState.targetDistance = distance;
                gs_calibrationState.startCount = motionGetHeadCount();
                
                SEGGER_RTT_printf(0, "Head calibration started: %.2f mm\n", distance);
                SEGGER_RTT_printf(0, "Starting count: %d\n", gs_calibrationState.startCount);
                SEGGER_RTT_WriteString(0, "Use 'motor head <speed>' to move the head exactly this distance\n");
                SEGGER_RTT_printf(0, "Then run 'cal head %.2f' again to complete calibration\n", distance);
            }
            else
            {
                // Second call - finish calibration
                int32_t endCount = motionGetHeadCount();
                int32_t deltaCount = endCount - gs_calibrationState.startCount;
                
                SEGGER_RTT_printf(0, "Ending count: %d\n", endCount);
                SEGGER_RTT_printf(0, "Delta counts: %d\n", deltaCount);
                
                // Set the calibration start to our saved value and calculate
                motionSetCalibrationStartHead(gs_calibrationState.startCount);
                motionCalibrateHead_mm(distance);
                
                float countsPerMm = (float)deltaCount / distance;
                SEGGER_RTT_printf(0, "Counts per mm: %.2f\n", countsPerMm);
                SEGGER_RTT_WriteString(0, "Head axis calibration complete\n");
                
                // Reset calibration state
                gs_calibrationState.active = false;
            }
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Error: Distance must be 0.01 to 999.99 mm\n");
        }
    }
    else if (strncmp(args, "column ", 7) == 0)
    {
        float distance = (float)atof(args + 7);
        if (distance > 0.0f && distance < 1000.0f)
        {
            if (!gs_calibrationState.active || gs_calibrationState.isHead)
            {
                // First call - start calibration
                gs_calibrationState.active = true;
                gs_calibrationState.isHead = false;
                gs_calibrationState.targetDistance = distance;
                gs_calibrationState.startCount = motionGetColumnCount();
                
                SEGGER_RTT_printf(0, "Column calibration started: %.2f mm\n", distance);
                SEGGER_RTT_printf(0, "Starting count: %d\n", gs_calibrationState.startCount);
                SEGGER_RTT_WriteString(0, "Use 'motor column <speed>' to move the column exactly this distance\n");
                SEGGER_RTT_printf(0, "Then run 'cal column %.2f' again to complete calibration\n", distance);
            }
            else
            {
                // Second call - finish calibration
                int32_t endCount = motionGetColumnCount();
                int32_t deltaCount = endCount - gs_calibrationState.startCount;
                
                SEGGER_RTT_printf(0, "Ending count: %d\n", endCount);
                SEGGER_RTT_printf(0, "Delta counts: %d\n", deltaCount);
                
                // Set the calibration start to our saved value and calculate
                motionSetCalibrationStartColumn(gs_calibrationState.startCount);
                motionCalibrateColumn_mm(distance);
                
                float countsPerMm = (float)deltaCount / distance;
                SEGGER_RTT_printf(0, "Counts per mm: %.2f\n", countsPerMm);
                SEGGER_RTT_WriteString(0, "Column axis calibration complete\n");
                
                // Reset calibration state
                gs_calibrationState.active = false;
            }
        }
        else
        {
            SEGGER_RTT_WriteString(0, "Error: Distance must be 0.01 to 999.99 mm\n");
        }
    }
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: cal <status|ppr|head|column> [args]\n");
    }
}
