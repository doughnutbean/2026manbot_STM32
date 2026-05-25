# Manbot 工程结构（StdPeriph 版）

以下结构用于你当前 `STM32F103VET6 + Keil + STM32F10x_StdPeriph_Lib_V3.5.0` 的组织方式。

## 1) 推荐目录

- `Core/Inc`：系统级头文件（`main.h`、中断、系统配置）
- `Core/Src`：系统级源文件（`main.c`、`stm32f10x_it.c` 等）
- `App/Inc`：业务接口（协议、任务接口）
- `App/Src`：业务实现（主控、任务、状态机）
- `Drivers/CMSIS/CoreSupport`：`core_cm3.*`
- `Drivers/CMSIS/DeviceSupport/ST/STM32F10x`：`stm32f10x.h`、启动文件、system 文件
- `Drivers/STM32F10x_StdPeriph_Driver/inc`：标准库头文件
- `Drivers/STM32F10x_StdPeriph_Driver/src`：标准库源码
- `Drivers/Middleware/FreeRTOS`：FreeRTOS 内核与 CMSIS-RTOS 适配
- `MDK-ARM`：可选（散装脚本、sct、批处理）
- `Doc`：接口契约、引脚分配、阶段文档

## 2) 当前你仓库已处理项

1. 已切换 `main.h` 到标准库头：`stm32f10x.h`
2. 已把 `main.c` 的启动流程改为 StdPeriph 兼容（去 HAL 初始化）
3. 已创建标准库工程常用目录骨架（`Drivers/CMSIS/...`、`Drivers/STM32F10x_StdPeriph_Driver/...` 等）

## 3) 你下一步需要拷贝的库文件（从你的固件库路径）

从 `E:\file\school\课程\嵌入式与FPGA试验\野火【STM32F103开发板-指南者】资料\1-程序源码_教程文档\1-[野火]《STM32库开发实战指南》(标准库源码)【优先学习】\1-书籍配套例程-F103VE指南者_20240202\0【固件库】STM32F10x_StdPeriph_Lib_V3.5.0\【固件库】STM32F10x_StdPeriph_Lib_V3.5.0` 拷贝：

- `Libraries\CMSIS\CM3\CoreSupport\*` -> `Drivers/CMSIS/CoreSupport`
- `Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\*` -> `Drivers/CMSIS/DeviceSupport/ST/STM32F10x`
- `Libraries\STM32F10x_StdPeriph_Driver\inc\*` -> `Drivers/STM32F10x_StdPeriph_Driver/inc`
- `Libraries\STM32F10x_StdPeriph_Driver\src\*` -> `Drivers/STM32F10x_StdPeriph_Driver/src`

## 4) Keil Include Path 建议

至少加入：

- `.\Core\Inc`
- `.\App\Inc`
- `.\Drivers\CMSIS\CoreSupport`
- `.\Drivers\CMSIS\DeviceSupport\ST\STM32F10x`
- `.\Drivers\STM32F10x_StdPeriph_Driver\inc`
- `.\Drivers\Middleware\FreeRTOS\include`
- `.\Drivers\Middleware\FreeRTOS\portable\RVDS\ARM_CM3`

## 5) 关于旧目录

当前已有 `Drivers/STM32F1xx_HAL_Driver`、`Drivers/FreeRTOS` 目录。若你确定项目完全转 StdPeriph，可在 Keil 中移除 HAL 相关组并停用对应 Include 路径，避免混用。
