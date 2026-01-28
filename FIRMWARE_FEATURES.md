# SlimeVR CH59X 固件功能汇总文档

## 文档信息
- **固件版本**: v0.4.2
- **编译日期**: 2026-01-28
- **MCU**: CH592 (RISC-V rv32imac_zicsr_zifencei)
- **架构**: 32位 RISC-V

---

## 一、Tracker 固件功能汇总

### 1.1 核心功能模块

#### 1.1.1 传感器处理系统
- **传感器采样频率**: 200Hz (5ms周期)
- **支持的IMU传感器**:
  - BMI270, BMI323
  - ICM42688, ICM45686
  - LSM6DSO, LSM6DSR, LSM6DSV
  - IIM42652
  - SC7I22
- **传感器读取模式**:
  - 标准模式: 直接I2C/SPI读取
  - 优化模式 (USE_SENSOR_OPTIMIZED): FIFO缓冲 + 后台处理
  - DMA模式 (USE_SENSOR_DMA): DMA直接内存访问 (待实现)

#### 1.1.2 传感器融合算法
支持多种融合算法，可通过 `FUSION_TYPE` 配置:
- **VQF Ultra** (FUSION_VQF_ULTRA): 超高性能融合
- **VQF Advanced** (FUSION_VQF_ADVANCED): 高级融合，默认算法
- **VQF Opt** (FUSION_VQF_OPT): 优化版本
- **VQF Simple** (FUSION_VQF_SIMPLE): 简化版本
- **EKF AHRS** (FUSION_EKF): 扩展卡尔曼滤波

**融合功能**:
- 6DOF融合: 陀螺仪 + 加速度计
- 9DOF融合: 陀螺仪 + 加速度计 + 磁力计 (可选)
- 四元数输出
- 线性加速度计算 (移除重力影响)

#### 1.1.3 传感器数据处理
- **陀螺仪滤波** (gyro_noise_filter):
  - 中值滤波
  - 移动平均滤波
  - 卡尔曼滤波 (可选)
  - 静止检测
  
- **温度补偿** (temp_compensation):
  - 陀螺仪温漂补偿
  - 温度估计和校正
  
- **自动校准** (auto_calibration):
  - 运动中自动校准陀螺仪偏移
  - 动态偏置估计

- **磁力计支持** (mag_interface):
  - 航向校正
  - 磁力计校准
  - 9DOF融合增强

#### 1.1.4 RF通信系统
- **RF协议**:
  - TDMA时隙分配
  - 同步信标接收
  - 数据包重传机制
  - RSSI信号强度监测
  
- **RF优化模块** (v0.6.2):
  - **智能信道管理** (channel_manager): CCA检测 + 自动避让 + 黑名单
  - **RF自愈机制** (rf_recovery): 通信中断自动恢复
  - **时隙优化器** (rf_slot_optimizer): 动态分配 + 优先级调度
  - **时序优化** (rf_timing_opt): 精确同步 + 延迟补偿
  - **RF Ultra模式** (rf_ultra): 极限性能模式

- **配对功能**:
  - 自动配对流程
  - 配对数据持久化存储
  - 网络密钥管理

#### 1.1.5 电源管理
- **功耗优化** (power_optimizer):
  - 动态时钟调整
  - 睡眠调度
  - 自动睡眠 (静止超时)
  
- **睡眠模式**:
  - 浅睡眠 (light sleep): 保持RAM状态
  - 深睡眠 (deep sleep): 最大功耗节省
  - 状态保持 (retained_state): 睡眠后状态恢复

- **电池管理**:
  - ADC电池电压监测
  - 充电状态检测
  - 低电量警告

#### 1.1.6 系统监控与诊断
- **看门狗** (watchdog):
  - 硬件看门狗 (2秒超时)
  - 任务监控 (软件层面)
  - 复位原因检测
  - HardFault处理
  
- **事件日志** (event_logger):
  - 系统事件记录
  - 故障快照保存
  
- **诊断统计** (diagnostics):
  - 性能监控
  - 故障排查
  - 统计信息收集

#### 1.1.7 USB功能
- **USB Bootloader** (usb_bootloader):
  - UF2格式固件更新
  - USB MSC模式
  - 拖放升级支持
  
- **USB调试接口** (usb_debug):
  - 实时数据流 (四元数、传感器数据)
  - 状态查询
  - 配置修改
  - 诊断命令

- **USB大容量存储** (usb_msc):
  - UF2固件更新
  - 文件系统访问

### 1.2 运行状态机

Tracker固件使用状态机管理运行流程:

1. **STATE_PAIRING** (配对状态)
   - 等待接收器配对请求
   - 发送配对响应
   - 保存配对信息

2. **STATE_SEARCH_SYNC** (搜索同步)
   - 搜索接收器同步信标
   - 等待同步建立

3. **STATE_RUNNING** (运行状态)
   - 正常数据发送
   - 传感器采样和融合
   - RF通信

4. **STATE_CALIBRATING** (校准状态)
   - 陀螺仪偏置校准
   - 500个样本采集 (2.5秒@200Hz)

5. **STATE_SLEEPING** (睡眠状态)
   - 低功耗睡眠
   - 状态保持
   - 唤醒恢复

6. **STATE_BOOTLOADER** (Bootloader状态)
   - USB固件更新模式

7. **STATE_ERROR** (错误状态)
   - 错误处理
   - LED错误指示

### 1.3 主循环功能流程

```c
while (1) {
    // 1. 看门狗喂狗
    wdog_feed();
    
    // 2. 错误状态处理
    if (state == STATE_ERROR) {
        update_led();
        continue;
    }
    
    // 3. Bootloader模式
    if (state == STATE_BOOTLOADER) {
        bootloader_process();
        continue;
    }
    
    // 4. 睡眠模式
    if (state == STATE_SLEEPING) {
        continue;  // 已在enter_sleep_mode()中处理
    }
    
    // 5. 传感器任务 (200Hz)
    sensor_task();
    // - 读取IMU数据
    // - 温度补偿
    // - 陀螺仪滤波
    // - 自动校准
    // - 传感器融合
    // - 计算线性加速度
    
    // 6. RF数据发送
    rf_transmitter_set_data(quaternion, accel_linear, battery, flags);
    rf_transmitter_process();
    
    // 7. 按键处理
    // - 长按: 进入睡眠
    // - 双击: 配对/校准
    // - 单击: 保留
    
    // 8. LED更新
    update_led();
    
    // 9. 电池检测
    read_battery();
    
    // 10. 睡眠条件检查
    check_sleep_condition();
    
    // 11. USB调试处理
    usb_debug_process();
    
    // 12. 诊断统计更新
    diag_periodic_update();
    
    // 13. 信道管理器更新
    ch_mgr_periodic_update();
    
    // 14. USB MSC任务
    usb_msc_task();
    
    // 15. 短延时
    hal_delay_us(100);
}
```

### 1.4 关键函数说明

#### sensor_task() - 传感器任务
- **功能**: 200Hz传感器采样和处理
- **处理流程**:
  1. 读取IMU数据 (陀螺仪 + 加速度计)
  2. 温度补偿应用
  3. 陀螺仪滤波和静止检测
  4. 自动校准更新
  5. 偏置补偿
  6. 磁力计读取 (可选)
  7. 传感器融合 (6DOF/9DOF)
  8. 四元数获取
  9. 线性加速度计算

#### rf_transmitter_process() - RF发送处理
- **功能**: RF数据包发送和状态管理
- **处理内容**:
  - 同步信标接收
  - 时隙分配
  - 数据包封装
  - 重传机制
  - RSSI监测
  - 状态同步

#### enter_sleep_mode() - 进入睡眠
- **功能**: 低功耗睡眠管理
- **处理流程**:
  1. 保存状态到Retained RAM
  2. 配置唤醒源 (GPIO/WOM)
  3. 进入深睡眠或浅睡眠
  4. 唤醒后恢复状态

#### check_sleep_condition() - 睡眠条件检查
- **功能**: 自动睡眠触发
- **条件**: 静止时间超过 `AUTO_SLEEP_TIMEOUT_MS`

---

## 二、Receiver 固件功能汇总

### 2.1 核心功能模块

#### 2.1.1 RF接收系统
- **多追踪器管理**: 最多支持24个追踪器
- **TDMA同步**:
  - 5ms超帧周期
  - 同步信标广播
  - 时隙分配管理
  
- **数据接收**:
  - 数据包解析
  - CRC校验
  - 序列号检测
  - 丢包统计

- **RF优化** (v0.6.2):
  - 智能信道管理
  - RF自愈机制
  - 时隙优化
  - 时序优化
  - RF Ultra模式支持

#### 2.1.2 USB HID通信
- **SlimeVR协议兼容**:
  - Legacy格式 (Report ID 0x01)
  - Packet0格式 (设备信息)
  - Packet1格式 (传感器数据)
  - Packet3格式 (状态信息)
  
- **数据格式**:
  - 64字节HID报告
  - 每个追踪器10字节数据
  - 200Hz更新频率 (5ms周期)

- **USB命令**:
  - 0x10: 进入Bootloader
  - 0x11: 进入配对模式
  - 0x12: 退出配对模式
  - 0x20: 请求版本信息

#### 2.1.3 追踪器管理
- **追踪器数据结构**:
  - 四元数 (Q15格式)
  - 线性加速度 (mg)
  - 电池电量
  - 状态标志
  - RSSI信号强度
  
- **统计信息**:
  - 总接收包数
  - 丢失包数
  - 重传次数
  - 超时次数
  - CRC错误次数
  - 丢包率 (滑动平均)

- **超时管理**: 1秒无数据认为离线

#### 2.1.4 配对系统
- **配对流程**:
  - 接收配对请求
  - 分配Tracker ID
  - 网络密钥交换
  - 配对数据保存
  
- **配对超时**: 60秒

#### 2.1.5 系统监控
- **看门狗**: 硬件看门狗 + 任务监控
- **复位原因检测**: 区分上电/看门狗/软复位

#### 2.1.6 USB功能
- **USB Bootloader**: UF2固件更新
- **USB HID**: SlimeVR协议通信

### 2.2 运行状态机

Receiver固件使用状态机管理运行流程:

1. **STATE_RUNNING** (运行状态)
   - 正常数据接收和转发
   - USB HID数据发送

2. **STATE_PAIRING** (配对状态)
   - 等待追踪器配对请求
   - 处理配对流程
   - 60秒超时

3. **STATE_BOOTLOADER** (Bootloader状态)
   - USB固件更新模式

4. **STATE_ERROR** (错误状态)
   - 错误处理
   - LED错误指示

### 2.3 主循环功能流程

```c
while (1) {
    // 1. 看门狗喂狗
    wdog_feed();
    
    // 2. 错误状态处理
    if (state == STATE_ERROR) {
        update_led();
        continue;
    }
    
    // 3. Bootloader模式
    if (state == STATE_BOOTLOADER) {
        bootloader_process();
        continue;
    }
    
    // 4. 按键处理
    process_button();
    
    // 5. RF接收器处理
    rf_receiver_process();
    // - 同步信标广播
    // - 数据包接收
    // - 配对处理
    
    // 6. 同步追踪器状态
    // - 从rf_ctx获取数据
    // - 更新本地trackers数组
    // - 超时检测
    
    // 7. 配对模式处理
    if (state == STATE_PAIRING) {
        // 启动配对
        // 检查超时
    }
    
    // 8. USB HID数据发送 (200Hz)
    if (state == STATE_RUNNING) {
        send_usb_report();
    }
    
    // 9. USB HID任务
    usb_hid_task();
    
    // 10. LED更新
    update_led();
    
    // 11. 短延时
    hal_delay_us(100);
}
```

### 2.4 关键函数说明

#### rf_receiver_process() - RF接收处理
- **功能**: RF数据接收和同步信标广播
- **处理内容**:
  - 同步信标发送
  - 数据包接收
  - 配对请求处理
  - 追踪器数据更新

#### send_usb_report() - USB数据发送
- **功能**: 200Hz USB HID数据发送
- **数据格式**:
  - Legacy格式: 64字节报告
  - Packet0: 设备信息 (1Hz)
  - Packet3: 状态信息 (5Hz)
- **内容**: 四元数、加速度、电池、RSSI等

#### send_status_packets() - 状态包发送
- **功能**: 发送Packet3状态包
- **频率**: 约5Hz (每40帧一次)
- **内容**: 电池、状态标志、RSSI

#### send_info_packets() - 设备信息包发送
- **功能**: 发送Packet0设备信息
- **频率**: 约1Hz (每200帧一次)
- **内容**: 固件版本、MCU ID、IMU ID等

---

## 三、Bootloader 功能

### 3.1 功能特性
- **大小**: 468 bytes (最大4096 bytes)
- **启动地址**: 0x0000
- **应用地址**: 0x1000

### 3.2 功能模块
- **UF2支持**: USB大容量存储模式
- **Flash操作**: 固件擦写
- **启动跳转**: 跳转到应用程序

---

## 四、HAL (硬件抽象层) 功能

### 4.1 已实现的HAL模块

#### GPIO (hal_gpio)
- GPIO配置
- GPIO读写
- GPIO中断
- 按键处理

#### I2C (hal_i2c)
- I2C初始化
- 寄存器读写
- 多字节传输

#### SPI (hal_spi)
- SPI初始化
- 单字节传输
- 片选控制

#### Timer (hal_timer)
- 定时器初始化
- 微秒/毫秒延时
- 时间戳获取

#### Storage (hal_storage)
- Flash存储
- 数据读写
- 擦除操作

#### Power (hal_power)
- 电源管理
- 睡眠模式
- 时钟控制

#### Watchdog (hal_watchdog)
- 看门狗初始化
- 喂狗
- 复位原因检测

#### DMA (hal_dma)
- DMA初始化 (部分实现)
- SPI/I2C DMA传输 (待完善)

#### Charging (hal_charging)
- 充电检测
- 电池管理

---

## 五、编译配置选项

### 5.1 功能开关 (config.h)

- `USE_SENSOR_OPTIMIZED`: 优化传感器读取 (FIFO+后台)
- `USE_SENSOR_DMA`: DMA传感器读取
- `USE_USB_DEBUG`: USB调试接口
- `USE_DIAGNOSTICS`: 诊断统计
- `USE_CHANNEL_MANAGER`: 智能信道管理
- `USE_RF_RECOVERY`: RF自愈机制
- `USE_RF_SLOT_OPTIMIZER`: 时隙优化器
- `USE_RF_TIMING_OPT`: 时序优化
- `USE_RF_ULTRA`: RF Ultra模式
- `USE_USB_MSC`: USB大容量存储
- `USE_MAGNETOMETER`: 磁力计支持

### 5.2 融合算法选择

- `FUSION_TYPE`: 选择融合算法类型
  - `FUSION_VQF_ULTRA`
  - `FUSION_VQF_ADVANCED` (默认)
  - `FUSION_VQF_OPT`
  - `FUSION_VQF_SIMPLE`
  - `FUSION_EKF`

---

## 六、性能指标

### 6.1 Tracker固件
- **Flash使用**: ~62KB (text: 62456 bytes)
- **RAM使用**: ~8KB (data: 556 bytes, bss: 7620 bytes)
- **传感器采样**: 200Hz
- **RF发送频率**: 200Hz
- **功耗**: 支持低功耗睡眠模式

### 6.2 Receiver固件
- **Flash使用**: ~38KB (text: 37968 bytes)
- **RAM使用**: ~6.5KB (data: 548 bytes, bss: 6056 bytes)
- **USB发送频率**: 200Hz
- **追踪器数量**: 最多24个

### 6.3 Bootloader
- **Flash使用**: 468 bytes
- **最大限制**: 4096 bytes

---

## 七、固件版本信息

- **版本**: v0.4.2
- **协议版本**: SlimeVR Protocol (兼容nRF格式)
- **编译日期**: 2026-01-28
- **工具链**: xpack-riscv-none-elf-gcc 15.2.0
- **架构**: RISC-V rv32imac_zicsr_zifencei

---

## 八、主要改进 (v0.6.2)

### 8.1 新增功能
1. **智能信道管理**: CCA检测 + 自动避让
2. **RF自愈机制**: 通信中断自动恢复
3. **时隙优化器**: 动态分配 + 优先级调度
4. **时序优化**: 精确同步 + 延迟补偿
5. **RF Ultra模式**: 极限性能模式
6. **优化传感器读取**: FIFO + 后台处理
7. **USB调试接口**: 实时数据流和诊断
8. **诊断统计**: 性能监控和故障排查
9. **温度补偿**: 陀螺仪温漂补偿
10. **自动校准**: 运动中自动校准
11. **静止检测**: 陀螺仪滤波和静止检测
12. **状态保持**: 睡眠后状态恢复
13. **磁力计支持**: 航向校正

### 8.2 系统增强
- 看门狗和故障恢复
- 事件日志系统
- 功耗优化
- USB大容量存储 (UF2)

---

## 九、使用说明

### 9.1 烧录方式
1. **UF2方式**: 拖拽 `.uf2` 文件到USB Bootloader设备
2. **HEX方式**: 使用 `wch-link` 工具烧录 `.hex` 文件
3. **Combined HEX**: 已合并Bootloader的完整固件

### 9.2 配对流程
1. Tracker进入配对模式 (双击按键)
2. Receiver进入配对模式 (按键或USB命令)
3. 自动完成配对和数据交换
4. 配对信息保存到Flash

### 9.3 校准流程
1. 双击按键进入校准模式
2. 保持静止2.5秒
3. 自动完成陀螺仪偏置校准
4. 校准数据保存到Flash

---

## 十、技术规格

### 10.1 硬件要求
- **MCU**: CH592 (RISC-V)
- **Flash**: 448KB
- **RAM**: 26KB
- **时钟**: 60MHz (PLL)

### 10.2 通信协议
- **RF频率**: 2.4GHz ISM频段
- **RF协议**: 私有TDMA协议
- **USB协议**: HID (SlimeVR兼容)
- **数据格式**: 兼容nRF格式

### 10.3 传感器支持
- **IMU**: 多种主流IMU传感器
- **磁力计**: 可选支持
- **采样率**: 200Hz

---

## 十一、故障排查

### 11.1 常见问题
1. **编译错误**: 参考 `BUILD_ERROR_LOG.md`
2. **链接错误**: 检查数学库链接 (`-lm`)
3. **未定义引用**: 检查头文件包含和函数声明

### 11.2 调试工具
- **USB调试接口**: 实时数据流
- **事件日志**: 系统事件记录
- **诊断统计**: 性能监控

---

**文档生成时间**: 2026-01-28  
**固件版本**: v0.4.2  
**编译状态**: ✅ 所有目标编译成功
