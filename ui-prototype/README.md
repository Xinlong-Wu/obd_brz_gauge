# MX-5 NC / Circuit Instruments

[English](README.en.md) | 简体中文

MX-5 NC / JDM 圆形车载仪表的视觉与交互原型，包含 16 个界面及 8 个导出状态。720×720 母版，360×360 原尺寸预览；所有数据和连接操作均为演示。导入仓库的设计基线为 v10。

## 修改与预览

后续修改以本目录为准，直接编辑 `nc-gauges.js`、`prototype.css`、`art.js` 和 `prototype.html`。预览服务固定读取当前仓库中的本目录；保存后刷新网页即可看到修改。

在仓库根目录运行，只需要 Python 3.9 或更高版本：

```bash
python3 ui-prototype/start-preview.py
```

打开 <http://127.0.0.1:8769/prototype.html>。直接进入 05 页可使用 <http://127.0.0.1:8769/prototype.html?page=oilpress>。点击“360 原尺寸”检查圆边、间距和文字遮挡。

服务在后台运行。重复启动会检查服务对应的目录；停止或更换端口：

```bash
python3 ui-prototype/start-preview.py --stop
python3 ui-prototype/start-preview.py --port 8770
```

`/__preview__` 返回实际服务目录和进程信息，方便确认预览来源。日志和 PID 位于 `.preview/`，不入库。服务为静态文件设置 `Cache-Control: no-store`，避免缓存旧界面。也可以直接在浏览器中打开本目录的 `prototype.html`，资源均为本地多文件。

## 页面

1. 挡位 / 转速：分段转速弧、中央挡位、辅助油温与油压。
2. 增压 / 真空：压力圆弧、细指针、大读数。
3. 冷却液温度：纵向温度柱、大读数和状态。
4. 发动机油温：温度弧与读数共用圆心，40–140°C 示意刻度。
5. 发动机油压：左侧偏心机械表、独立右侧读数与状态。
6. 三项综合监测：主增压、油温和油压。
7. 多项综合监测：油压、油温、电压、水温、增压和 TPS / B.P 切换。
8. 四轮胎压与胎温：敞篷跑车线稿、四轮定位和低压状态。
9. 车速与巡航：当前车速、巡航状态和设定车速。
10. G 力：同心圆、十字刻度、当前坐标点及双轴读数。
11. 心率：实时读数、低值、峰值、一分钟统计及波形。
12. 机械压力表：居中针轴、270°刻度和独立压力读数。
13. 音乐：歌曲、歌手、歌词、进度、上一首、播放 / 暂停、下一首。
14. 日夜亮度：日间和夜间独立亮度、加减和模式切换。
15. 蓝牙与遥控：连接状态、可发现开关、遥控键盘、配对和清除配对确认。
16. 配对码：分组配对码、确认和返回。

## 当前设计约束

- 暖白读数、琥珀强调、红色报警；数字用 Barlow Condensed，标签用 IBM Plex Mono。
- 音乐主键为 52px 深色圆形、琥珀细圈；辅助图标点击区域至少 44×44px，按压反馈不移动控件。
- 07 使用紧凑的 `TPS / B.P` 标签和整体点击区域，点击标签、数字或留白切换；当前项保留琥珀色和下划线。
- 05 的偏心表与数字保持间距，右侧压力、单位和状态统一对齐；正常和报警数字在 360 预览下距仪表约 22px。
- 12 的标题融入刻度环内部，仅留小 `BOOST` 标识；针轴和刻度居中，数字无底框。
- 10 内圈为 1g、外圈为 2g，仅显示当前坐标点，不显示轨迹。
- 表盘不显示装饰性的 `NC-01 / ROADSTER` 署名；断连时保留 `NO SIGNAL` 状态。

## 导出与文件

- `prototype.html` / `prototype.css`：多文件网页入口与样式。
- `nc-gauges.js`：页面、示例数据、SVG 渲染和交互。
- `art.js`：基础矢量图形；`fonts/`：本地字体及 SIL OFL 许可。
- `vectors-text/`：24 张带文字节点及嵌入字体的 SVG。
- `vectors/`：24 张文字转轮廓的 SVG，适合导入 Figma。
- `previews/`：24 张 PNG；`overview.png` / `overview.svg`：完整总览。
- `design-manifest.json`：页面、状态、色彩与字体索引。

修改源文件后，重新导出并提交对应的 SVG、PNG 和总览。浏览器预览本身不需要安装导出依赖；导出需要 Node.js 20.9 或更高版本及 Python：

```bash
cd ui-prototype
npm install
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
npm run export
```

导出依次执行 `export-vectors.cjs`、`outline-type.py`、`rasterize.cjs`，路径均相对于本目录。中文歌词转轮廓需要 CJK 字体，脚本自动查找常见系统字体，也可以指定字体文件：

```bash
UI_CJK_FONT=/path/to/cjk-font.ttc npm run export
```

验证 JS 语法后，按 360 原尺寸检查圆边、数字、文字及点击区域；修改交互时验证对应操作与正常 / 报警 / 断连状态。

## Figma 参考

<https://www.figma.com/design/4Ah7vldYa2qIrVheZ0Xzc8> 为早期 6 个核心表盘的参考文件。完整的当前版本以仓库中的原型和 SVG 为准。
