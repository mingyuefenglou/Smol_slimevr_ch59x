# CH592 内存优化指南

## 硬件资源

| 资源 | 容量 | 说明 |
|------|------|------|
| Flash | 448KB | 代码 + 常量 |
| RAM | 26KB | 变量 + 栈 + 堆 |

## 优化后内存分配

### Flash 布局 (448KB)

```
0x00000000 ┌─────────────────────┐
           │    Bootloader       │ 4KB
0x00001000 ├─────────────────────┤
           │    Vector Table     │ 256B
           ├─────────────────────┤
           │    .init            │ ~64B
           ├─────────────────────┤
           │    .text (代码)     │ ~40KB (预估)
           ├─────────────────────┤
           │    .rodata (常量)   │ ~8KB
           ├─────────────────────┤
           │    .data init       │ ~1KB
           ├─────────────────────┤
           │      (空闲)         │ ~395KB
0x0006FFFF └─────────────────────┘
```

### RAM 布局 (26KB = 26624 bytes)

```
0x20000000 ┌─────────────────────┐
           │    .highcode        │ ~512B (从Flash复制)
           ├─────────────────────┤
           │    .data            │ ~256B
           ├─────────────────────┤
           │    .bss             │ 见下表
           ├─────────────────────┤
           │    Heap             │ 512B
           ├─────────────────────┤
           │    Stack            │ 1KB
0x200067FF └─────────────────────┘
```

## .bss 段详细分析

### Tracker 固件 RAM 使用

| 模块 | 变量 | 原始大小 | 优化后 | 节省 |
|------|------|----------|--------|------|
| **RF Transmitter** | | | | |
| rf_transmitter_opt_t | 上下文 | 80B | 48B | 32B |
| **Sensor Fusion** | | | | |
| vqf_opt_state_t | 状态 | 100B | 64B | 36B |
| **Bootloader** | | | | |
| bootloader_ctx_t | 上下文 | 1600B | 350B | 1250B |
| **Main** | | | | |
| quaternion[4] | float | 16B | 8B (Q15) | 8B |
| linear_accel[3] | float | 12B | 6B (s16) | 6B |
| gyro_bias[3] | float | 12B | 12B | 0B |
| btn_context x2 | 按键 | 40B | 24B | 16B |
| **HAL** | | | | |
| timer_state | 定时器 | 16B | 8B | 8B |
| **总计** | | ~1900B | ~520B | **~1380B** |

### Receiver 固件 RAM 使用

| 模块 | 变量 | 原始大小 | 优化后 | 节省 |
|------|------|----------|--------|------|
| **RF Receiver** | | | | |
| rf_receiver_opt_t | 上下文 | 500B | 280B | 220B |
| tracker_info x10 | 追踪器 | 480B | 240B | 240B |
| **USB Protocol** | | | | |
| tx_buffer | 发送 | 512B | 256B | 256B |
| rx_buffer | 接收 | 128B | 64B | 64B |
| **总计** | | ~1600B | ~840B | **~760B** |

## 优化技术

### 1. 数据类型优化

```c
// 原始: 使用 float (4 bytes each)
float quat[4];      // 16 bytes
float accel[3];     // 12 bytes

// 优化: 使用 Q15 定点数 (2 bytes each)
q15_t quat[4];      // 8 bytes
s16 accel[3];       // 6 bytes
```

### 2. 结构体打包

```c
// 原始: 编译器对齐填充
typedef struct {
    uint8_t  id;      // 1 byte
    uint32_t key;     // 4 bytes (需要3字节填充)
    uint16_t frame;   // 2 bytes
} packet_t;           // 实际占用 12 bytes

// 优化: 使用 packed 属性
typedef struct PACKED {
    uint8_t  id;      // 1 byte
    uint32_t key;     // 4 bytes
    uint16_t frame;   // 2 bytes
} packet_t;           // 实际占用 7 bytes
```

### 3. 位域使用

```c
// 原始: 多个 bool
bool connected;    // 1 byte
bool charging;     // 1 byte
bool calibrating;  // 1 byte
bool error;        // 1 byte
                   // 总计 4 bytes

// 优化: 位域
uint8_t flags;     // 1 byte
#define FLAG_CONNECTED    BIT(0)
#define FLAG_CHARGING     BIT(1)
#define FLAG_CALIBRATING  BIT(2)
#define FLAG_ERROR        BIT(3)
```

### 4. 常量放入 Flash

```c
// 原始: 数组在 RAM
static const uint8_t table[512];  // 占用 512B RAM

// 优化: 使用 PROGMEM
static const uint8_t PROGMEM table[512];  // 仅占用 Flash
```

### 5. 无表 CRC 计算

```c
// 原始: CRC 查找表
static const uint16_t crc_table[256];  // 512 bytes Flash

// 优化: 计算方式
static inline uint16_t crc16_update(uint16_t crc, uint8_t data) {
    crc ^= data;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}
```

### 6. 快速数学函数

```c
// 原始: 标准库 (带入大量代码)
#include <math.h>
float result = sqrtf(x);

// 优化: 快速近似
static inline float fast_invsqrt(float x) {
    union { float f; uint32_t i; } u = {x};
    u.i = 0x5f3759df - (u.i >> 1);
    u.f *= (1.5f - 0.5f * x * u.f * u.f);
    return u.f;
}
```

## 编译器优化选项

```makefile
# 尺寸优化
OPT = -Os

# 额外优化标志
SIZE_OPT = \
    -ffunction-sections \      # 每个函数单独段
    -fdata-sections \          # 每个变量单独段
    -fno-common \              # 禁止 common 块
    -fno-exceptions \          # 禁用 C++ 异常
    -fshort-enums \            # 最小枚举尺寸
    -flto                      # 链接时优化

# 链接器垃圾回收
LDFLAGS += -Wl,--gc-sections   # 删除未使用段
LDFLAGS += -Wl,--relax         # 指令松弛优化
```

## 内存检查

### 编译后检查

```bash
# 查看段大小
arm-none-eabi-size build/tracker/slimevr_tracker.elf

   text    data     bss     dec     hex filename
  42356     856    2048   45260    b0cc slimevr_tracker.elf

# 详细映射
arm-none-eabi-nm -S --size-sort build/tracker/slimevr_tracker.elf
```

### 运行时检查

```c
// 获取空闲 RAM
extern uint32_t _heap_end;
extern uint32_t _estack;

uint32_t get_free_ram(void) {
    uint32_t sp;
    __asm volatile ("mv %0, sp" : "=r" (sp));
    return sp - (uint32_t)&_heap_end;
}

// 栈水位检测
void check_stack_usage(void) {
    extern uint32_t _stack;
    uint32_t *p = &_stack;
    uint32_t used = 0;
    
    while (*p++ != 0xDEADBEEF) used++;
    printf("Stack used: %lu bytes\n", used * 4);
}
```

## 预期最终内存使用

| 固件 | Flash | RAM (.data + .bss) | 栈 | 堆 |
|------|-------|-------------------|-----|-----|
| Tracker | ~45KB | ~600B | 1KB | 512B |
| Receiver | ~50KB | ~900B | 1KB | 512B |

**剩余可用:**
- Flash: ~400KB (可添加更多功能)
- RAM: ~23KB (足够应对复杂场景)
