/**
 * @file version.h
 * @brief SlimeVR CH59X 固件版本管理
 * 
 * 版本号来源优先级:
 * 1. 编译时定义 (make VERSION=x.y.z)
 * 2. GitHub SlimeVR-Tracker-nRF-Receiver 最新版本
 * 3. 默认值 0.4.2
 */

#ifndef __VERSION_H__
#define __VERSION_H__

/*============================================================================
 * 版本定义
 *============================================================================*/

// 版本号 (可通过编译参数覆盖)
#ifndef FIRMWARE_VERSION_MAJOR
#define FIRMWARE_VERSION_MAJOR      0
#endif

#ifndef FIRMWARE_VERSION_MINOR
#define FIRMWARE_VERSION_MINOR      6
#endif

#ifndef FIRMWARE_VERSION_PATCH
#define FIRMWARE_VERSION_PATCH      2
#endif

// 版本字符串
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING     TOSTRING(FIRMWARE_VERSION_MAJOR) "." \
                                    TOSTRING(FIRMWARE_VERSION_MINOR) "." \
                                    TOSTRING(FIRMWARE_VERSION_PATCH)
#endif

// 固件名称
#define FIRMWARE_NAME               "SlimeVR-CH59X"

// 构建日期 (编译时自动生成)
#define BUILD_DATE                  __DATE__
#define BUILD_TIME                  __TIME__

/*============================================================================
 * 协议版本 (与 nRF 兼容)
 *============================================================================*/

// SlimeVR 协议版本
#define SLIMEVR_PROTOCOL_VERSION    7       // 与 SlimeVR Server 兼容

// RF 协议版本 (CH59X 私有)
#define RF_PROTOCOL_VERSION         1

// USB HID 版本
#define USB_HID_PROTOCOL_VERSION    1

/*============================================================================
 * 硬件版本
 *============================================================================*/

// 芯片类型
#ifdef CH592
#define CHIP_NAME                   "CH592"
#define CHIP_FLASH_KB               448
#define CHIP_RAM_KB                 26
#elif defined(CH591)
#define CHIP_NAME                   "CH591"
#define CHIP_FLASH_KB               192
#define CHIP_RAM_KB                 26
#else
#define CHIP_NAME                   "CH59X"
#define CHIP_FLASH_KB               192
#define CHIP_RAM_KB                 26
#endif

/*============================================================================
 * 兼容性标记
 *============================================================================*/

// 与 nRF 固件的兼容性
// 注意: CH59X 使用私有 RF 协议，与 nRF ESB 不兼容
// 但 USB HID 格式与 SlimeVR Server 兼容
#define NRF_COMPATIBLE_USB          1       // USB HID 兼容
#define NRF_COMPATIBLE_RF           0       // RF 不兼容 (私有协议)

/*============================================================================
 * 版本信息结构
 *============================================================================*/

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t protocol;
    const char *chip_name;
    const char *build_date;
} firmware_version_t;

/*============================================================================
 * 内联函数
 *============================================================================*/

static inline void get_firmware_version(firmware_version_t *ver)
{
    ver->major = FIRMWARE_VERSION_MAJOR;
    ver->minor = FIRMWARE_VERSION_MINOR;
    ver->patch = FIRMWARE_VERSION_PATCH;
    ver->protocol = SLIMEVR_PROTOCOL_VERSION;
    ver->chip_name = CHIP_NAME;
    ver->build_date = BUILD_DATE;
}

// 版本号打包为 32 位整数 (用于比较)
#define VERSION_CODE(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define CURRENT_VERSION_CODE \
    VERSION_CODE(FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH)

#endif /* __VERSION_H__ */
