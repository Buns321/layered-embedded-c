/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************
*
*        SEGGER RTT * Real Time Transfer for embedded targets
*                  https://github.com/SEGGERMicro/RTT
*
**********************************************************************

---------------------------END-OF-HEADER------------------------------
Purpose : Project configuration for SEGGER RTT.

----------------------------------------------------------------------
*/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

/*
 * The project uses the C implementation so no RTT ARMv7-M assembly source
 * needs to be added to the build.
 */
#define RTT_USE_ASM                      0

/* Terminal output buffer, target -> host. */
#define BUFFER_SIZE_UP                   1024

/* Input is not used by system_log, but channel 0 must remain valid. */
#define BUFFER_SIZE_DOWN                 16
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS    1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS  1

/*
 * A missing debugger must never stall a task. Messages are either fully
 * accepted or fully dropped by the RTT backend.
 */
#define SEGGER_RTT_MODE_DEFAULT          SEGGER_RTT_MODE_NO_BLOCK_SKIP

#endif

/*************************** End of file ****************************/
