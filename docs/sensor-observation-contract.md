# 通用传感器观测契约

本轮定义 GNSS、相机、激光雷达的最小观测数据，不增加配置 RPC、驱动或视频服务栈。参考 ROS2 字段语义，但使用 OpenEmber Protobuf，不是 ROS wire 兼容协议。

## 设备与数据流

复用 `device.v1.DeviceInfo` 和 `sensor.v1.SensorStatus`，不再定义另一套设备目录。`sensor_id` 对应 `DeviceInfo.device_id`，由部署/驱动提供稳定标识，不使用数组下标或硬编码路径。SensorType 追加 CAMERA=4、LIDAR=5，旧枚举编号不变。没有驱动时，不能把协议类型当作已发现设备。

一台 RGB-D 相机只有一个设备标识，其 color、depth、infrared_left、points 等输出用不同的 `stream_id` 区分；普通 RGB 相机只声明实际提供的流。相机和三维激光雷达共用 `PointCloud`。完整能力发现、主题和控制接口在驱动接入时确定，本轮不先建抽象配置框架。

| 文件 / 消息 | 内容 |
| --- | --- |
| `sensor.proto` / `GnssFix` | 既有定位消息的兼容扩展 |
| `observation.proto` / `ObservationMetadata` | 新观测的设备、流、坐标系与时间 |
| `camera.proto` / `Image`、`CameraInfo` | 原始图像、深度单位与标定 |
| `lidar.proto` / `LaserScan` | 二维平面扫描 |
| `point_cloud.proto` / `PointCloud`、`PointField` | 自描述二进制点云 |

## 时间和坐标系

ObservationMetadata 仅供本轮新增观测共用，不迁移既有 IMU/GNSS 字段，也不是所有消息必须使用的信封。

- `header.timestamp_unix_ns` 保留消息发布/日志关联语义；`header.sequence` 与观测 `sequence` 一致。
- `sample_time_monotonic_ns` / `sample_clock_domain` 是原始采样时刻和对应时钟域，可以来自设备或主机，不能隐式换算。
- `sample_time_unix_ns` 是可选采样 UTC；0 表示未知，不能用当前发布时间补齐。
- `receive_time_monotonic_ns` / `clock_domain` 是原始样本进入生产者的主机时间和会话。重启，或重连并重置序号，必须更换域；重复发布、回放不得刷新原接收时刻。同一流同会话的重复序号不是新采样。
- 未知时间使用 0 和空域；非零单调时间必须带对应域。消费者用本机接收时钟检测断流与序号进展，不得跨主机直接相减。
- `frame_id` 是数据坐标系，不隐含安装变换。相机光学轴为 X 向右、Y 向下、Z 向前，与机体 X 向前、Y 向左、Z 向上的坐标系不同，需要显式外参。

元数据不保证硬件同步。两个流序号相同、或发布时间接近，不代表 RGB/深度已经同步或对齐。

## GNSS：保留字段 1–13

仓库已有 GnssFix，本轮只追加字段 14–26，不改变旧字段、GnssFixType 编号或核心 IGnss 接口。旧适配器仍只填旧字段；接入真实 GNSS 时需要显式设置新增有效性，不把旧默认值当作有效定位。

- 经纬度为 WGS84，单位度，北纬/东经为正；纬度 [-90,90]、经度 [-180,180]，数值必须有限。`frame_id` 标识天线测量点，不是大地基准。
- `position_valid` 表示经纬度有效，同时必须有可用 fix 类型；UNSPECIFIED/NO_FIX 不可用于定位。合法的 (0,0) 与“没有定位”由有效性区分。
- 高度、ENU 速度、参与定位卫星数、水平/垂直精度各有独立 valid。2D fix 不自动意味着高度有效；有效卫星数为 0 也不能据此伪造位置。有效速度必须存在 Vector3 子消息，且分量有限。
- `altitude_reference` 显式区分 WGS84 椭球高和平均海平面高。未知基准不得当作椭球高；转换到 ROS NavSatFix 时需先转换高度或标为不可用。
- 新生产者的 `horizontal_accuracy_m` 为水平标准差 `sqrt(varE+varN)`，`vertical_accuracy_m` 为 `sqrt(varU)`；不能直接填 HDOP、CEP 或未经转换的 95% 精度。旧消息没有 valid 时不推断精度语义。
- `position_covariance_m2` 是 ENU 切平面的行主序 3×3，单位 m²。未知时为空；已知时恰好 9 项、有限、对称半正定，DIAGONAL 的非对角项为 0。缺省零矩阵不是完美定位；极区注意 ENU 的奇异性。
- 原 `sample_time_monotonic_ns` 含义不变，追加 `sample_clock_domain`；接收时钟、采样 UTC 及回放规则同上。GNSS 按 sensor_id 跟踪序号。

本仓既有 2D/3D/RTK 分类不能按枚举数值转换 ROS NavSatStatus，映射存在信息损失。本轮不加原始 NMEA、逐星状态或差分服务控制。

## 图像和标定

Image 只装未压缩像素，不将 JPEG/H.264 冒充 raw encoding。默认 data_valid=false；即使声明 true，元数据缺失、零尺寸或非法布局仍无效。

- 初期 encoding：rgb8/bgr8（3 字节/像素）、rgba8/bgra8（4）、mono8（1）、mono16/16UC1（2）、32FC1（4），大小写敏感，拒绝未知编码。
- `step >= width * bytes_per_pixel`，`data.size == height * step`；保留行填充和端序。使用溢出检查，接收前限制字节数。
- content 区分彩色、深度、红外、单色。UNSPECIFIED 不足以决定用途；彩色用 RGB/BGR 系，深度用 16UC1/32FC1，单色和红外用 mono 编码。
- 深度为光学 Z 距离，正有限 `depth_unit_m` 将像素值换算为米；整数深度常用 0.001、浮点米制用 1。16 位深度的 0 为无返回；浮点非有限或 <=0 无效。无效像素不等于整帧失效。非深度图的单位为 0。
- CameraInfo 关联相同设备、流、frame_id、尺寸与 calibration_revision 的完整输出图像。Image revision 为空表示未关联标定，不能自动套用最新标定。裁剪/缩放后必须给出匹配标定和新 revision。
- calibration_valid=true 时，K/R/P 分别恰好 9/9/12 项，行主序、有限，K 焦距为正、R 为旋转、P 为对应投影。默认全零矩阵不是有效标定。D 初期支持空模型且无系数（无畸变）、plumb_bob 5 项、rational_polynomial 8 项、equidistant 4 项；拒绝未知模型。

不加 ROI/binning、曝光、对齐开关或视频传输协议。外参属于坐标变换系统，不把彩色和深度默认视为同一坐标系。

## 扫描和点云

LaserScan 是二维平面扫描，零角沿 +X，绕 +Z 向 +Y 为正；单位 rad/m/s，采样时刻指第一束。有效扫描至少一束；角增量非零且有限，末角在浮点容差内等于 `min+(N-1)*increment`；时间间隔非负，测距范围有限且 `0 <= min < max`。单束 NaN 为无效，+Inf 为无返回，-Inf 为低于可测范围；这些值及超出范围的距离不能当作有效点。强度为空或与距离等长，其单位由驱动声明。

PointCloud 支持有组织和无组织点云，后者 height=1。fields 名称唯一、count>0、类型明确；`offset+count*type_size <= point_step`，`row_step >= width*point_step`，`data.size == height*row_step`。检查整数溢出、越界和不允许的重叠。初期提供 8 种数值类型，不假定字段顺序；几何点必须有单值浮点 x/y/z，单位 m。is_dense 只表示点是否全部有限，不替代 data_valid 或消费者检查。颜色/强度等附加字段需明确类型和单位，本轮不约定逐点时间或运动补偿。

## 资源边界

Nanopb 字符串上限含 NUL：设备/流/坐标系/revision 各 64 字节、时钟域各 128、encoding 和点字段名各 32、畸变模型 64。协方差/K/R/P 最多 9/9/9/12 项、D 最多 16 项、点字段最多 16 项。图像/点云 bytes 和扫描数组采用 callback，由消费者按部署预算有界处理，避免巨型静态 MCU 数组。C++ Protobuf 不自动执行这些限制。

首次驱动接入必须确定每流的帧字节数、点数/射线数、频率和队列上限，接收长度与业务布局分别校验。**本仓只有 schema 和 wire 测试，没有运行时 payload 校验器**；valid=true 不替代消费端检查。状态/元数据与大载荷分开路由，Web `/api/state` 不承载大图像/点云。预览限频、降采样、压缩或视频通道在真实接入时确定。

## ROS2 参考和验证

参考官方 [NavSatFix](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/NavSatFix.msg)、[Image](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Image.msg)、[CameraInfo](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/CameraInfo.msg)、[LaserScan](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/LaserScan.msg)、[PointCloud2](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/PointCloud2.msg) 和 [PointField](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/PointField.msg)。OpenEmber 额外显式区分有效性、采样/接收时间域、深度单位和标定版本。

```bash
cmake -S . -B build/sensor-contract \
  -DOPENEMBER_MSGS_BUILD_CPP=ON -DOPENEMBER_MSGS_BUILD_TESTS=ON
cmake --build build/sensor-contract --parallel 4
ctest --test-dir build/sensor-contract --output-on-failure
```

sensor_observation_wire_contract 用独立旧 GNSS descriptor 检查字段 1–13、缺省有效性和 C++ 未知字段往返；检查图像端序/填充、点云二进制、扫描 NaN/Inf 与独立时间字段的 wire 保真。不代表驱动、负载预算或实机接入验收。

可选设置 `-DOPENEMBER_MSGS_NANOPB_ROOT=/absolute/path/to/nanopb`，额外生成并编译 C 绑定，验证 GNSS 有效性/协方差往返及大载荷字段采用 callback。2026-11-26 本机 C++ 与 Nanopb 0.4.9.1 检查共 7/7 通过。
