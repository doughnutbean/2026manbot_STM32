# 阶段一 Keil 集成说明（成员 D）

## 1. 需要加入工程的文件

请在 Keil 工程中加入以下源文件：

- `Core/Src/main.c`
- `App/Src/mb_app.c`
- `App/Src/mb_tasks.c`

并确保以下头文件目录在 Include Path 中：

- `Core/Inc`
- `App/Inc`
- `Drivers/CMSIS/RTOS2/Include`
- `Drivers/STM32F1xx_HAL_Driver/Inc`
- `Drivers/CMSIS/Device/ST/STM32F1xx/Include`
- `Drivers/CMSIS/Include`

## 2. 宏与组件要求

1. 目标芯片选择 `STM32F103VET6`。
2. 启用 HAL 和 CMSIS-RTOS2（FreeRTOS 适配层）。
3. 确保工程中有 FreeRTOS 内核源码并开启 `osKernelInitialize/osThreadNew/osMessageQueueNew` 所需组件。

## 3. 入口流程（已在 main.c 实现）

1. `HAL_Init()`
2. `SystemClock_Config()`
3. `osKernelInitialize()`
4. `MB_AppInit()`
5. `MB_AppStartTasks()`
6. `osKernelStart()`

## 4. 联调自检清单

1. 工程可编译通过（无未定义 RTOS 符号）。
2. 启动后不进入 `Error_Handler()`。
3. 任务创建成功（可在调试器查看线程列表）。
4. 后续由 A/B/C 分别替换各任务占位逻辑，接口不改。
