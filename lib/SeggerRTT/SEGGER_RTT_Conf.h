/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       SEGGER RTT * Real Time Transfer for embedded targets         *
*                                                                    *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : SEGGER_RTT_Conf.h
Purpose : Implementation of SEGGER real-time transfer (RTT) which
          allows real-time communication on targets which support
          debugger memory accesses while the CPU is running.
Revision: $Rev: 24316 $

*/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

//
// Take in and set to correct values for Cortex-A systems with CPU cache
//
//#define SEGGER_RTT_CPU_CACHE_LINE_SIZE            (32)          // Largest cache line size (in bytes) in the current system
//#define SEGGER_RTT_UNCACHED_OFF                   (0xFB000000)  // Address alias where RTT CB and buffers can be accessed uncached
//
// Most common settings for various Cortex-M processors
//
#if (defined __ARM7M__) || (defined __ARM_ARCH_7M__) || (defined __ARM_ARCH_7EM__) || (defined __ARM_ARCH_8M_MAIN__)
  #ifndef   SEGGER_RTT_MAX_INTERRUPT_PRIORITY
    #define SEGGER_RTT_MAX_INTERRUPT_PRIORITY         (0x20)   // Interrupt priority to lock on SEGGER RTT output routines on Cortex-M3/4/7 (Default: 0x20)
  #endif
#endif

//
// STM32F103 specific: Cortex-M3, 20KB RAM - use smaller buffers
//
#ifndef   BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP                              (512)    // Size of the buffer for terminal output of target, up to host (Default: 1k)
#endif

#ifndef   BUFFER_SIZE_DOWN
  #define BUFFER_SIZE_DOWN                            (16)     // Size of the buffer for terminal input to target from host (Usually keyboard input) (Default: 16)
#endif

#ifndef   SEGGER_RTT_MAX_NUM_UP_BUFFERS
  #define SEGGER_RTT_MAX_NUM_UP_BUFFERS               (2)      // Number of up-buffers (T->H) available on this target (Default: 3)
#endif

#ifndef   SEGGER_RTT_MAX_NUM_DOWN_BUFFERS
  #define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS             (2)      // Number of down-buffers (H->T) available on this target (Default: 3)
#endif

#ifndef   SEGGER_RTT_BUFFER_SECTION
  // #define SEGGER_RTT_BUFFER_SECTION                   ""       // Section used for RTT buffers
#endif

#ifndef   SEGGER_RTT_ALIGNMENT
  #define SEGGER_RTT_ALIGNMENT                        0        // Alignment of the SEGGER RTT buffer
#endif

#ifndef   SEGGER_RTT_BUFFER_ALIGNMENT
  #define SEGGER_RTT_BUFFER_ALIGNMENT                 0        // Alignment of SEGGER RTT buffer contents
#endif

#ifndef   SEGGER_RTT_MODE_DEFAULT
  #define SEGGER_RTT_MODE_DEFAULT                     SEGGER_RTT_MODE_NO_BLOCK_SKIP // Mode for pre-initialized terminal channel (buffer 0)
#endif

#ifndef   SEGGER_RTT_LOCK
  #define SEGGER_RTT_LOCK()                           // Lock RTT (nestable)   (i.e. disable interrupts)
#endif

#ifndef   SEGGER_RTT_UNLOCK
  #define SEGGER_RTT_UNLOCK()                         // Unlock RTT (nestable) (i.e. enable interrupts)
#endif

#ifndef   SEGGER_RTT_MEMCPY_USE_BYTELOOP
  #define SEGGER_RTT_MEMCPY_USE_BYTELOOP              0        // 0: Use memcpy/SEGGER_RTT_MEMCPY, 1: Use a simple byte-loop
#endif
//
// For some environments, NULL may not be defined until certain headers are included
//
#ifndef NULL
  #define NULL ((void *)0)
#endif

#ifdef __cplusplus
}
#endif

#endif

/*************************** End of file ****************************/
