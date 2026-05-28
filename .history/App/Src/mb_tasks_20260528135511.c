#include "mb_app.h"
#include "mb_tasks.h"
#include <string.h>
#include "stm32f10x_gpio.h"

#ifndef MB_DEBUG_LOG_ENABLE
#define MB_DEBUG_LOG_ENABLE 0U
#endif

#ifndef MB_TEST_INJECT_ENABLE
#define MB_TEST_INJECT_ENABLE 0U
#endif

volatile uint32_t g_mb_heartbeat_ticks = 0U;
extern void MB_DebugLog(const char *text);

#if (MB_DEBUG_LOG_ENABLE == 1U)
#define MB_LOG(text) MB_DebugLog(text)
#else
#define MB_LOG(text) ((void)0)
#endif

static void MB_SetState(MB_SystemState_t state)
{
    if (g_mb_app.state_lock == NULL)
    {
        return;
    }

    if (osMutexAcquire(g_mb_app.state_lock, osWaitForever) == osOK)
    {
        g_mb_app.state = state;
        (void)osMutexRelease(g_mb_app.state_lock);
    }
}

static uint8_t MB_MapCommandToDisplay(MB_CommandId_t command, MB_DisplayEvent_t *display_event)
{
    display_event->command = command;
    display_event->expression_id = MB_EXPRESSION_NONE;
    display_event->animate = 0U;

    switch (command)
    {
    case MB_CMD_FORWARD:
    case MB_CMD_BACKWARD:
    case MB_CMD_WAVE:
    case MB_CMD_EXPRESSION_HAPPY:
        display_event->expression_id = MB_EXPRESSION_HAPPY;
        display_event->animate = 1U;
        return 1U;
    case MB_CMD_EXPRESSION_SAD:
        display_event->expression_id = MB_EXPRESSION_SAD;
        display_event->animate = 0U;
        return 1U;
    case MB_CMD_SIT:
    case MB_CMD_EXPRESSION_SLEEPY:
        display_event->expression_id = MB_EXPRESSION_SLEEPY;
        display_event->animate = 0U;
        return 1U;
    case MB_CMD_TURN_LEFT:
    case MB_CMD_TURN_RIGHT:
    case MB_CMD_EXPRESSION_CONFUSED:
        display_event->expression_id = MB_EXPRESSION_CONFUSED;
        display_event->animate = 1U;
        return 1U;
    default:
        return 0U;
    }
}

static void MB_HandleCommand(const MB_VoiceEvent_t *voice_event)
{
    MB_MotionEvent_t motion_event;
    MB_DisplayEvent_t display_event;
    MB_TtsEvent_t tts_event;
    (void)memset(&motion_event, 0, sizeof(motion_event));
    (void)memset(&display_event, 0, sizeof(display_event));
    (void)memset(&tts_event, 0, sizeof(tts_event));

    motion_event.command = voice_event->command;
    tts_event.source_command = voice_event->command;

    switch (voice_event->command)
    {
    case MB_CMD_FORWARD:
        motion_event.motion_group_id = 0x01;
        motion_event.duration_ms = 1500;
        tts_event.tts_id = MB_TTS_EXEC_FORWARD;
        break;
    case MB_CMD_BACKWARD:
        motion_event.motion_group_id = 0x02;
        motion_event.duration_ms = 1500;
        tts_event.tts_id = MB_TTS_EXEC_BACKWARD;
        break;
    case MB_CMD_TURN_LEFT:
        motion_event.motion_group_id = 0x03;
        motion_event.duration_ms = 900;
        tts_event.tts_id = MB_TTS_EXEC_TURN_LEFT;
        break;
    case MB_CMD_TURN_RIGHT:
        motion_event.motion_group_id = 0x04;
        motion_event.duration_ms = 900;
        tts_event.tts_id = MB_TTS_EXEC_TURN_RIGHT;
        break;
    case MB_CMD_SIT:
        motion_event.motion_group_id = 0x05;
        motion_event.duration_ms = 1000;
        tts_event.tts_id = MB_TTS_EXEC_SIT;
        break;
    case MB_CMD_WAVE:
        motion_event.motion_group_id = 0x06;
        motion_event.duration_ms = 1200;
        tts_event.tts_id = MB_TTS_EXEC_WAVE;
        break;
    case MB_CMD_SLEEP:
        tts_event.tts_id = MB_TTS_SLEEP_ACK;
        MB_SetState(MB_STATE_LOW_POWER);
        break;
    case MB_CMD_WAKEUP:
        tts_event.tts_id = MB_TTS_WAKE_ACK;
        MB_SetState(MB_STATE_AWAKE);
        break;
    case MB_CMD_EXPRESSION_HAPPY:
    case MB_CMD_EXPRESSION_SAD:
    case MB_CMD_EXPRESSION_SLEEPY:
    case MB_CMD_EXPRESSION_CONFUSED:
        tts_event.tts_id = MB_TTS_NONE;
        break;
    default:
        tts_event.tts_id = MB_TTS_UNKNOWN_CMD;
        break;
    }

    (void)MB_MapCommandToDisplay(voice_event->command, &display_event);

    if (motion_event.motion_group_id != 0U)
    {
        (void)osMessageQueuePut(g_mb_app.main_to_motion_queue, &motion_event, 0U, 0U);
    }
    if (display_event.expression_id != 0U)
    {
        (void)osMessageQueuePut(g_mb_app.main_to_display_queue, &display_event, 0U, 0U);
    }
    if (tts_event.tts_id != MB_TTS_NONE)
    {
        (void)osMessageQueuePut(g_mb_app.main_to_tts_queue, &tts_event, 0U, 0U);
    }
}

void MB_TaskMain(void *argument)
{
    MB_VoiceEvent_t voice_event;
    (void)memset(&voice_event, 0, sizeof(voice_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.voice_to_main_queue, &voice_event, NULL, osWaitForever) == osOK)
        {
            MB_LOG("[MAIN] recv voice cmd\r\n");
            MB_SetState(MB_STATE_EXECUTING);
            MB_HandleCommand(&voice_event);
            MB_SetState(MB_STATE_IDLE);
        }
    }
}

void MB_TaskVoice(void *argument)
{
#if (MB_TEST_INJECT_ENABLE == 1U)
    static const MB_CommandId_t test_commands[] = {
        MB_CMD_WAVE,
        MB_CMD_TURN_LEFT,
        MB_CMD_TURN_RIGHT,
        MB_CMD_FORWARD,
        MB_CMD_BACKWARD,
        MB_CMD_SIT
    };
    uint32_t idx = 0U;
    MB_VoiceEvent_t voice_event;
    (void)argument;
    for (;;)
    {
        voice_event.command = test_commands[idx];
        voice_event.timestamp_ms = g_mb_heartbeat_ticks * 500U;
        voice_event.confidence = 100U;
        if (osMessageQueuePut(g_mb_app.voice_to_main_queue, &voice_event, 0U, 0U) == osOK)
        {
            MB_LOG("[VOICE] inject cmd\r\n");
        }
        else
        {
            MB_LOG("[VOICE] queue full\r\n");
        }

        idx++;
        if (idx >= (sizeof(test_commands) / sizeof(test_commands[0])))
        {
            idx = 0U;
        }
        osDelay(2000U);
    }
#else
    (void)argument;
    for (;;)
    {
        osDelay(20U);
    }
#endif
}

void MB_TaskMotion(void *argument)
{
    MB_MotionEvent_t motion_event;
    (void)memset(&motion_event, 0, sizeof(motion_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.main_to_motion_queue, &motion_event, NULL, osWaitForever) == osOK)
        {
            MB_LOG("[MOTION] run\r\n");
            osDelay(motion_event.duration_ms);
        }
    }
}

void MB_TaskDisplay(void *argument)
{
    MB_DisplayEvent_t display_event;
    (void)memset(&display_event, 0, sizeof(display_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.main_to_display_queue, &display_event, NULL, osWaitForever) == osOK)
        {
            MB_LOG("[DISPLAY] run\r\n");
            osDelay(display_event.animate ? 80U : 10U);
        }
    }
}

void MB_TaskTts(void *argument)
{
    MB_TtsEvent_t tts_event;
    (void)memset(&tts_event, 0, sizeof(tts_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.main_to_tts_queue, &tts_event, NULL, osWaitForever) == osOK)
        {
            MB_LOG("[TTS] run\r\n");
            osDelay(60U);
        }
    }
}

void MB_TaskHeartbeat(void *argument)
{
    (void)argument;
    for (;;)
    {
        g_mb_heartbeat_ticks++;
        GPIOB->ODR ^= GPIO_Pin_0;
        MB_LOG("[HB] tick\r\n");
        osDelay(500U);
    }
}
