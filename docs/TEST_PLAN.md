# TEST_PLAN (CH59X SlimeVR)
版本：v0.5.1 → v0.6.0  
目标：≤10 Tracker，固定 200Hz（5000us），稳定/丢包少/续航强/低延迟  
更新时间：2026-01-24

---

## A. 必跑：逻辑分析仪时序确认（200Hz + slot 不越界）
### 目的
确认 Receiver SYNC 周期为 5000us，且每个 tracker TX 不跨 slot。

### 连接建议
- CH1：Receiver SYNC GPIO（每帧一次）
- CH2：Tracker TX 指示 GPIO（或 TX 开始触发点）
- （可选）CH3：Tracker IMU INT / Wake pin

### 判定标准
- SYNC 周期 ≈ 5000us（抖动应很小）
- TX 波形始终落在各自 400us slot 内
- 帧尾 guard 存在且稳定（推荐 ≥ 500us）

---

## B. 必跑：10 trackers 长稳测试（60min）
### 记录项（每 10s）
- 丢包率（10s 窗口）
- miss_sync_count
- rf_timeout_count
- crc_fail_count
- 是否掉线/假死

### 通过标准
- 60min 无掉线
- 干扰环境可自愈（见 C）

---

## C. 必跑：干扰环境自愈测试（30min）
### 场景
Wi-Fi 密集（2.4G 忙），PC/路由器附近。

### 通过标准
- 不出现“永远半失联”
- sync 丢失可恢复
- counters 增加合理且不爆炸

---

## D. 必跑：睡眠唤醒压力测试（50次）
### 步骤
1) 静止等待进入 sleep
2) 抬手/移动触发 WOM 唤醒
3) 恢复追踪后再次静止
重复 50 次

### 通过标准
- 唤醒成功率 100%
- 唤醒后 1~2s 内恢复追踪
- 不出现误唤醒风暴（静置 10min 不反复醒）

---

## E. 可观测性与故障报告导出
### 目标
确保用户环境出问题时可导出报告给开发者定位。

### 输出格式建议
`SlimeVR_CH59X_Report_YYYYMMDD_HHMM.json`

### 必须包含
- fw_version/build_time/board_id/imu_type
- superframe_us/slot_us/max_trackers/sleep_mode
- counters（rf_timeout/miss_sync/crc_fail/sleep次数等）
- recent_events（最近 50 条）

---

## F. 封版推荐（≥4小时）
- 10 trackers 4h 长稳
- 记录 counters 曲线与丢包率
- 若支持：记录电池电压变化（续航估算）
