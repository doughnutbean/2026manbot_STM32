# 阶段一接口契约（成员 D 维护）

## 1. 任务与队列拓扑

- `VoiceTask -> MainTask`：`voice_to_main_queue`，消息类型 `MB_VoiceEvent_t`
- `MainTask -> MotionTask`：`main_to_motion_queue`，消息类型 `MB_MotionEvent_t`
- `MainTask -> DisplayTask`：`main_to_display_queue`，消息类型 `MB_DisplayEvent_t`
- `MainTask -> TtsTask`：`main_to_tts_queue`，消息类型 `MB_TtsEvent_t`

## 2. 统一指令枚举（A/B/C/D 必须对齐）

定义文件：`App/Inc/mb_protocol.h`

- 唤醒与休眠：`MB_CMD_WAKEUP`、`MB_CMD_SLEEP`
- 运动控制：`MB_CMD_FORWARD`、`MB_CMD_BACKWARD`、`MB_CMD_TURN_LEFT`、`MB_CMD_TURN_RIGHT`、`MB_CMD_SIT`、`MB_CMD_WAVE`
- 表情触发：`MB_CMD_EXPRESSION_HAPPY`、`MB_CMD_EXPRESSION_SAD`、`MB_CMD_EXPRESSION_SLEEPY`、`MB_CMD_EXPRESSION_CONFUSED`

语音组（成员 A）输出必须是上述枚举，禁止直接向其他模块发送私有字符串。

## 3. 状态机定义（系统主状态）

- `MB_STATE_IDLE`
- `MB_STATE_AWAKE`
- `MB_STATE_RECOGNIZING`
- `MB_STATE_EXECUTING`
- `MB_STATE_REPLYING`
- `MB_STATE_LOW_POWER`

状态由主控任务统一管理，其他任务仅通过消息驱动行为。

## 4. 模块职责边界

- A 负责：语音识别/合成驱动、识别状态机、输出 `MB_VoiceEvent_t`
- B 负责：动作组与步态、消费 `MB_MotionEvent_t`
- C 负责：表情资源与显示驱动、消费 `MB_DisplayEvent_t`
- D 负责：FreeRTOS 框架、主状态机、任务通信
- E 负责：串口可视化调试与联调测试

## 5. 下一步接入建议

1. 在 `Core/Src/main.c` 中 `osKernelInitialize()` 后调用 `MB_AppInit()`
2. 在 `osKernelStart()` 前调用 `MB_AppStartTasks()`
3. A/B/C 在各自任务函数中替换占位逻辑，保持消息结构不变
