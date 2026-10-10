# 16 页设计规范反查

[English](DESIGN_AUDIT.en.md) | 简体中文

检查日期：2026-10-10。对象为仓库 `ui-prototype` 当前 v14 工作副本，依据 [DESIGN_SYSTEM.md](DESIGN_SYSTEM.md)、`foundations.js` 和 `scenarios.js`。

## 结论

**已补齐规范中列出的报警、提醒和系统场景，当前示例通过检查。** 16 页提供 50 个场景选项，其中 34 个为异常或系统场景；08 的三个单轮场景均可定位 FL / FR / RL / RR。导出包含 78 张画板及正常 / 报警两份总览。

报警项使用读数和原因共同表达。换挡、自定义限值和心率范围提醒采用琥珀色；监测异常采用红色；低油压、转速超限、极低胎压等危急状态增加外缘细红弧。14 保留正常亮度设置，不增加数值阈值报警。

## 检查范围

- 浏览器在 360×360 显示下检查全部场景选项，08 另外检查三个单轮场景的四轮定位。核对报警文字、单位、点击区域和圆边，字体排版框空白不作为字形遮挡证据。
- 检查 11 个车辆页在报警时断连：有效图形与报警提示均隐藏，恢复数据后保留所选场景。11 心率有独立无数据状态，车辆断连不影响其连接状态。
- 验证 09 / 10 / 11 限值输入，及媒体不可用、连接丢失、配对失败 / 过期的控件状态与恢复路径。07 整体切换和 13 正常播放 / 切歌继续使用既有交互。
- `npm test` 的 13 项检查通过，覆盖基础同步、组件几何、状态语义、全场景渲染、断连优先级、多项异常排序、四轮定位、可选限值及系统禁用状态。
- 已重新导出 SVG、文字轮廓、PNG、正常 / 报警总览及 manifest；浏览器预览读取仓库目录。

## 逐页结果

`PASS_SCENARIOS` 表示当前正常状态与列出的场景通过；`PASS_NORMAL` 表示设置页无需数值报警，正常交互通过。所有场景 ID 保持与代码一致。

| 编号 | 页面 | 本次结果 | 已检查场景 ID |
|---|---|---|---|
| 01 | GEAR & RPM | PASS_SCENARIOS | shift / rpm-high / oil-hot / oil-low |
| 02 | MANIFOLD | PASS_SCENARIOS | pressure-high |
| 03 | COOLANT | PASS_SCENARIOS | coolant-hot |
| 04 | OIL TEMPERATURE | PASS_SCENARIOS | oil-hot |
| 05 | OIL PRESSURE | PASS_SCENARIOS | oil-low |
| 06 | POWERTRAIN | PASS_SCENARIOS | pressure-high / oil-hot / oil-low / multiple |
| 07 | ENGINE VITALS | PASS_SCENARIOS | oil-low / oil-hot / coolant-hot / voltage-low / voltage-high / pressure-high / multiple |
| 08 | TYRE MONITOR | PASS_SCENARIOS | tyre-low / tyre-high / tyre-hot / multiple |
| 09 | SPEED | PASS_SCENARIOS | speed-limit |
| 10 | G-FORCE | PASS_SCENARIOS | g-limit |
| 11 | DRIVER PULSE | PASS_SCENARIOS | heart-high / heart-low / no-data |
| 12 | PRESSURE / ANALOG | PASS_SCENARIOS | pressure-high |
| 13 | NOW PLAYING | PASS_SCENARIOS | media-unavailable |
| 14 | ILLUMINATION | PASS_NORMAL | normal |
| 15 | CONNECTIONS | PASS_SCENARIOS | connection-lost / no-device |
| 16 | PAIR DEVICE | PASS_SCENARIOS | pair-failed / pair-expired |

08 的 `tyre-low` / `tyre-high` / `tyre-hot` 支持四个独立轮位；`multiple` 使用 FL 低压、RR 高压、RL 高温。06 / 07 的多项异常按严重度排序摘要，并分别标记每个异常项。

## 先前问题处理

- **I01 / I03 / I04**：动态指针、有效弧、转速段、温度柱和进度条现在跟随场景，断连时隐藏；清空胎温，取消有效巡航状态，抑制失去数据后的越限文案。
- **I05**：06 分隔线与下方分区后移，主读数的 `bar` 留在主区域，分隔线不再穿过单位。
- **I06**：13 歌曲标题登记为 `mediaTitle` 字号角色；静态刻度的字号绑定 `support`。
- **I07**：11 补齐高 / 低心率提醒与独立无数据状态，范围可调整；文本表达自定义范围，不作健康诊断。
- **I02**：02 指针扫过底层印刷名称属于已接受的机械表盘层叠；保留该形式。报警时的针位由示例压力计算。

## v14 视觉修订

- 全部报警文案与轮位去除叹号，保留原因、颜色和细线标记。
- 03 删除柱内独立红段，110–120°C 刻度与 120 标签标红，柱体和游标跟随示例温度。
- 10 删除底部 LAT / LON 及重复数字行；正常只显示点与坐标图，提醒仅以虚线阈值圈和当前点强调表达越限，底部不显示提醒文字或阈值数字。

## 保证范围

当前检查覆盖已定义场景与输入范围，代表原型设计状态，不代表已实现实车报警逻辑。任意新增数字格式、长歌曲名、长状态文案或新图形仍需独立检查；原型不是所有未来数据的排版保证。单测不能替代视觉检查。

## 预览与后续修改

打开 <http://127.0.0.1:8769/prototype.html?page=multi&scenario=multiple>。在表盘下方选择“状态场景”；08 选择轮位，09 / 10 / 11 编辑提醒限值。共享报警样例位于 <http://127.0.0.1:8769/components.html#alerts>。

后续改场景与示例值使用 `scenarios.js`，改基础角色使用 `foundations.js`，改共享表现使用 `components.js`，改构图使用 `nc-gauges.js`。
