# Components & foundations

[English](DESIGN_SYSTEM.en.md) | 简体中文

这套规范以当前仓库原型为事实源，沿用早期 Figma 的 Foundations、Readout、Action、Gauge Shell 思路，补充最近确认的控件与圆屏规则。交互示例在 [components.html](components.html)；浏览器入口为 <http://127.0.0.1:8769/components.html>。

## 分层与事实源

- `foundations.js`：颜色用途、字体角色、字号、圆屏几何、间距、描边与点击尺寸。
- `foundations.css` / `foundations.json`：从基础定义生成的浏览器样式变量和可读数据；不要手改。
- `components.js` / `components.css`：共用 SVG 组件、属性、状态和反馈。
- `nc-gauges.js`：16 个页面的组合与交互；页面坐标负责各自构图。
- `art.js`：基础矢量图形，颜色与标签字体通过语义绑定引用基础定义。
- `components.html` / `catalog.js`：调用实际共享组件的交互规范页，帮助检查尺寸与状态。

共同基础和控件改在对应共享层；特有刻度、坐标图、车辆线稿与分区构图保留在页面和图形层。

## 基础规则

### 颜色

- `text` / `muted`：主要读数与辅助文字。
- `amber`：选中、强调和当前项，配合下划线、描边或文字。
- `red`：报警；Readout 的 warning 状态始终使用报警色。
- `green` / `blue`：正常状态与连接信息。
- `night`：夜间读数颜色；日夜亮度属于独立交互状态。
- `face` / `panel` / `line`：表盘底色、控件表面和结构线；外页面板使用独立的 viewer 角色。

不要仅靠颜色表达状态。报警需要读数或原因文案。当前选择需要标记，断连使用 `—` 和状态信息。

### 字体与尺寸

数字使用 Barlow Condensed，标签、单位和状态使用 IBM Plex Mono；中文歌词使用系统字体。

- `hero`：180 母版像素，对应 90px 显示。
- `eccentric`：144，对应 72px。
- `analog`：96，对应 48px。
- `compact`：76，对应 38px。
- `mediaTitle`：60，对应 30px，专用于音乐页歌曲名称。
- `title` / `label` / `support` / `small`：28 / 26 / 24 / 22，对应 14 / 13 / 12 / 11px。

字号和字体角色定义在同一个基础文件中。优先使用现有角色，新增角色时说明用途；不要按页面复制一套配色和字号。

### 圆屏和间距

- 母版为 720×720，显示参考为 360×360，圆心为 (360,360)。坐标、尺寸和半径均写母版值。
- 内容检查边界半径为 342；按钮点击区域的四个角须位于半径 318 的交互安全区内。
- 点击区域至少为 88×88 母版像素，即 44×44px 显示。组件会拒绝过小、非有限数值及越界区域。
- 共享间距角色为 8 / 16 / 24 / 32 母版像素。独立仪表与读数分区建议至少留出 32 母版像素，即 16px 显示间距。
- 组件区域不能覆盖其他数据区。按 360 原尺寸检查数字最大位数、长文案、报警和断连状态。
- 机械表的指针位于印刷式表盘文字之上，可以扫过名称、单位或刻度；这种层叠不单独判定为布局缺陷。独立数字读数、状态提示和按钮仍须保持清晰，不能被长期遮挡。02 当前细指针与底层名称的层叠予以保留。

## 组件与属性

### Readout

属性包括 `x`、`y`、`label`、`value`、`unit`、`size`、`state`，以及正常状态的可选强调色 `color`。

- `size`：`primary`、`compact`、`eccentric`、`analog`。
- `state`：`normal`、`warning`、`disconnected`。
- 标签、数字和单位共用中心轴；不同尺寸具有固定的纵向间距。
- `warning` 强制使用报警色；`disconnected` 显示 `—`，不显示传入的有效值。

```js
const U = NC_COMPONENTS;
U.readout({
  x: 200, y: 220, label: 'OIL TEMP', value: '104', unit: '°C',
  size: 'primary', state: 'normal'
});
```

Figma 的 `Size / State` 和 `Label / Value / Unit` 对应这里的尺寸、状态与文本属性。`metric(...)` 适配入口支持最后一个 `state` 参数；无数据状态优先于报警。

### Action 与辅助控件

Action 属性包括位置、宽度、文字、动作名、`variant` 和 `disabled`。

- `variant`：`secondary`、`primary`、`ghost`、`danger`。
- 不可用状态独立于视觉变体，使用 `aria-disabled` 和 `tabindex=-1`，不响应点击或键盘。
- 按压、悬停与键盘焦点只改变颜色、描边或可见标记，不平移或缩放控件；反馈时长为 120ms，支持减少动画偏好。
- 音乐主键为 52px 圆形，辅助切歌图标保持 44px 点击区域。
- 开关同步显示拨片、ON / OFF 和 `aria-checked`。
- TPS / B.P 为整体切换区域；琥珀色与下划线共同标记当前项。

```js
U.actionButton({
  x: 240, y: 260, width: 240, label: 'CONFIRM', act: 'confirm',
  variant: 'primary', disabled: false
});
```

`roundButton`、`mediaButton`、`toggleControl` 和 `readoutControl` 是同一共享组件层的专用控件。动作仍由页面通过 `data-act` 接收，Enter / Space 与点击使用同一动作。

### Gauge Shell 与页面组合

`shell()` 复用表盘底色、圆形边框和顶部定位标记；页面裁切和圆心遵循基础几何。底部不显示装饰署名，必要的断连信息由页面传入。

页面组合保留不同表现形式：01 / 02 / 04 / 12 的圆形主表、05 的偏心分区、06 / 07 的多项分区，以及 10 的同心坐标图。共用组件保障字体、读数和控件一致，构图继续适应各类数据。


## 修改与验证流程

修改基础定义后生成 CSS 和 JSON，再验证组件约束；完整导出命令同时生成界面资源：

```bash
cd ui-prototype
npm run export:foundations
npm test
npm run export
```

组件测试覆盖基础同步、44px 点击区域、圆边约束、不可用状态、Readout 的报警 / 断连语义、图形颜色绑定，以及所有页面在主要设置下能否渲染；仍需浏览器验证页面图形与实际排版。


浏览器检查组件规范页的状态切换和控件，再检查受影响的表盘在 360 原尺寸下的文字、数字、单位、圆边和操作。有意调整视觉时同步导出 SVG、PNG 和总览，并更新中英文规范。
