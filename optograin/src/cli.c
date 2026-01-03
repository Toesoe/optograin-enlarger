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
    SEGGER_RTT_WriteString(0, "  help                   - Show this help\n");
    SEGGER_RTT_WriteString(0, "  enc                    - Get encoder positions and homing status\n");
    SEGGER_RTT_WriteString(0, "  motor head <speed>     - Set head motor speed (-1000 to 1000)\n");
    SEGGER_RTT_WriteString(0, "  motor column <speed>   - Set column motor speed (-1000 to 1000)\n");
    SEGGER_RTT_WriteString(0, "  motor stop             - Stop all motors\n");
    SEGGER_RTT_WriteString(0, "  lamp on                - Turn lamp on\n");
    SEGGER_RTT_WriteString(0, "  lamp off               - Turn lamp off\n");
    SEGGER_RTT_WriteString(0, "  lamp <ms>              - Timed exposure (milliseconds)\n");
    SEGGER_RTT_WriteString(0, "  home head              - Home head axis\n");
    SEGGER_RTT_WriteString(0, "  home column            - Home column axis\n");
    SEGGER_RTT_WriteString(0, "  cal ppr head           - Measure head encoder PPR\n");
    SEGGER_RTT_WriteString(0, "  cal ppr column         - Measure column encoder PPR\n");
    SEGGER_RTT_WriteString(0, "  cal head <mm>          - Calibrate head (move known distance)\n");
    SEGGER_RTT_WriteString(0, "  cal column <mm>        - Calibrate column (move known distance)\n");
    SEGGER_RTT_WriteString(0, "  cal status             - Show calibration status\n");
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
    else
    {
        SEGGER_RTT_WriteString(0, "Usage: motor <head|column> <speed> or motor stop\n");
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
            hwDelayMs(200);
            int32_t checkPos = bspEncoderGetCount(encoder);
            
            if (checkPos == startPos)
            {
                SEGGER_RTT_WriteString(0, "Motor stalled (no movement detected)\n");
                bspMotorStop(motor, MOTOR_BRAKE_COAST);
                continue;
            }
            
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
            SEGGER_RTT_printf(0, "Calibrating head axis with %.2f mm movement\n", distance);
            SEGGER_RTT_WriteString(0, "Move head axis exactly this distance, then press Enter\n");
            motionCalibrateHead_mm(distance);
            SEGGER_RTT_WriteString(0, "Head axis calibration complete\n");
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
            SEGGER_RTT_printf(0, "Calibrating column axis with %.2f mm movement\n", distance);
            SEGGER_RTT_WriteString(0, "Move column axis exactly this distance, then press Enter\n");
            motionCalibrateColumn_mm(distance);
            SEGGER_RTT_WriteString(0, "Column axis calibration complete\n");
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
