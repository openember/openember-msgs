# OpenEmber 消息协议

`openember-msgs` 是 OpenEmber 与 EmberLite 共用的语言无关消息协议仓库。

本仓库只定义稳定的运行时通信契约，不包含具体运行时实现。OpenEmber 可以基于这些 `.proto` 文件生成 C++ Protobuf 绑定；EmberLite 可以使用同一份 `.proto` 与 Nanopb 生成纯 C 绑定。

## 设计目标

- OpenEmber 与 EmberLite 共用一条消息协议主线。
- 使用 Protobuf 作为 schema 与 wire format。
- 保持协议对 Nanopb 和受限设备友好。
- 由 Zenoh key 负责路由，由 Protobuf message 负责结构化数据。
- 通过 `v1`、`v2` 等包路径显式管理协议版本。

## 包结构

```text
proto/openember/msgs/common/v1
proto/openember/msgs/node/v1
proto/openember/msgs/lifecycle/v1
proto/openember/msgs/diagnostics/v1
proto/openember/msgs/parameter/v1
proto/openember/msgs/log/v1
proto/openember/msgs/device/v1
proto/openember/msgs/runtime/v1
proto/openember/msgs/sensor/v1
proto/openember/msgs/actuator/v1
proto/openember/msgs/actuator/v2
proto/openember/msgs/power/v1
proto/openember/msgs/geometry/v1
proto/openember/msgs/host/v1
```

说明：

- `common/v1`：通用类型，例如 `Header`、`Status`、`QosProfile`、`HealthState`、`KeyValue`、`Metric`。
- `node/v1`：节点发现与自省，例如 `NodeInfo`、`NodeHeartbeat`、`NodeQuery`、`NodeRegistrySnapshot`。
- `lifecycle/v1`：节点生命周期状态、状态切换命令与响应。
- `diagnostics/v1`：健康状态与诊断信息。
- `parameter/v1`：运行时参数读取、设置与事件通知。
- `log/v1`：结构化日志记录。
- `device/v1`：面向 LPIO 的设备信息与设备状态。
- `runtime/v1`：面向 launch manager 的进程启动、停止、事件与完整状态快照消息。
- `sensor/v1`：IMU、温度、GNSS，以及图像、标定、二维扫描和点云基础观测；见[传感器观测契约](docs/sensor-observation-contract.md)。
- `actuator/v1`：关节控制器命令、状态与执行器健康状态。
- `actuator/v2`：独立版本的关节目标、控制权管理、能力与逐关节反馈；见 [关节控制契约](docs/actuator-control-contract.md)。核心已有离线校验和 HI 会话适配，现有网络端点仍使用 v1。
- `power/v1`：电池、电源状态与电源控制命令。
- `geometry/v1`：平面位姿与速度，例如 `Pose2D`、`Twist2D`（宿主机仿真 / teleop 示例）。
- `host/v1`：主机资源快照，例如 CPU / 内存 / 磁盘占用（`HostResourceSnapshot`）。

## Zenoh Key 约定

Zenoh key 应保持稳定、可读、可调试。建议核心 key 如下：

```text
openember/<robot_id>/<namespace>/nodes/<node>/info
openember/<robot_id>/<namespace>/nodes/<node>/heartbeat
openember/<robot_id>/<namespace>/nodes/<node>/lifecycle
openember/<robot_id>/<namespace>/topics/<topic>
openember/<robot_id>/<namespace>/services/<service>
openember/<robot_id>/<namespace>/diagnostics/<node>
openember/<robot_id>/<namespace>/logs/<node>
openember/<robot_id>/<namespace>/devices/<device>
openember/<robot_id>/<namespace>/runtime/processes/<process>
```

Zenoh key 用于选择数据流或服务；Protobuf message 用于描述 payload 结构。除非未来录包、桥接或跨域转发确实需要，否则第一版不强制引入全局 envelope。

## 生成 C++ 绑定

默认 CMake 配置只暴露 `.proto` 文件。需要生成并编译 C++ 绑定时启用：

```bash
cmake -S . -B build -DOPENEMBER_MSGS_BUILD_CPP=ON
cmake --build build
```

也可以直接调用 `protoc`：

```bash
protoc -I proto --cpp_out generated/cpp \
  proto/openember/msgs/common/v1/common.proto
```

## 使用 Nanopb 生成 C 绑定

Nanopb 约束文件位于：

```text
nanopb/openember_msgs.options
```

使用同一份 `.proto` 生成纯 C 代码：

```bash
python3 generator/nanopb_generator.py \
  -I proto \
  -f nanopb/openember_msgs.options \
  proto/openember/msgs/common/v1/common.proto
```

`openember_msgs.options` 会限制字符串长度和 repeated 字段数量，使 EmberLite 可以使用静态内存承载消息结构。

其中小型字段优先使用 `max_size` / `max_count`，便于 C 端直接读写；节点列表、设备列表、诊断数组、日志批次、参数批量响应、节点 endpoints 等可能变大的集合使用 `type:FT_CALLBACK`，避免在受限设备上为一次消息预分配过大的结构体。

## 兼容性规则

- 不重命名已有 package、message、enum、field。
- 不复用已使用过的 field number。
- 删除字段时在后续修改中使用 `reserved` 保留字段号和字段名。
- 新增字段只能使用新的 field number。
- 破坏性变更放入新的包版本，例如 `node/v2`。
- 核心消息中避免使用 `Any`、`map`、无界 repeated 字段和过深嵌套。

## 当前范围

IMU 新增姿态、逐通道有效性及接收时钟域，保持原字段 wire 兼容。
迁移规则、Nanopb 上限和测试方法见 [IMU 契约](docs/imu-contract.md)。

电源消息追加逐字段有效性、接入状态是否已知、接收时间与时钟域；见 [电源契约](docs/power-contract.md)。

本仓库定义核心运行时消息和传感器、执行器、电源基础消息。相机与 LiDAR 现有最小观测 schema，GNSS 保留旧 wire 布局并追加有效性、时间域及高度基准；本轮不包含驱动、视频传输或设备控制。Radar、Audio、导航、视觉推理和 Agent 等高层业务协议待明确需求后再增加。

- [Alerts v1: structured events, full state and heartbeat](docs/alerts-contract.md)

## 公共运动控制协议

`motion/v1` 定义跨四足、人形、轮足与轮式机器人的高层平面速度、控制权、模式/动作请求、结果、状态及能力契约，具体支持项由能力声明决定。当前仅提供协议与 wire 测试，尚未注册运行时控制服务；六自由度和全身轨迹不在首版范围。见 [公共运动控制契约](docs/motion-control-contract.md)。

### 受管理进程快照

`runtime/v1.ProcessStateSnapshot` 用于 `/runtime/process/state` 的完整替换快照，包含管理器会话、PID、在线标记、失效期限和全部受管理进程；配置中未启动或已退出的进程也保留。消费者按 manager_session 与 header.sequence 判断进展，使用本地时钟计算失效，来源过期后应将运行状态标记为未知。此列表与 `/nodes/query` 的逻辑节点注册信息不同；管理器自身 PID 单列，不计入 processes。

`ProcessInfo` 的新增字段以追加编号保持原有消息兼容，覆盖就绪方式、期望运行状态、依赖、自动失败重试计数和心跳监督。restart_limit=0 表示不限次数，heartbeat_timeout_ms=0 表示未启用监督；只有 readiness_notify=true 的 ready 才表示应用就绪，其他模式仅确认进程创建。快照不包含进程参数和环境变量。Nanopb 静态配置最多容纳 32 个进程、每进程 16 个依赖；这是嵌入式解码容量，不限制主机端 C++ 的进程数量。

网络只读快照 `/network/state` 的范围、有效性与边界见 [网络观测契约](docs/network-state-contract.md)。
