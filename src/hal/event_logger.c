/**
 * @file event_logger.c
 * @brief v0.6.0 事件环形缓冲区与崩溃快照实现
 */

#include "event_logger.h"
#include "hal.h"
#include "version.h"
#include "config.h"
#include "diagnostics.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * 常量
 *============================================================================*/

#define CRASH_MAGIC     0x43525348  // "CRSH"

/*============================================================================
 * 静态变量
 *============================================================================*/

// 环形缓冲区
static event_entry_t event_ring[EVENT_RING_SIZE];
static uint8_t event_head = 0;      // 下一个写入位置
static uint8_t event_count = 0;     // 当前事件数

// 启动时间
static uint32_t boot_time_ms = 0;

/*============================================================================
 * 内部函数
 *============================================================================*/

static uint16_t calc_crc16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/*============================================================================
 * 事件日志API
 *============================================================================*/

void event_logger_init(void)
{
    memset(event_ring, 0, sizeof(event_ring));
    event_head = 0;
    event_count = 0;
    boot_time_ms = hal_get_tick_ms();
    
    // 记录启动事件
    event_log_simple(EVT_BOOT);
}

void event_log(event_type_t type, const uint8_t *data, uint8_t data_len)
{
    event_entry_t *entry = &event_ring[event_head];
    
    entry->timestamp_ms = hal_get_tick_ms() - boot_time_ms;
    entry->type = type;
    entry->data_len = (data_len > EVENT_MAX_DATA_LEN) ? EVENT_MAX_DATA_LEN : data_len;
    
    if (data && entry->data_len > 0) {
        memcpy(entry->data, data, entry->data_len);
    } else {
        memset(entry->data, 0, EVENT_MAX_DATA_LEN);
    }
    
    // 移动写指针
    event_head = (event_head + 1) % EVENT_RING_SIZE;
    if (event_count < EVENT_RING_SIZE) {
        event_count++;
    }
}

void event_log_simple(event_type_t type)
{
    event_log(type, NULL, 0);
}

void event_log_u8(event_type_t type, uint8_t value)
{
    event_log(type, &value, 1);
}

void event_log_u32(event_type_t type, uint32_t value)
{
    event_log(type, (uint8_t *)&value, 4);
}

uint8_t event_get_recent(event_entry_t *events, uint8_t max_count)
{
    if (!events || max_count == 0) return 0;
    
    uint8_t copy_count = (event_count < max_count) ? event_count : max_count;
    
    // 从最新开始往回复制
    uint8_t read_pos = (event_head + EVENT_RING_SIZE - 1) % EVENT_RING_SIZE;
    
    for (uint8_t i = 0; i < copy_count; i++) {
        memcpy(&events[i], &event_ring[read_pos], sizeof(event_entry_t));
        read_pos = (read_pos + EVENT_RING_SIZE - 1) % EVENT_RING_SIZE;
    }
    
    return copy_count;
}

/*============================================================================
 * 崩溃快照API
 *============================================================================*/

void event_crash_snapshot_save(uint32_t pc, uint32_t sp, uint32_t ra, uint8_t crash_type)
{
    event_crash_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    
    snapshot.magic = CRASH_MAGIC;
    snapshot.timestamp_ms = hal_get_tick_ms();
    snapshot.pc = pc;
    snapshot.sp = sp;
    snapshot.ra = ra;
    snapshot.crash_type = crash_type;
    
    // 获取当前计数器
    snapshot.rf_timeout_count = g_receiver_stats.frame_overrun;
    snapshot.miss_sync_count = 0;  // 需要从rf_ctx获取
    snapshot.crc_fail_count = 0;
    
    // 计算CRC
    snapshot.crc = calc_crc16(&snapshot, sizeof(snapshot) - 2);
    
    // 写入Flash
    hal_storage_erase(EVENT_FLASH_OFFSET, sizeof(event_crash_snapshot_t));
    hal_storage_write(EVENT_FLASH_OFFSET, &snapshot, sizeof(event_crash_snapshot_t));
}

bool event_crash_snapshot_exists(void)
{
    uint32_t magic;
    hal_storage_read(EVENT_FLASH_OFFSET, &magic, 4);
    return (magic == CRASH_MAGIC);
}

int event_crash_snapshot_read(event_crash_snapshot_t *snapshot)
{
    if (!snapshot) return -1;
    
    hal_storage_read(EVENT_FLASH_OFFSET, snapshot, sizeof(event_crash_snapshot_t));
    
    if (snapshot->magic != CRASH_MAGIC) {
        return -2;  // 无效
    }
    
    uint16_t crc = calc_crc16(snapshot, sizeof(event_crash_snapshot_t) - 2);
    if (crc != snapshot->crc) {
        return -3;  // CRC错误
    }
    
    return 0;
}


void event_crash_snapshot_clear(void)
{
    uint32_t zero = 0;
    hal_storage_write(EVENT_FLASH_OFFSET, &zero, 4);
}

/*============================================================================
 * 诊断报告生成
 *============================================================================*/

uint16_t diag_report_generate_json(char *buf, uint16_t buf_size)
{
    if (!buf || buf_size < 256) return 0;
    
    uint16_t pos = 0;
    
    // 开始JSON
    pos += snprintf(buf + pos, buf_size - pos, "{\n");
    
    // 版本信息
    pos += snprintf(buf + pos, buf_size - pos, 
        "  \"fw_version\": \"%d.%d.%d\",\n",
        FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    pos += snprintf(buf + pos, buf_size - pos,
        "  \"build_time\": \"%s %s\",\n", BUILD_DATE, BUILD_TIME);
    pos += snprintf(buf + pos, buf_size - pos,
        "  \"chip\": \"%s\",\n", CHIP_NAME);
    
    // 配置信息
    pos += snprintf(buf + pos, buf_size - pos,
        "  \"config\": {\n");
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"superframe_us\": %d,\n", RF_SUPERFRAME_US);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"slot_us\": %d,\n", RF_DATA_SLOT_US);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"max_trackers\": %d,\n", MAX_TRACKERS);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"auto_sleep_ms\": %d\n", AUTO_SLEEP_TIMEOUT_MS);
    pos += snprintf(buf + pos, buf_size - pos,
        "  },\n");
    
    // 计数器
    pos += snprintf(buf + pos, buf_size - pos,
        "  \"counters\": {\n");
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"uptime_sec\": %lu,\n", g_receiver_stats.uptime_sec);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"superframe_count\": %lu,\n", g_receiver_stats.superframe_count);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"frame_overrun\": %lu,\n", g_receiver_stats.frame_overrun);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"usb_tx_count\": %lu,\n", g_receiver_stats.usb_tx_count);
    pos += snprintf(buf + pos, buf_size - pos,
        "    \"usb_errors\": %lu\n", g_receiver_stats.usb_errors);
    pos += snprintf(buf + pos, buf_size - pos,
        "  },\n");
    
    // 最近事件
    pos += snprintf(buf + pos, buf_size - pos,
        "  \"recent_events\": [\n");
    
    event_entry_t events[10];
    uint8_t evt_count = event_get_recent(events, 10);
    
    for (uint8_t i = 0; i < evt_count; i++) {
        pos += snprintf(buf + pos, buf_size - pos,
            "    {\"ts\": %lu, \"type\": %d}%s\n",
            events[i].timestamp_ms, events[i].type,
            (i < evt_count - 1) ? "," : "");
    }
    
    pos += snprintf(buf + pos, buf_size - pos,
        "  ]\n");
    
    // 结束JSON
    pos += snprintf(buf + pos, buf_size - pos, "}\n");
    
    return pos;
}

uint16_t diag_report_generate_bin(uint8_t *buf, uint16_t buf_size)
{
    if (!buf || buf_size < sizeof(diag_report_header_t) + 10 * sizeof(event_entry_t)) {
        return 0;
    }
    
    uint16_t pos = 0;
    
    // 头部
    diag_report_header_t header;
    header.fw_major = FIRMWARE_VERSION_MAJOR;
    header.fw_minor = FIRMWARE_VERSION_MINOR;
    header.fw_patch = FIRMWARE_VERSION_PATCH;
    header.board_id = 0x59;  // CH59X
    header.imu_type = 0;     // 需要从imu_interface获取
    header.mag_type = 0;
    header.superframe_us = RF_SUPERFRAME_US;
    header.slot_us = RF_DATA_SLOT_US;
    header.max_trackers = MAX_TRACKERS;
    header.sleep_mode = 1;   // WOM enabled
    header.rf_timeout_count = g_receiver_stats.frame_overrun;
    header.miss_sync_count = 0;
    header.crc_fail_count = 0;
    header.sleep_count = 0;
    header.wake_count = 0;
    header.uptime_sec = g_receiver_stats.uptime_sec;
    header.event_count = event_count;
    
    memcpy(buf + pos, &header, sizeof(header));
    pos += sizeof(header);
    
    // 事件
    uint8_t evt_count = (event_count > 10) ? 10 : event_count;
    event_entry_t events[10];
    event_get_recent(events, evt_count);
    
    memcpy(buf + pos, events, evt_count * sizeof(event_entry_t));
    pos += evt_count * sizeof(event_entry_t);
    
    return pos;
}
