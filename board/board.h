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
 * - Mingyue CH591D: Mingyue CH591D (20-pin QFN) Tracker
 * - Mingyue CH592X: Mingyue CH592X (28-pin QFN) Tracker/Receiver
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
#elif defined(BOARD_MINGYUE_CH591D)
    #include "mingyue_slimevr/mingyue_ch591d/config.h"
    #include "mingyue_slimevr/mingyue_ch591d/pins.h"
    #define BOARD_NAME "Mingyue CH591D"
    #define BOARD_SERIES "mingyue_slimevr"
#elif defined(BOARD_MINGYUE_CH592X)
    #include "mingyue_slimevr/mingyue_ch592x/config.h"
    #include "mingyue_slimevr/mingyue_ch592x/pins.h"
    #define BOARD_NAME "Mingyue CH592X"
    #define BOARD_SERIES "mingyue_slimevr"
#elif defined(BOARD_CH591D)
    // Backward compatibility: ch591d -> mingyue_ch591d
    #include "mingyue_slimevr/mingyue_ch591d/config.h"
    #include "mingyue_slimevr/mingyue_ch591d/pins.h"
    #define BOARD_NAME "Mingyue CH591D"
    #define BOARD_SERIES "mingyue_slimevr"
#elif defined(BOARD_CH592X)
    // Backward compatibility: ch592x -> mingyue_ch592x
    #include "mingyue_slimevr/mingyue_ch592x/config.h"
    #include "mingyue_slimevr/mingyue_ch592x/pins.h"
    #define BOARD_NAME "Mingyue CH592X"
    #define BOARD_SERIES "mingyue_slimevr"
#else
    #error "Board not defined! Please specify BOARD=generic_receiver, BOARD=generic_board, BOARD=mingyue_ch591d or BOARD=mingyue_ch592x in Makefile"
#endif

#endif /* __BOARD_H__ */

