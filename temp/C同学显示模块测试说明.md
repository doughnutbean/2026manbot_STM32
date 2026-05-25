# C同学显示模块测试说明（修订版 v3 — ILI9341 TFT）

> **重大变更**：屏幕型号从 I2C OLED 更正为 **ILI9341 TFT 彩屏（240×320）**，
> 接口从 I2C（PB6/PB7）改为 **FSMC 并口（8080）**。
> 驱动代码已完全重写，参考野火指南者官方 ILI9341 例程。

## 一、硬件确认

**屏幕直接插在板载 TFT 排母上即可，无需额外接线。**

| 信号 | STM32 引脚 | 说明 |
|------|-----------|------|
| FSMC_D0~D15 | PD14,PD15,PD0,PD1, PE7~PE15, PD8~PD10 | 16 位数据总线 |
| LCD-CS | PD7 (FSMC_NE1) | 片选 |
| LCD-RD | PD4 (FSMC_NOE) | 读使能 |
| LCD-WR | PD5 (FSMC_NWE) | 写使能 |
| LCD-DC | PD11 (FSMC_A16) | 命令/数据选择 |
| LCD-RST | PE1 | 硬件复位 |
| LCD-BK | PD12 | 背光控制 |

## 二、Keil 工程配置（必须操作！）

新文件需要加入 Keil 工程：

1. 在 Keil 左侧 Project 窗口中，右键 **App** 组 → **Add Existing Files**
2. 选择 `App/Src/mb_display_ili9341.c`
3. 确认 `stm32f10x_fsmc.c` 已在 Drivers 组中（野火标准库默认包含）

## 三、测试步骤

### 步骤 1：确认代码
- `App/Src/mb_tasks.c` 顶部 include 了 `mb_display_ili9341.h`
- `MB_DEBUG_LOG_ENABLE = 1U`（默认已开，观察日志）
- `MB_TEST_INJECT_ENABLE = 1U`（默认已开，自动注入命令）

### 步骤 2：编译烧录
- **Rebuild** → 确保 0 error, 0 warning
- **Download** 到板子

### 步骤 3：上电观察
1. **PB0 心跳灯** 约 500ms 闪烁 → RTOS 正常
2. **TFT 屏幕** 上电约 2 秒后应显示**全白**（启动占位图）
3. 之后每 2 秒自动切换表情颜色

### 步骤 4：串口验证
- 运行 `python man.py`（PA9, 115200 8N1）
- 预期日志：
  ```
  [DISPLAY] ILI9341 init start...
  [DISPLAY] ILI9341 init ok
  [VOICE] inject cmd
  [MAIN] recv voice cmd
  [DISPLAY] draw
  ```

## 四、预期表情显示

| 命令 | 屏幕颜色 |
|------|---------|
| 启动 | 全白 |
| HAPPY | 全绿 |
| SAD | 全蓝 |
| SLEEPY | 全黄 |
| CONFUSED | 全品红（粉紫） |

## 五、故障排查

| 现象 | 可能原因 | 解决 |
|------|---------|------|
| 编译报错找不到 `stm32f10x_fsmc.h` | FSMC 库未加入工程 | 在 Keil 的 Drivers 组添加 `stm32f10x_fsmc.c` |
| 屏幕全黑 | 背光未亮 | 万用表量 PD12 是否为低电平 |
| 屏幕白屏不切换 | `MB_TEST_INJECT_ENABLE` 未开 | 确认宏为 1U 后重新编译 |
| 串口无输出 | 串口接线问题 | 确认 PA9→TTL-RXD, GND→GND |
