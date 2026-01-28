/**
 * @file optimize.h
 * @brief Memory Optimization Macros for CH592
 * 
 * CH592 Resources:
 * - Flash: 448KB (0x00000000 - 0x0006FFFF)
 * - RAM:   26KB  (0x20000000 - 0x200067FF)
 * 
 * Optimization strategies:
 * - Place constants in Flash using PROGMEM
 * - Use packed structures
 * - Minimize buffer sizes
 * - Use appropriate integer sizes
 */

#ifndef __OPTIMIZE_H__
#define __OPTIMIZE_H__

#include <stdint.h>

/*============================================================================
 * Compiler Attributes
 *============================================================================*/

// Place data in Flash (read-only)
#define PROGMEM             __attribute__((section(".rodata")))

// Force inline for critical paths
#define FORCE_INLINE        __attribute__((always_inline)) inline

// Prevent inline for size optimization
#define NO_INLINE           __attribute__((noinline))

// Pack structure to minimize padding
#define PACKED              __attribute__((packed))

// Align to specific boundary
#define ALIGNED(x)          __attribute__((aligned(x)))

// Place in specific section
#define SECTION(x)          __attribute__((section(x)))

// Weak symbol
#define WEAK                __attribute__((weak))

// Used (prevent optimizer from removing)
#define USED                __attribute__((used))

// Hot path (optimize for speed)
#define HOT                 __attribute__((hot))

// Cold path (optimize for size)
#define COLD                __attribute__((cold))

// Pure function (no side effects)
#define PURE                __attribute__((pure))

// Const function (only depends on arguments)
#define CONST_FUNC          __attribute__((const))

/*============================================================================
 * Size-Optimized Types
 *============================================================================*/

// Use smallest types possible
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;

// Fixed-point types for sensor data (saves RAM vs float)
typedef int16_t     q15_t;      // Q1.15 format: -1.0 to 0.99997
typedef int32_t     q31_t;      // Q1.31 format

// Conversion macros
#define FLOAT_TO_Q15(x)     ((q15_t)((x) * 32767.0f))
#define Q15_TO_FLOAT(x)     ((float)(x) / 32767.0f)
#define FLOAT_TO_Q31(x)     ((q31_t)((x) * 2147483647.0f))
#define Q31_TO_FLOAT(x)     ((float)(x) / 2147483647.0f)

/*============================================================================
 * Buffer Size Optimization
 *============================================================================*/

// Reduced buffer sizes for CH592
#define RF_TX_BUFFER_SIZE       48      // Was 64
#define RF_RX_BUFFER_SIZE       48      // Was 64
#define USB_TX_BUFFER_SIZE      256     // Was 512
#define USB_RX_BUFFER_SIZE      64      // Was 128

// Bootloader virtual disk (reduced)
#define VDISK_SECTOR_COUNT      4096    // 2MB instead of 8MB

/*============================================================================
 * Memory Pool (Static Allocation)
 *============================================================================*/

// Small fixed-size memory pool to avoid malloc
#define MEM_POOL_SIZE           512
#define MEM_BLOCK_SIZE          32
#define MEM_BLOCK_COUNT         (MEM_POOL_SIZE / MEM_BLOCK_SIZE)

/*============================================================================
 * String Optimization
 *============================================================================*/

// Place string in flash
#define PSTR(s)             (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))

// Flash string type
typedef const char* PGM_P;

/*============================================================================
 * Bit Field Macros
 *============================================================================*/

// Use bit fields to pack multiple booleans
#define BIT(n)              (1U << (n))
#define SET_BIT(x, n)       ((x) |= BIT(n))
#define CLR_BIT(x, n)       ((x) &= ~BIT(n))
#define GET_BIT(x, n)       (((x) >> (n)) & 1U)
#define TOGGLE_BIT(x, n)    ((x) ^= BIT(n))

/*============================================================================
 * Optimization Helpers
 *============================================================================*/

// Likely/unlikely for branch prediction
#define likely(x)           __builtin_expect(!!(x), 1)
#define unlikely(x)         __builtin_expect(!!(x), 0)

// Unreachable code marker
#define UNREACHABLE()       __builtin_unreachable()

// Count leading zeros
#define CLZ(x)              __builtin_clz(x)

// Count trailing zeros
#define CTZ(x)              __builtin_ctz(x)

// Population count (number of set bits)
#define POPCOUNT(x)         __builtin_popcount(x)

// Minimum/maximum without branches
#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)    MIN(MAX(x, lo), hi)

/*============================================================================
 * Debug Optimization
 *============================================================================*/

#ifdef NDEBUG
    #define ASSERT(x)       ((void)0)
    #define DEBUG_PRINT(...)    ((void)0)
#else
    #define ASSERT(x)       do { if (!(x)) { while(1); } } while(0)
    #define DEBUG_PRINT(...)    // Implement if needed
#endif

#endif /* __OPTIMIZE_H__ */
