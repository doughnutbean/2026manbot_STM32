# 2026manbot_STM32 项目整体架构说明

## 1. 文档目的

本文基于当前代码与现有文档，给出本项目的整体架构视图，帮助成员快速理解：

- 工程启动路径
- 任务与消息通信关系
- 模块边界与职责
- 当前阶段能力与后续扩展方向

适用范围：阶段一基线（STM32F103VE + CMSIS-RTOS2/RTX5）。

---

## 2. 项目定位与技术栈

- 目标芯片：STM32F103VE（Cortex-M3）
- 开发工具：Keil MDK5（ARMCC5 / ARM-ADS 工具链）
- 实时系统：CMSIS-RTOS2 + RTX5
- 代码组织：Core（系统入口）+ App（业务框架）+ Drivers（外设库/RTOS实现）

参考：
- `manbot.uvprojx`（Target 1）
- `Core/Src/main.c`
- `App/Src/mb_app.c`
- `App/Src/mb_tasks.c`

---

## 3. 目录与分层架构

### 3.1 Core 层（系统启动与板级初始化）

- `Core/Src/main.c`
  - 系统时钟初始化
  - 板级心跳灯初始化（PB0）
  - 调试串口初始化（USART1 TX=PA9）
  - RTOS 内核初始化与启动

### 3.2 App 层（业务编排与任务框架）

- `App/Inc/mb_protocol.h`
  - 系统命令枚举（如前进/后退/转向/唤醒/休眠）
  - 系统状态枚举（IDLE/AWAKE/EXECUTING 等）
  - 任务间事件结构体（Voice/Motion/Display/TTS）
- `App/Inc/mb_app.h`
  - 全局上下文 `MB_AppContext_t`
  - 队列句柄、状态锁、系统状态的集中定义
- `App/Src/mb_app.c`
  - 创建消息队列与互斥锁
  - 创建并启动 6 个任务线程
- `App/Src/mb_tasks.c`
  - 各任务主循环
  - Main 任务中的命令分发逻辑
  - 心跳与可选调试/注入机制

### 3.3 Drivers 层（基础库与中间件）

- CMSIS、RTX5、STM32F10x 标准外设库
- 作为平台依赖层，通常不作为业务迭代改动入口

### 3.4 文档层（规则与验收）

- `Doc/阶段一代码结构详解.md`
- `Doc/0-1历史版本/phase1_interface_contract.md`
- `Doc/0-1历史版本/phase1_keil_integration.md`
- `Doc/阶段一验收报告.md`
- `Doc/阶段一引脚分配总表.md`

---

## 4. 启动时序与生命周期

当前启动流程如下：

1. `SystemClock_Config()`
2. `BoardLed_Init()`
3. `DebugUart_Init()`
4. `osKernelInitialize()`
5. `MB_AppInit()`
6. `MB_AppStartTasks()`
7. `osKernelStart()`

说明：

- 初始化顺序体现了“硬件准备 -> RTOS准备 -> 应用对象构建 -> 任务启动”的依赖关系。
- `main` 最后的空循环仅作为兜底，正常执行路径应由 RTOS 调度各任务。

---

## 5. 任务与消息通信架构

### 5.1 任务拓扑

- `MB_TaskVoice`：语音/输入源（支持测试注入）
- `MB_TaskMain`：中心分发与状态管理
- `MB_TaskMotion`：动作执行
- `MB_TaskDisplay`：显示执行
- `MB_TaskTts`：语音播报执行
- `MB_TaskHeartbeat`：系统活性指示（LED 翻转）

### 5.2 队列拓扑

- `voice_to_main_queue`：Voice -> Main
- `main_to_motion_queue`：Main -> Motion
- `main_to_display_queue`：Main -> Display
- `main_to_tts_queue`：Main -> TTS

### 5.3 核心消息流

`Voice -> Main -> Motion/Display/TTS`

- Voice 产生 `MB_VoiceEvent_t`
- Main 解析命令并构造：
  - `MB_MotionEvent_t`
  - `MB_DisplayEvent_t`
  - `MB_TtsEvent_t`
- 下游任务仅消费对应事件，不跨模块直接调用

这一结构使系统具备较好的解耦性：输入源可替换、执行端可独立演进、主控保持统一路由。

---

## 6. 状态管理与并发模型

- 全局状态由 `g_mb_app.state` 表示
- 通过 `state_lock` 互斥锁保护状态写入
- 统一入口函数：`MB_SetState(...)`

并发策略要点：

- 任务之间通过消息队列传递数据，避免共享可变数据的直接读写
- 主状态由 Main 任务协同更新，降低状态竞争风险
- 心跳节拍 `g_mb_heartbeat_ticks` 用于系统活性和测试注入时间戳

---

## 7. 硬件与引脚约束（阶段一）

公共保留引脚：

- PB0：心跳 LED
- PA9：USART1 调试日志 TX

约束原则：

- 阶段一不复用 PB0/PA9
- 新增外设前先核对引脚分配文档并同步更新

参考：`Doc/阶段一引脚分配总表.md`

---

## 8. 调试与验证机制

调试相关宏位于 `App/Src/mb_tasks.c`：

- `MB_DEBUG_LOG_ENABLE`
- `MB_TEST_INJECT_ENABLE`

默认验收建议：

- 两者默认关闭（0）用于基线验收
- 联调阶段可临时打开（1）观察消息链路

验收重点：

- Keil Rebuild 结果为 0 error / 0 warning
- 心跳灯约 500ms 翻转
- 任务/队列对象创建成功

参考：`Doc/阶段一验收报告.md`

---

## 9. 架构边界与协作规范

建议遵守以下边界：

- 扩展协议优先在 `mb_protocol.h` 增量演进，避免私有字符串协议扩散
- 新业务逻辑优先接入 Main 路由，不跨任务硬耦合
- 状态变更经状态锁路径处理，避免无保护写入
- 优先改动 App/Core；除非明确要求，不修改 Drivers
- 不将生成目录（Objects/Listings）作为源码修改目标

---

## 10. 当前成熟度评估（阶段一）

已具备：

- 可运行的 RTOS 多任务骨架
- 稳定的任务-队列通信通路
- 可观察的硬件活性信号（心跳灯）
- 可开关的调试日志与测试注入机制

仍待阶段二/三完善：

- 真实语音输入驱动对接
- 动作/显示/TTS 设备驱动与资源层落地
- 更细粒度的异常处理、超时与容错策略

---

## 11. 建议的演进路线

1. 在协议层为新能力先定义命令/事件，再进入任务实现。
2. 为 Motion/Display/TTS 增加驱动适配层，减少任务函数内硬编码。
3. 将调试开关与板级配置集中到统一配置头文件（如 board/config）。
4. 增加“消息链路自检”与“状态机行为”回归检查项，形成固定联调清单。

---

## 12. 快速阅读顺序（新成员）

1. `App/Inc/mb_protocol.h`
2. `App/Inc/mb_app.h`
3. `App/Src/mb_app.c`
4. `App/Src/mb_tasks.c`
5. `Core/Src/main.c`
6. `Doc/阶段一代码结构详解.md`

以上顺序可在最短时间内建立“协议 -> 框架 -> 任务 -> 启动 -> 文档约束”的完整心智模型。
