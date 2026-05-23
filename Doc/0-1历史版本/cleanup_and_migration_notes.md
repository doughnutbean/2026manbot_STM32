# 工程精简与迁移记录（2026-05-21）

## 已完成精简

1. 保留核心目录：`App`、`Core`、`Drivers`、`Doc`、Keil 工程文件。
2. 删除空目录：`Drivers/STM32F1xx_HAL_Driver`、`Drivers/FreeRTOS`、`Drivers/BSP`、`Drivers/Middleware`、`MDK-ARM`、`ProjectConfig`。
3. 保留 Keil 产物目录：`Listings`、`Objects`（可继续用于编译输出）。

## 已完成库文件落地

从你的标准库路径复制到工程：

- `Drivers/CMSIS/CoreSupport`（`core_cm3.c/.h`）
- `Drivers/CMSIS/DeviceSupport/ST/STM32F10x`
  - `stm32f10x.h`
  - `system_stm32f10x.c/.h`
  - `startup_stm32f10x_hd.s`（仅保留 F103VE 需要的 HD 启动文件）
- `Drivers/STM32F10x_StdPeriph_Driver/inc`（23 个头文件）
- `Drivers/STM32F10x_StdPeriph_Driver/src`（23 个源文件）

## 你在 Keil 里需要确认

1. 工程分组至少加入：
   - `Core/Src/main.c`
   - `Core/Src/system_stm32f10x.c`
   - `Core/Startup/startup_stm32f10x_hd.s`（或当前 Drivers 路径下该文件）
   - `App/Src/mb_app.c`
   - `App/Src/mb_tasks.c`
   - 按需加入 StdPeriph 对应 `.c`（先加 `rcc/gpio/usart/tim/i2c/spi/misc` 常用集）
2. 预编译宏建议：
   - `USE_STDPERIPH_DRIVER`
   - `STM32F10X_HD`
3. Include Path 至少包含：
   - `.\Core\Inc`
   - `.\App\Inc`
   - `.\Drivers\CMSIS\CoreSupport`
   - `.\Drivers\CMSIS\DeviceSupport\ST\STM32F10x`
   - `.\Drivers\STM32F10x_StdPeriph_Driver\inc`

