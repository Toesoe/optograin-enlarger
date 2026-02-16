/**
 * @file interface.c
 * @author Toesoe (thijs@nbtg.dev; github.com/toesoe)
 * @brief data structure and functions for communication between control box and logic PCB
 * @version 0.1
 * @date 15-02-2026
 *
 * @copyright Copyright (c) 2026
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "interface.h"

#include "lamp.h"
#include "motor.h"
#include "switch.h"

#include "optograin.pb.h"
#include "pb_common.h"
#include "pb_decode.h"
#include "pb_encode.h"

#include "uart.h"
#include "crc.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================
#define HEADER_BYTE_SIZE_BYTES (1)

#define PB_STATUS_MESSAGE_TRANSMISSION_SIZE (HEADER_BYTE_SIZE_BYTES + sizeof(Status) + CRC32_SIZE_BYTES)
#define PB_COMMAND_MESSAGE_TRANSMISSION_SIZE (HEADER_BYTE_SIZE_BYTES + sizeof(Command) + CRC32_SIZE_BYTES)

#define REPLY_MESSAGE_SIZE (6) // 1 byte header + 1 byte ACK/NACK + 4 bytes CRC

//=====================================================================================================================
// Public functions
//=====================================================================================================================

void pbBuildAndTransmitStatusMessage(void)
{
    uint8_t      buffer[PB_STATUS_MESSAGE_TRANSMISSION_SIZE] = { MESSAGE_HEADER_BYTE_PROTOBUF_DATABLOCK, 0 };

    pb_ostream_t stream  = pb_ostream_from_buffer(&buffer[1], sizeof(buffer) - 1);

    Status message = Status_init_zero;

    SFanStatus_t fanStatus = bspFanGetStatus();

    message.error_fan = fanStatus.hasError;
    message.fan_speed_rpm = fanStatus.fanSpeedRPM;

    message.lamp_on = bspLampGetState();
    message.exposure_remaining_ms = bspLampGetRemainingTime();

    message.lamp_temperature = 0; // TODO: read from sensor
    message.head_position_mm = 0; // TODO: read from position sensors
    message.lens_board_position_mm = 0; // TODO: read from position sensors
    message.positions_calibrated = false; // TODO: set to true after homing complete

    message.lens_index = bspSwitchGetCurrentLensIndex();
    
    message.error_overtemp = false; // TODO: set based on temperature sensor
    message.error_position = false; // TODO: set based on position sensor status

    if (!pb_encode(&stream, Status_fields, &message))
    {
        // Encoding failed, handle error
        // For now, we can just return without transmitting
        return;
    }

    // add CRC
    uint32_t crc = bspCalculateCRCData(buffer, sizeof(Status));
    memcpy(&buffer[sizeof(Status)], &crc, CRC32_SIZE_BYTES);

    // transmit!
    bspUartTransmitFrame(buffer, PB_STATUS_MESSAGE_TRANSMISSION_SIZE);
}

void pbDecodeAndHandleCommandMessage(const uint8_t *data, size_t size)
{
    // check CRC
    if ((size < PB_COMMAND_MESSAGE_TRANSMISSION_SIZE) || (data[0] != MESSAGE_HEADER_BYTE_PROTOBUF_DATABLOCK))
    {
        return; // invalid message
    }

    bool commandWasHandledSuccessfully = false;

    uint32_t receivedCrc;
    memcpy(&receivedCrc, &data[size - CRC32_SIZE_BYTES], CRC32_SIZE_BYTES);

    if (receivedCrc != bspCalculateCRCData(data, size - CRC32_SIZE_BYTES))
    {
        return; // CRC mismatch, invalid message
    }

    // decode protobuf
    Command message = Command_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data, size - CRC32_SIZE_BYTES);
    if (!pb_decode(&stream, Command_fields, &message))
    {
        return; // decoding failed
    }

    // handle command
    switch (message.type)
    {
        case CommandType_CMD_SET_LAMP_ON:
        {
            bspLampSet(true);
            break;
        }
        case CommandType_CMD_SET_LAMP_TIME:
        {
            if (message.has_lamp_duration_ms)
            {
                if (message.lamp_duration_ms > 0)
                {
                    bspLampStartExposure(message.lamp_duration_ms, NULL, NULL);
                    commandWasHandledSuccessfully = true;
                }
                else
                {
                    bspLampCancelExposure();
                }
            }
            break;
        }
        case CommandType_CMD_MOVE_HEAD:
        {
            if (message.has_head_position_mm)
            {
                
            }
            break;
        }
        case CommandType_CMD_MOVE_LENS_BOARD:
        {
            if (message.has_lens_board_position_mm)
            {
                
            }
            break;
        }
        case CommandType_CMD_STOP:
        {
            bspMotorEmergencyStop();
            commandWasHandledSuccessfully = true;
            break;
        }
        case CommandType_CMD_GET_STATUS:
        {
            pbBuildAndTransmitStatusMessage();
            commandWasHandledSuccessfully = true;
            break;
        }
        case CommandType_CMD_CALIBRATE:
        {
            break;
        }

        default:
            // unrecognized command type, ignore
            break;
    }

    if (message.type != CommandType_CMD_GET_STATUS)
    {
        // For non-status-request commands, send an ACK/NACK response
        uint8_t replyBuf[REPLY_MESSAGE_SIZE] = { MESSAGE_HEADER_BYTE_COMMAND_STATUS, commandWasHandledSuccessfully ? 0x01 : 0x00 };

        uint32_t crc = bspCalculateCRCData(replyBuf, REPLY_MESSAGE_SIZE - CRC32_SIZE_BYTES);
        memcpy(&replyBuf[REPLY_MESSAGE_SIZE - CRC32_SIZE_BYTES], &crc, CRC32_SIZE_BYTES);
        bspUartTransmitFrame(replyBuf, REPLY_MESSAGE_SIZE);
    }
}