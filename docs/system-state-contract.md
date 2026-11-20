# 整机状态协议 v1

system/v1/SystemState 是与机器人形态无关的观测协议，分别表达生命周期、健康程度和执行器状态。默认逻辑 topic 为 `/system/state`，只读快照端点 `/system/state/get` 的请求体为空、回复为 SystemState；实际 robot/namespace 前缀由 Link 添加。

每个发布进程生成新的 manager_session，序号只在该会话内比较；header.source_instance 应与其一致。订阅方按本地接收时间与 stale_after_ms 判断过期，online=false 明确离线，过期或缺失不能沿用正常状态。motion_session 用于关联当前运动服务，不携带控制授权。

UNKNOWN、故障和告警是独立于运行阶段的健康证据，运行中存在 WARNING 合法。active_alerts/unknown_alerts 是当前可用快照中的计数，unavailable_sources 非空表示存在缺失、不完整或过期来源，不能将计数零解释为整机无告警。当前产品发布者只读聚合，尚无整机状态转换请求协议，也没有自动停止或恢复语义。

本版仅新增 protobuf C++ 生成入口，不修改 motion/v1 或 alerts/v1 契约。通用 reducer 位于 OpenEmber components/system_state，产品规则位于 openember-quadruped。
