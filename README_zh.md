# openember-msgs

OpenEmber 与 emberlite 共享的、与编程语言无关的协议定义。

本仓库包含核心运行时通信的稳定线协议契约。OpenEmber 可基于这些文件生成 C++ Protobuf 绑定；emberlite 可基于同一套 `.proto` 源文件，通过 Nanopb 生成纯 C 绑定。

## 设计目标

- 为 OpenEmber 与 emberlite 维护一条清晰、统一的协议线。
- 使用 Protobuf 作为模式定义与线格式。
- 使消息定义对 Nanopb 与资源受限设备友好。
- 由 Zenoh 键负责路由，由 Protobuf 消息负责结构。
- 通过 `v1`、`v2` 等显式版本化各包。

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
```

`common/v1` 定义共享类型，例如 `Header`、`Status`、`QosProfile`、`HealthState`、`KeyValue` 与 `Metric`。

`node/v1` 定义节点发现与自省相关消息：`NodeInfo`、`NodeHeartbeat`、`NodeQuery` 与 `NodeRegistrySnapshot`。

`lifecycle/v1` 定义生命周期状态转换与命令响应。

`diagnostics/v1` 定义健康状态与诊断报告。

`parameter/v1` 定义运行时参数的 get/set/event 消息。

`log/v1` 定义结构化日志记录。

`device/v1` 定义面向 LPIO 的设备清单与状态消息。

`runtime/v1` 定义供启动管理器使用的进程 start/stop/event 消息。

## Zenoh 键约定

Zenoh 键应保持稳定且可读。建议的核心键如下：

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

Zenoh 键选择数据流或服务；Protobuf 消息声明载荷结构。除非未来的桥接或录制格式确有需求，否则避免引入强制性的全局封装信封。

## 构建 C++ 绑定

默认 CMake 配置仅暴露 `.proto` 文件。需要时可启用生成的 C++ 绑定：

```bash
cmake -S . -B build -DOPENEMBER_MSGS_BUILD_CPP=ON
cmake --build build
```

消费方也可直接运行 `protoc`：

```bash
protoc -I proto --cpp_out generated/cpp \
  proto/openember/msgs/common/v1/common.proto
```

## 使用 Nanopb 生成 C 绑定

Nanopb 选项文件位于：

```text
nanopb/openember_msgs.options
```

使用同一套 `.proto` 文件配合 Nanopb：

```bash
python3 generator/nanopb_generator.py \
  -I proto \
  -f nanopb/openember_msgs.options \
  proto/openember/msgs/common/v1/common.proto
```

该选项文件限制字符串长度与 repeated 字段数量，以便 emberlite 可对生成的 C 类型进行静态分配。

## 兼容性规则

- 不得重命名包或已有字段。
- 不得复用字段编号。
- 删除字段时，应在后续编辑中保留其编号与名称。
- 新增字段只能使用新编号。
- 破坏性变更应放入新包版本，例如 `node/v2`。
- 核心消息中避免使用 `Any`、`map`、无界 repeated 字段与深层嵌套。

## 当前范围

本仓库定义核心运行时及 Hardware Interface 使用的基础传感器、执行器、电源消息。
视觉、导航或智能体等高层载荷应按需要作为独立包增加。

## IMU 契约补充

公共 IMU 的姿态、有效性、接收时钟域及旧消息兼容规则见 [IMU 契约](docs/imu-contract.md)。
