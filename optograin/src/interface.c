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

#include "pb_common.h"
#include "pb_decode.h"
#include "pb_encode.h"

#include "optograin.pb.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

//=====================================================================================================================
// Public functions
//=====================================================================================================================

int pbTest(void)
{
    uint8_t buffer[128];
    size_t message_length;
    bool status;
    
    /* Encode our message */
    {
        /* Allocate space on the stack to store the message data.
         *
         * Nanopb generates simple struct definitions for all the messages.
         * - check out the contents of simple.pb.h!
         * It is a good idea to always initialize your structures
         * so that you do not have garbage data from RAM in there.
         */
         
        Status message = Status_init_zero;
        
        /* Create a stream that will write to our buffer. */
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
        
        /* Fill in the lucky number */
        message.lens_index = 0;
        
        /* Now we are ready to encode the message! */
        status = pb_encode(&stream, Status_fields, &message);
        message_length = stream.bytes_written;
        
        /* Then just check for any errors.. */
        if (!status)
        {
            return 1;
        }
    }

    // transmit!
}