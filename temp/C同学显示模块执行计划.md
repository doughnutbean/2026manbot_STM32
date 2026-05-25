# C同学显示模块执行计划（阶段一 — 已完成）

## 1. 背景与目标

依据当前项目文档与分工，C同学负责显示模块：消费 `MB_DisplayEvent_t`，完成显示驱动与表情渲染链路。

屏幕型号确认：**ILI9341 TFT 彩屏（240x320）**，走 **FSMC 并口**。
驱动代码位于 `App/Src/mb_display_ili9341.c`，独立于任务代码。

## 2. 约束

- 不改变任务/队列拓扑：保持 `Main -> Display` 队列模型
- 不修改 `Drivers/` 第三方库源码（仅添加 `stm32f10x_fsmc.c` 到工程）
- 不复用保留引脚：PB0（心跳）、PA9（日志）
- FSMC 引脚与 A/B 同学无冲突

## 3. 执行步骤（实际完成情况）

| 步骤 | 内容 | 状态 |
|------|------|------|
| 1 | 新建 `App/Src/mb_display_ili9341.c` 和 `App/Inc/mb_display_ili9341.h` | 完成 |
| 2 | 配置 FSMC（Bank1 NORSRAM1），初始化 ILI9341 寄存器序列 | 完成 |
| 3 | 实现 `ILI9341_FillScreen()` 全屏填充函数 | 完成 |
| 4 | 实现 `MB_DisplayShowBootPattern()` — 上电白屏 | 完成 |
| 5 | 在 `MB_TaskDisplay` 中接入初始化逻辑 + 延时等待串口 | 完成 |
| 6 | 接入 4 种表情映射：HAPPY(绿)/SAD(蓝)/SLEEPY(黄)/CONFUSED(品红) | 完成 |
| 7 | 其余命令回退到启动图案 | 完成 |
| 8 | 记录执行日志与结果 | 完成 |

## 4. 验收标准（本轮）— 全部通过

- 代码结构不破坏现有任务框架（独立文件，仅修改 mb_tasks.c 显示部分）
- `MB_TaskDisplay` 能完成初始化并显示基础图样（白屏）
- 收到显示事件后，能切换 4 类占位图
- Drivers 仅添加 `stm32f10x_fsmc.c` 到工程，未修改源码

## 5. 实际引脚分配

| 信号 | STM32 引脚 |
|------|-----------|
| FSMC_D0~D3 | PD14, PD15, PD0, PD1 |
| FSMC_D4~D12 | PE7~PE15 |
| FSMC_D13~D15 | PD8, PD9, PD10 |
| LCD-CS | PD7 (FSMC_NE1) |
| LCD-RD | PD4 (FSMC_NOE) |
| LCD-WR | PD5 (FSMC_NWE) |
| LCD-DC | PD11 (FSMC_A16) |
| LCD-RST | PE1 |
| LCD-BK | PD12 |

## 6. 调试发现

1. **初始误判**：最初的代码按 I2C OLED（SSD1306/SH1106）编写，屏幕完全不亮
2. **根因**：确认屏幕型号为 ILI9341 TFT，接口完全不同（FSMC vs I2C）
3. **修复**：新建独立驱动文件 `mb_display_ili9341.c`，参考野火官方 ILI9341 例程重写
4. **编译问题**：`stm32f10x_fsmc.c` 需手动添加到 Keil StdPeriph 组

## 7. 下一步（阶段二）

- 表情从纯色填充升级为**实际图案/文字**（绘制眼睛、嘴巴等）
- 支持动画过渡
- 如需显示文字，引入字库
