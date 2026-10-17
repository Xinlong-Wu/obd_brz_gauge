# 通用 CAN 广播信号发现

[English](CAN_DISCOVERY.en.md) | 简体中文

`tools/elm327_can_discover.py` 是独立的主机工具：BLE 监听 → JSONL 留档 → 离线盘点 / 状态对比。不依赖固件车型表，不限 MX-5，也不限门、车灯、车顶等预设信号；标签由实验者自由命名。它只给出相关性候选，不自动给信号命名，不证明含义，也不自动生成固件规则。

## 边界与安全

- v1 仅支持 BLE ELM327；不支持 WiFi、USB 或蓝牙串口。`scan` / `capture` 需要 `bleak`，`summary` / `analyze` 只依赖 Python 标准库。
- 只监听经典 CAN 数据帧：11 / 29 位 ID，DLC 为 0–8（包括零字节数据帧）。不支持 CAN FD / remote frame；遇到不支持的格式会明确失败，不猜测解码。
- 车辆停稳、驻车，按维修手册操作；车顶等机构须保持运动区域无人。不要边开车边操作电脑。关闭占用同一适配器的手机 App / 仪表连接，一次只允许一个客户端。
- 不发送车辆诊断请求，没有隐藏的 `0100` 探测、任意发送接口或控制执行器功能。但适配器收到 `ATCSM1` 后返回 `OK`，**不等于电气层已证实静默**；克隆设备可能忽略命令。需要严格无 ACK / 无发送保证时，用经过验证的 listen-only 硬件。
- 原始采集含设备标识、时间与车辆活动信息，默认按私有数据处理；保存在仓库外，不提交、不上传。分享前另做脱敏副本。

## 安装与命令入口

从仓库根目录运行；推荐虚拟环境：

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install bleak
mkdir -p ~/can-captures
python3 tools/elm327_can_discover.py --help
python3 tools/elm327_can_discover.py scan --scan-timeout 5
```

全部时间参数（`--scan-timeout`、`--duration`、`--settle`、`--timeout`、`--drain-timeout`）的单位都是**秒**。`scan` 默认 5 秒；`capture` 的 `--timeout` 默认 `3` 秒，用于等待适配器命令提示符，`--drain-timeout` 必须为正数、默认 `30` 秒，单独用于停止后的后台排空，两者不可混用。`--buffer-mb` 为 `1`–`1024` 的整数，默认 `64`，单位为 MiB（`1 MiB = 1024 × 1024 bytes`）；预算含接收载荷与估算的每项开销，不是精确进程 RSS 上限。`capture` 的 `--protocol` 默认 `6`，可选协议如下，不自动探测波特率、不自定义波特率、不自动切换 CAN 引脚：

```text
6 = 11-bit CAN, 500 kbit/s
7 = 29-bit CAN, 500 kbit/s
8 = 11-bit CAN, 250 kbit/s
9 = 29-bit CAN, 250 kbit/s
```

`capture` 用 `--name IOS-Vlink` 或 `--address UUID` 二选一（用扫描得到的真实名称 / 地址替换示例；macOS 地址通常是 UUID）。JSONL 和 CSV 输出均**不覆盖已有文件**；父目录必须事先存在。重复实验换新文件名，没有 `--force`。

## 先盘点，再设计标签实验

先不做目标动作，用优化后的 **10 秒无过滤试采集、64 MiB 主机缓冲**检查连通性、格式、实际可见的 ID 与吞吐指标，避免凭空挑一个“车身 ID”，也不要一开始就过滤掉未知候选。ELM / BLE 仍可能数秒就溢出，短采集不保证完整；先看接收 / 处理速率、主机缓冲积压 / 峰值和 writer lag，再决定下一步：

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 10 --buffer-mb 64 \
  --label inventory --no-input --out ~/can-captures/inventory-01.jsonl
python3 tools/elm327_can_discover.py summary \
  ~/can-captures/inventory-01.jsonl --top 50 --out ~/can-captures/inventory-01.csv
```

`summary` 按 ID / DLC 盘点帧数、接收频率（Hz）、变化字节等；`--top` 默认 `50`，只限制终端显示，`--out` CSV 导出全部结果；省略 `--out` 可只看终端结果。多文件频率分母为全部文件监听窗口之和，包含某 ID 未出现的文件，不是只按它出现的时段计算。主机 BLE 收包时间戳**不是 CAN 线上的发送时间**；适配器缓冲、BLE 分包 / 合包和吞吐限制都会影响观测频率，不能当作精确总线时序或完整总线负载。

## 主机缓冲与吞吐诊断

BLE 事件循环只做最小接收工作：保存接收时间、按字节入队并及时识别命令提示符；逐帧解析与 JSONL 写盘由专用后台线程处理，不在 BLE 回调里执行。本地标签、命令与其他元数据也进入同一 writer FIFO，与接收块按入队顺序写出，不另开一条可能乱序的写盘路径。

`--buffer-mb` 限制的是含估算每项开销的字节预算，不是固定条数或无限队列。预算耗尽会明确累计丢失的接收块数 / 字节数并标记 incomplete，触发停止；预算耗尽时不采用环形覆盖，也不为了保留尾部而丢弃已排队的早期帧。排空超时的丢弃规则见下文。增大缓冲只能吸收暂时积压，不能解决持续处理速率不足，也不能扩展物理 BLE 链路吞吐或 ELM 内部缓冲容量。

采集中终端每 `2` 秒显示接收速率（KiB/s）、处理速率（KiB/s）、缓冲待处理量 / 上限 / 高水位和累计解析帧数。周期性遥测使用既有 `event` 记录，`code="telemetry"`、`message="host receive/writer statistics"`、`incomplete=false`，不新增记录类型；该事件的 `incomplete=false` 不代表整次采集完整。遥测事件与最终 `summary` 均包含以下累计计数 / 缓冲状态 / 延迟指标：

```text
rx_bytes
processed_bytes
queue_drops
dropped_bytes
buffer_bytes
buffer_limit_bytes
peak_buffer_bytes
writer_lag_seconds
frames
```

遥测事件还包含 `rx_bytes_per_second`、`processed_bytes_per_second`，单位为 bytes/s，终端除以 `1024` 显示为 KiB/s；最终 `summary` 不包含这两个区间速率字段。`queue_drops` 统计被拒绝或排空超时丢弃的接收块，不是 CAN 帧数；对应载荷字节计入 `dropped_bytes`。接收 / 处理字节统计是主机收到 / 处理的适配器文本字节，不是 CAN 线上载荷量。终端速率用于观测当前吞吐，累计指标用于检查整次采集。

1. 若处理速率持续低于接收速率，待处理缓冲与 writer lag 上升，先排查 / 优化主机解析、写盘及其他负载，必要时增加 `--buffer-mb` 再试；仅延长 `--duration` 会放大积压。
2. 若出现 ELM `BUFFER FULL`，而主机积压低，说明瓶颈更可能在主机队列之前，例如适配器内部或 BLE 传输；这不能单凭指标确定具体部件。增大主机缓冲不能修复上游已经丢失的数据。
3. 先完成上述吞吐检查；若当前适配器 / 链路仍无法承受无过滤流量，再按下文进行硬件分组过滤并逐步收窄，每次换新文件重新实验。不要把扩大主机缓冲或没有本地丢块当成全链路无损保证。

## 自由标签实验

一次只改变一个条件，先稳定 `baseline`，再 `active`，再回到 `baseline`；在同次采集及不同次采集中重复。切换期间使用独立的 `transition` 标签，稳定后再标目标状态；记录点火、发动机、其他负载和操作顺序，避免把时间漂移当成目标信号。以下为 120 秒实验命令，使用 128 MiB 主机缓冲、60 秒队列排空期限（不保证进程在 60 秒内退出，见下文文件系统 I/O 限制）；应先按上文检查吞吐，必要时按下文加入已验证的 `--filter` / `--mask` 或 `--can-id`，不要未经检查就延长无过滤采集。

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 120 --buffer-mb 128 --drain-timeout 60 \
  --label baseline --note 'repeat-01; parked; one condition at a time' \
  --out ~/can-captures/experiment-01.jsonl
```

采集中在终端输入任意标签行并回车（下列为示例标签，不是 shell 命令）：

```text
transition
active
transition
baseline
/quit
```

标签与 `--note` **只写本地记录，绝不发往车辆**。`/quit` 停止采集，`Ctrl-C` 请求优雅停止并收尾；输入 EOF 只关闭标签输入，采集仍持续至 `--duration` 到期。`--no-input` 完全禁用交互标签，适合盘点或按固定状态分别采集。自然到时、`/quit` 或 `Ctrl-C` 时，先立即向适配器发送停止监听命令，而不是先等待主机积压排空；BLE 回调直接确认返回提示符，不被后台 writer 延迟。随后尝试在 `--drain-timeout` 内排空已入队的尾部数据，再写最终汇总。排空超时会标记 incomplete，丢弃尚未处理的排队接收数据，明确累计 `queue_drops` / `dropped_bytes`，保留已写数据；仍须等待正在进行的文件系统 I/O 完成及后台线程退出后才能安全写最终汇总。工具不能强制中断挂起的文件系统 I/O，因此 `--drain-timeout` 不是进程退出的硬时限，也不会把未写完的文件误报为 complete。自然到时或主动停止都应检查最终完整性，不以“文件存在”判断成功。

`active` 可以是任何可安全、可重复设置的状态，不限离散开关；连续量实验也可自定义多个稳定区间标签，逐对比较。手动标签有反应延迟，不适合证明毫秒级因果关系。

## 离线对比与复验

```bash
python3 tools/elm327_can_discover.py summary \
  ~/can-captures/experiment-01.jsonl ~/can-captures/experiment-02.jsonl
python3 tools/elm327_can_discover.py analyze \
  ~/can-captures/experiment-01.jsonl ~/can-captures/experiment-02.jsonl \
  --state-a baseline --state-b active --settle 1 --min-samples 5 \
  --top 20 --out ~/can-captures/candidates-01.csv
```

`experiment-02.jsonl` 是另一次按相同流程取得的采集，不是复制第一份。合并分析要求协议与硬件过滤设置一致；不同过滤分组分别盘点、分别对比，不能合并成完整总线频率或状态覆盖。`summary` / `analyze` 都可读一个或多个文件；离线操作不连接适配器。`analyze` 必须指定两种不同标签，默认跳过每段开始的 `1` 秒（`--settle`），每个文件 / 标签段 / ID / DLC 最低样本数为 `5`（`--min-samples`），终端显示前 `20` 个候选（`--top`），CSV 仍导出全部结果。缺失 / 不足样本会显示覆盖诊断，不补成 0；`score` 只是启发式排序，不是概率或置信度。字节从 **byte 0** 编号；每字节 **bit 0 是 LSB**，不是部分外部资料采用的 MSB-first 编号。

排名是状态与字节 / 位变化的**相关性线索**，不是已知信号表、单位 / 公式识别或含义证明；计数器、校验字节、点火变化和同步负载都可能排在前面。复验时：

1. 用候选的精确 ID 重新采集，重做多次 `baseline → active → baseline`，检查回归与跨次一致性。
2. 加入与目标无关的动作作对照，排除电压 / 负载 / 时间等混杂因素；保留失败与反例。
3. 区分瞬时事件与持续状态；例如灯的实际闪烁反馈不等于拨杆 / 按钮请求，车顶运动不等于已锁止。单一动作同时改变多项状态时不能据此命名某个位。
4. 只有独立复验过的映射才考虑固件适配；本工具的通用候选不受固件现有通道限制。

## 过滤与吞吐限制

硬件接收过滤会限制**实际可见范围**，不是离线显示筛选。`--can-id HEX` 与 `--filter HEX --mask HEX` 互斥；后两项必须一起提供。按协议用 3 位 / 8 位十六进制：11 位 ID 为 `000`–`7FF`，29 位 ID 为 `00000000`–`1FFFFFFF`。例如 `123` 仅为精确过滤语法示例，不代表已知信号：

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 120 --label baseline \
  --can-id 123 --out ~/can-captures/exact-123-01.jsonl
python3 tools/elm327_can_discover.py capture \
  --address UUID --protocol 6 --duration 120 --label inventory --no-input \
  --filter 000 --mask 700 --out ~/can-captures/group-000-01.jsonl
```

掩码匹配规则：

```text
(id & mask) == (filter & mask)
```

`--protocol 6 --filter 000 --mask 700` 只覆盖 `000`–`0FF`，**不是全 11 位 ID**。固定 `--mask 700`，另以 `--filter 100`、`200`、`300`、`400`、`500`、`600`、`700` 分组采集；最后一组覆盖 `700`–`7FF`。每组用新文件、重做同一标签实验，分时采集不能冒充同步全总线视图。完成无过滤试采集和主机吞吐检查后，若仍需限流，再从分组盘点逐渐收窄到精确 ID；被过滤掉、网关隔离或未广播的信号，离线分析都无法恢复。

## 初始化、记录格式与完整性

采集顺序为复位并等待稳定，逐项检查适配器初始化响应，清除 / 设置硬件过滤，然后进入监听：

```text
ATZ
ATE0
ATL0
ATS0
ATH1
ATCAF0
ATD0
ATCFC0
ATCSM1
ATSPn
ATMA
```

`n` 为选定协议。`ATZ` 复位适配器并重载其保存的设置；这些设置保存在适配器侧，不是车辆侧。无过滤模式在 `ATMA` 前显式清除遗留接收过滤，精确过滤模式设置 `ATCRA`，掩码模式设置 `ATCF` / `ATCM`，各项均检查响应。收到超出所选过滤范围的 ID 会报错并停止，不能把命令确认当成过滤已生效的唯一证据。`ATS0` 替代 `ATS1`，去掉文本空格以降低传输量；`ATH1` 仍保留 CAN 头，原始接收字节仍写入记录，解析器已支持无空格紧凑格式。`ATCAF0` 保留原始帧，`ATCFC0` 关闭自动流控，`ATCSM1` 请求静默监听。关键命令不支持 / 不确认会中止，不静默回退到主动查询。此命令链及 `OK` 仍不能证明克隆硬件在电气层的行为。

输出为每行一条 JSON 的 JSONL，schema 为 `elm327_can_discover`、version 为 `1`，记录类型包括：

```text
session
transport
command
command_response
rx
frame
marker
event
monitor_start
monitor_end
summary
```

其中保留会话与配置、BLE 通道信息（`transport`）、命令及确认（`command_response`）、原始接收、解析帧、人工标签、事件、监听边界、周期性吞吐指标（`event`，`code="telemetry"`）和最终汇总；这些额外诊断记录均为本地数据。遥测沿用 schema version `1` 的既有 `event` 类型，离线分析器不变，旧采集不要求补写遥测事件。`monitor_start` / `monitor_end` 用于区分初始化与有效监听窗口；`monitor_end` 的时间戳取实际接收关闭时刻，即使记录在排空后写入，也不以最终写盘时刻计算，因此停止后的后台排空不延长监听时段或频率统计分母。时间单位为秒，接收时间来自主机 BLE 回调，不是硬件 CAN 时间戳。不要将旧 dump 文本直接改扩展名冒充此 schema。

BLE 错误 / 断连、ELM `BUFFER FULL`、畸形报文、过滤范围外的 ID、主机字节预算耗尽导致丢块或后台排空超时，都会使采集 incomplete；零有效帧或缺少完整收尾的文件同样不能当完整样本。`summary` / `analyze` **默认拒绝不完整采集**。仅排障时可加 `--allow-incomplete`，工具会警告；结果只作诊断，不可当作完整频率统计或信号验证依据。保留已有数据用于排障，按吞吐指标先检查主机积压，再视需要缩小硬件过滤范围，用新文件重新采集；不能忽略丢帧继续结论。

## MX-5 NC / PRHT 只是示例

NC 的 PRHT（电动折叠硬顶）可用稳定状态与运动过渡做实验，但手动软顶的机构、传感器及控制模块不同，不能套用 PRHT 假设。年款 / 市场 / 配置也可能改变可见报文。这里不提供未经验证的门、灯或车顶位图，也不保证任何目标状态会在适配器所接总线上广播。

无广播结果不等于无该状态：先核对协议、过滤、连接网络与网关可见性；不要假定 NC 车身总线一定是 125 kbit/s，或适配器一定能切换到 MS-CAN 引脚。若最终需要模块增强诊断，应另行依据维修资料进行**只读**查询；那是主动请求路径，不是本工具的被动采集。v1 不盲扫 DID，也不控制车顶等机构。

参考资料只用于建立待验证假设，各自的位编号需单独核对：

- [NC CAN 社区记录](https://github.com/hatsunearu/nc-mx5-miata-canbus/blob/main/mazda_mx5_nc.md)：已有动力 / 底盘等记录，仍有 WIP，不能推导成完整车身信号表。
- [NC PRHT 维修资料：power_roof.pdf](https://www.mellens.net/mazda/Mazda-Miata-2006-2007/power_roof.pdf)：机构、传感器与诊断流程参考；不是已验证的 CAN 位图，须匹配实车版本。
- [ELM327 官方数据手册](https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf?download=1)：协议、监听与过滤命令；不能代替克隆适配器实测。

## 开发验证与现有工具

无需车辆 / BLE 的主机测试从仓库根目录运行：

```bash
python3 -B -m unittest discover -s tests -p 'test_*discover*.py' -v
```

测试不证明克隆设备静默性或实车信号映射。既有 `tools/elm327_ble_dump.py` 保持不变，仍用于交互 / 主动 PID 查询；不要与本工具同时占用适配器。其他工具见 [tools/README.md](../tools/README.md)，经实车验证后如何适配见 [VEHICLES.md](VEHICLES.md)。
