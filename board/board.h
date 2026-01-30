/**
 * @file board.h
 * @brief Board Configuration Unified Entry Point
 * 
 * Automatically includes board-specific configuration based on BOARD macro
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/*============================================================================
 * Board Selection
 * 
 * Supported Boards:
 * - Generic Receiver: Generic receiver board (pins undefined, need configuration)
 * - Generic Board: Generic board (pins undefined, need configuration)
 * - CH591D: CH591D (20-pin QFN) Tracker
 * - CH592X: CH592X (28-pin QFN) Tracker/Receiver
 *============================================================================*/

/*============================================================================
 * Include Board-Specific Configuration
 *============================================================================*/

#if defined(BOARD_GENERIC_RECEIVER)
    #include "generic_receiver/config.h"
    #include "generic_receiver/pins.h"
    #define BOARD_NAME "Generic Receiver"
    #define BOARD_SERIES "generic_receiver"
#elif defined(BOARD_GENERIC_BOARD)
    #include "generic_board/config.h"
    #include "generic_board/pins.h"
    #define BOARD_NAME "Generic Board"
    #define BOARD_SERIES "generic_board"
#elif defined(BOARD_CH591D)
    #include "mingyue_slimevr/ch591d/config.h"
    #include "mingyue_slimevr/ch591d/pins.h"
    #define BOARD_NAME "CH591D"
    #define BOARD_SERIES "mingyue_slimevr"
#elif defined(BOARD_CH592X)
    #include "mingyue_slimevr/ch592x/config.h"
    #include "mingyue_slimevr/ch592x/pins.h"
    #define BOARD_NAME "CH592X"
    #define BOARD_SERIES "mingyue_slimevr"
#else
    #error "Board not defined! Please specify BOARD=generic_receiver, BOARD=generic_board, BOARD=ch591d or BOARD=ch592x in Makefile"
#endif

#endif /* __BOARD_H__ */

