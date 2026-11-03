# IMU v1 的兼容扩展

保留 `sensor.v1.ImuSample` 的字段 1–9，在同一消息中追加字段 10–19，不修改旧字段的类型/编号。
这是 additive wire 扩展，不保证旧消费者理解新增信息。消息包仍为 `sensor/v1`；CMake capability
`OPENEMBER_IMU_CONTRACT_VERSION=1` 表示包含本次扩展，并非 Protobuf 包版本递增。

| 字段 | 语义 |
| --- | --- |
| `orientation` / `orientation_valid` | 命名 w/x/y/z，单位 quaternion，R_reference_sensor；v_reference=R(q)·v_sensor |
| `frame_id` / `reference_frame_id` | 姿态两侧坐标系；没有默认 ENU/NED 或默认安装旋转 |
| `mounting_revision` | 驱动输出/安装变换的配置版本；变更时消费者应重新配置或重建跟踪器 |
| `clock_domain` | 生产者 boot/acquisition 会话；重启、断线重连并重置序号必须更换 |
| `receive_time_monotonic_ns` | 原始样本进入生产者时的接收时间；重复发布/录制回放不得刷新成当前时间 |
| 四个测量 `*_valid` | 加速度、角速度、磁场、温度各自有效性；数值零不等于有效 |

`sample_time_monotonic_ns` 保留旧字段语义，可能来自设备采样时钟，不能隐式与新接收时钟相减。
`header.timestamp_unix_ns` 用于消息头/日志关联，不替代样本接收时间；header.sequence 与 sample.sequence 应一致。
跨主机消费者应使用本机接收超时和序号进展检测，不能直接减两台机器的 monotonic 值。

新消息默认的有效标志均为 false，默认 quaternion 全零，不是单位姿态。
有效的 vector/orientation 必须同时携带对应子消息；例如 `angular_velocity_valid=true` 但缺少
`angular_velocity_radps` 不能被消费者解释为有效静止 gyro。标量温度零值可合法，单独由 valid 标志表示可用性。
消费者还需校验有限数值、四元数范数和所需元数据，不可只看 `orientation_valid`。

本扩展不增加估计器、协方差、坐标变换或安全决策。现有 acceleration 字段的重力补偿约定没有在本轮重新定义；
SDK 接入/策略使用前仍必须冻结单位、轴向、手性、重力含义、安装变换和策略期望。

## 兼容与资源限制

- 新 reader 读旧消息时，新字段缺省为 false/空/0，不能直接把旧样本升级成可运动的就绪状态。
- 旧 reader 可继续读字段 1–9；C++ Protobuf 测试还覆盖了旧 reader 保留未知字段后的重新编码。
  不承诺所有语言/运行库都保留未知字段，尤其不能依赖 Nanopb 中转保留未知扩展。
- Nanopb 新增字符串上限：reference_frame_id 和 mounting_revision 各 64 字节，clock_domain 128 字节，
  `max_size` 含结尾 NUL。面向 C 消费者的发送端应遵守这些上限；C++ runtime 本身不自动限制字符串长度。

## 验证

```bash
cmake -S . -B build/imu-contract \
  -DOPENEMBER_MSGS_BUILD_CPP=ON -DOPENEMBER_MSGS_BUILD_TESTS=ON
cmake --build build/imu-contract --parallel 4
ctest --test-dir build/imu-contract --output-on-failure
```

可选传入 `-DOPENEMBER_MSGS_NANOPB_ROOT=/absolute/path/to/nanopb`，额外生成 C 代码并编译/运行
bounded-string、有效标志及 quaternion 往返测试。测试借用现有 Nanopb 源码，不修改它、不安装全局依赖。
2026-11-03：C++ 兼容测试和 Nanopb 0.4.9.1 往返测试共 2/2 通过；这不是 EmberLite 全量固件验收。
