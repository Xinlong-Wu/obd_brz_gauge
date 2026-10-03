# 编码与注释风格

[English](CODE_STYLE.en.md) | 简体中文

本仓库的 C 代码风格规范。适用于**新代码与手写模块**;目标是可读、可测、
可长期维护。中文文档是事实源,英文版(`CODE_STYLE.en.md`)1:1 镜像。

## 适用范围(先读这个)

| 区域 | 是否适用 | 原因 |
|------|----------|------|
| `main/export_path/ui.c`(生成段)、`screens/*.c`、`ui_theme_generated.c`、`theme_assets/` | ❌ 不适用 | SquareLine/脚本生成物,重导出会覆盖(AGENTS.md 红线) |
| `main/export_path/ui_ext.c`、`ui_disp_item.c` 等手写扩展区 | ✅ 适用 | 手写区,随改随规范 |
| `main/app_obd_dsp/`、`main/bsp_obd_dsp/`、`main/theme_engine/` | ✅ 适用 | 手写业务/BSP 逻辑 |
| `simulator/`、`tools/`、`tests/` | ✅ 适用 | 工具与测试代码同标准 |

对既有不符合规范的代码:**渐进改造**,不做一次性大重写——顺手改到的模块
按本规范补齐,并在同一提交里补测试。

## 纯逻辑头文件模式(可测性的核心)

凡是"输入 → 输出、无副作用"的逻辑(解码、区间判定、步进/平滑、掩码
组合、布局计算),抽成 `*_logic.h`,全部 `static inline`:

```c
/* ui_disp_item_logic.h — 数据项纯逻辑(无 LVGL / ESP-IDF 依赖) */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** 自适应步进:diff ≤ threshold 每次走 ±1;diff > threshold 按 ~1/3 比例逼近。 */
static inline int32_t ui_disp_item_anim_step_i32(int32_t displayed,
                                                 int32_t target,
                                                 int32_t threshold)
{
    ...
}
```

规则:

- 头文件**只**依赖 `<stdint.h>` / `<stdbool.h>` / 本仓库其他 `_logic.h`
- 不 include LVGL、ESP-IDF、FreeRTOS、BSP 头——这样 `tests/` 能直接编译
  断言(见 `tests/test_ui_disp_item.c` 的用法)
- 落地页(`.c`)include 该头并在 UI/IO 层调用;`static` 实现优先迁移到
  `_logic.h` 而不是留在 `.c` 里
- 每个新 `_logic.h` 至少配一个单测用例(改动才敢放手做)

## 注释

- **中文注释为主**,讲"为什么、约束、坑",不复述代码在做什么
- 模块头:文件顶部一段块注释,写职责、边界、与谁协作(参考
  `simulator/src/main.c`、`bsp_stubs.c` 的写法)
- 函数:公开 API 用 `/** ... */` Doxygen 风格,中文一句话职责 + 参数/返回
  的关键约束;`static` 小函数可不注释(名字即注释)
- 数值常量带单位(`0.1 bar`、`×100`、`ms`);魔法数要么命名要么注释来历
- 外部协议/硬件时序的注释里给出处(ISO PID、datasheet 页码、CAN 帧号)
- 不写"翻译代码"式注释(`i++ // i 加一`),不写对 review 者说话的注释

## 代码组织

- 函数短小、单一职责;一个函数超过 ~60 行考虑拆分(生成物除外)
- 公开 API 入参做防御式检查(NULL / 区间),非法输入早返回带明确语义;
  `static` 内部函数可依赖调用方保证的不变量,不必层层判空
- 命名:`snake_case`;模块前缀统一(`disp_item_*`、`theme_*`、
  `espnow_link_*`);布尔用 `is_/has_/should_` 前缀;不要 `Temp2`、
  `flag1` 这类无信息命名
- 状态放 `static` 单例结构体(如 `s_ctx` / `s_st`),集中定义在文件头部;
  不要散落的关联全局变量
- 错误路径:ESP-IDF 返回 `esp_err_t` 并检查;资源成对释放
  (create/delete、mmap/munmap、lock/unlock)写在同一函数视野内

## 模块伴生文档(可选)

复杂模块(状态机、驱动、协议解析)可在同目录放 `<module>.md`,写**实现
细节**(内部状态、时序图、已知限制)。与 `docs/` 的分工:

- `<module>.md` = 怎么实现的(给改代码的人)
- `docs/*.md` = 怎么用 / 事实源清单(给用的人,AGENTS.md 映射表管辖)

两处不要重复记载同一事实,数字冲突时以代码为准并改文档。

## 提交与验证

- conventional commits(`feat:` `fix:` `docs:` `build:` `chore:`),提交信息
  英文、正文可中文,写"为什么"
- 行为改动必须三绿:单测(`ctest`)、截图回归(`tools/sim_regress.py`,
  有意改 UI 则先 `--update-goldens`)、固件构建(Docker)
- 文档同步规则见 [AGENTS.md](../AGENTS.md) 映射表
