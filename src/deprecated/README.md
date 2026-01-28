# Deprecated/Unused Functions

本文件夹包含从 main_tracker.c 和 main_receiver.c 中提取的未调用函数。

这些函数是早期开发时的本地实现，后来被模块化的 rf_transmitter.c 和 rf_receiver.c 替代。
保留这些代码仅供参考，不会被编译到最终固件中。

## 从 main_tracker.c 提取的函数

| 函数名 | 原始行号 | 说明 |
|--------|----------|------|
| build_tracker_packet() | 315-363 | 数据包构建 (被 rf_transmitter 替代) |
| rf_transmit_task() | 365-412 | RF发送任务 (被 rf_transmitter_process 替代) |
| sync_search_task() | 414-444 | 同步搜索 (被 rf_transmitter 状态机替代) |
| pairing_task() | 463-509 | 配对任务 (被 rf_transmitter 配对流程替代) |
| process_sync_beacon() | 446-461 | 信标处理 (被 rf_transmitter 替代) |
| process_pairing_response() | 511-530 | 配对响应 (被 rf_transmitter 替代) |

## 从 main_receiver.c 提取的函数

| 函数名 | 原始行号 | 说明 |
|--------|----------|------|
| broadcast_beacon() | 197-246 | 信标广播 (被 rf_receiver 定时器替代) |
| receive_tracker_data() | 248-259 | 数据接收 (被 rf_receiver_process 替代) |
| process_tracker_packet() | 261-309 | 数据包处理 (被 rf_receiver 回调替代) |
| process_pairing_request() | 311-386 | 配对请求处理 (被 rf_receiver 替代) |

## 恢复说明

如果需要恢复这些函数到主文件:
1. 复制对应的函数代码
2. 添加函数声明到文件开头
3. 在主循环中添加调用
4. 移除对模块化 API 的调用 (rf_receiver_process/rf_transmitter_process)

**注意**: 不建议恢复，模块化实现功能更完整，维护更方便。
