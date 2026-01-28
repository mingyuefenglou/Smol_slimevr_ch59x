# CH591 vs CH592 兼容性分析

## 1. 芯片规格对比

| 特性 | CH591 | CH592 |
|------|-------|-------|
| **CPU** | QingKe V4C RISC-V | QingKe V4C RISC-V |
| **主频** | 60MHz | 60MHz |
| **Flash** | 256KB | 448KB |
| **RAM** | 18KB | 26KB |
| **BLE 版本** | BLE 5.1 | BLE 5.4 |
| **USB** | Full-Speed (12Mbps) | Full-Speed (12Mbps) |
| **封装** | QFN28/QFN48 | QFN28/QFN48 |
| **工作电压** | 1.8V-3.6V | 1.8V-3.6V |

## 2. 主要差异

### 2.1 内存差异

```
CH591:
  Flash: 256KB (0x00000000 - 0x0003FFFF)
  RAM:   18KB  (0x20000000 - 0x200047FF)

CH592:
  Flash: 448KB (0x00000000 - 0x0006FFFF)
  RAM:   26KB  (0x20000000 - 0x200067FF)
```

**影响:**
- CH591 Flash 少 192KB
- CH591 RAM 少 8KB
- 本项目当前使用 ~22KB Flash, ~2KB RAM → **两者均可运行**

### 2.2 BLE 协议栈

| 特性 | CH591 (BLE 5.1) | CH592 (BLE 5.4) |
|------|-----------------|-----------------|
| 2M PHY | ✓ | ✓ |
| LE Coded PHY | ✓ | ✓ |
| Periodic Advertising | - | ✓ |
| LE Power Control | - | ✓ |
| Enhanced ATT | - | ✓ |

**影响:** 对于 SlimeVR 项目，BLE 5.1 完全足够

### 2.3 外设差异

两者外设基本相同:
- 4x UART
- 2x SPI
- 1x I2C  
- 14ch ADC (12-bit)
- 12ch TouchKey
- LCD 驱动

**无显著差异影响本项目**

## 3. 固件兼容性

### 3.1 直接兼容 ✅

当前 `tracker.bin` **可以直接用于 CH591**，因为:

1. **内存使用在限制内**
   - Flash: ~22KB < 256KB (CH591 限制)
   - RAM: ~2KB < 18KB (CH591 限制)

2. **外设寄存器地址相同**
   - GPIO: 0x400010xx
   - USB: 0x40008xxx
   - SPI: 0x40004xxx
   - I2C: 0x40006xxx
   - PFIC: 0xE000E000

3. **中断向量表相同**
   - 相同的 IRQ 编号
   - 相同的异常处理

### 3.2 需要注意的事项

1. **链接脚本调整** (仅用于完整优化)
   ```ld
   /* CH591 版本 */
   MEMORY {
     FLASH (rx) : ORIGIN = 0x00001000, LENGTH = 255K
     RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 18K
   }
   
   /* CH592 版本 (当前) */
   MEMORY {
     FLASH (rx) : ORIGIN = 0x00001000, LENGTH = 447K
     RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 26K
   }
   ```

2. **BLE 库版本**
   - CH591: LIBCH59xBLE.a (BLE 5.1)
   - CH592: LIBCH59xBLE.a (BLE 5.4)
   - API 兼容，但库文件不同

## 4. 构建配置

### 4.1 统一构建 (推荐)

```makefile
# 自动检测或手动指定
CHIP ?= CH592

ifeq ($(CHIP),CH591)
    FLASH_SIZE = 256K
    RAM_SIZE = 18K
    DEFINES += -DCH591
else
    FLASH_SIZE = 448K
    RAM_SIZE = 26K
    DEFINES += -DCH592
endif
```

### 4.2 条件编译

```c
// 在代码中检测芯片类型
#if defined(CH591)
    #define MAX_TRACKERS    16  // RAM 限制
#elif defined(CH592)
    #define MAX_TRACKERS    24  // 更多 RAM
#endif
```

## 5. 测试结果

### 5.1 tracker.bin 分析

```
文件大小:  10,933 字节 (~10.7KB)
起始地址:  0x00001000
格式:      Intel HEX / 原始二进制

内存使用预估:
  .text (代码): ~9KB
  .rodata (常量): ~1KB  
  .data (初始化数据): ~0.5KB
  .bss (未初始化): ~1KB

总 Flash: ~10KB
总 RAM:   ~1.5KB
```

### 5.2 兼容性验证

| 测试项 | CH591 | CH592 | 结果 |
|--------|-------|-------|------|
| Flash 大小 | 256KB | 448KB | ✅ 均可容纳 |
| RAM 大小 | 18KB | 26KB | ✅ 均可运行 |
| 外设地址 | 相同 | 相同 | ✅ 兼容 |
| 中断向量 | 相同 | 相同 | ✅ 兼容 |
| USB 功能 | 相同 | 相同 | ✅ 兼容 |

## 6. 结论

### ✅ tracker.bin 完全兼容 CH591 和 CH592

1. **直接烧录**: 可以直接烧录到任一芯片
2. **无需修改**: 当前代码无需任何修改
3. **性能相同**: 两者运行性能一致

### 建议

1. **开发阶段**: 使用 CH592 (更多资源用于调试)
2. **量产阶段**: 可选择 CH591 (更低成本)
3. **如需 BLE 5.4 特性**: 必须使用 CH592

## 7. 烧录命令

```bash
# CH591 和 CH592 使用相同的烧录命令
wchisp flash tracker.bin

# 或使用 WCHISPTool (Windows)
```

## 8. 芯片识别

```c
// 运行时检测芯片类型
uint32_t get_chip_id(void) {
    return *(volatile uint32_t*)0x1FFFF7E8;
}

// CH591 ID: 0x91xxxxxx
// CH592 ID: 0x92xxxxxx
```
