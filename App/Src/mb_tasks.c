#include "mb_app.h"
#include "mb_tasks.h"
#include "mb_display_ili9341.h"
#include <string.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define MB_DEBUG_LOG_ENABLE     1U     // ???
#define MB_TEST_INJECT_ENABLE   1U     // ?????

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

/* ==================== ILI9341 表情绘制 ==================== */

static uint8_t g_mb_display_inited = 0U;

static void MB_DisplayShowHappy(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_HAPPY);
}

static void MB_DisplayShowSad(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_SAD);
}

static void MB_DisplayShowSleepy(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_SLEEPY);
}

static void MB_DisplayShowConfused(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_CONFUSED);
}

static void MB_DisplayShowBootPattern(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_BOOT);
}

static void MB_DisplayHwInit(void)
{
    MB_LOG("[DISPLAY] ILI9341 init start...\r\n");
    ILI9341_Init();
    g_mb_display_inited = 1U;
    MB_DisplayShowBootPattern();
    MB_LOG("[DISPLAY] ILI9341 init ok\r\n");
}

static void MB_DisplayHandleEvent(const MB_DisplayEvent_t *display_event)
{
    if ((display_event == NULL) || (g_mb_display_inited == 0U))
    {
        return;
    }

    switch (display_event->command)
    {
    case MB_CMD_EXPRESSION_HAPPY:
        MB_DisplayShowHappy();
        break;
    case MB_CMD_EXPRESSION_SAD:
        MB_DisplayShowSad();
        break;
    case MB_CMD_EXPRESSION_SLEEPY:
        MB_DisplayShowSleepy();
        break;
    case MB_CMD_EXPRESSION_CONFUSED:
        MB_DisplayShowConfused();
        break;
    default:
        if (display_event->expression_id == 0x01U)
        {
            MB_DisplayShowHappy();
        }
        else if (display_event->expression_id == 0x02U)
        {
            MB_DisplayShowSad();
        }
        else if (display_event->expression_id == 0x03U)
        {
            MB_DisplayShowSleepy();
        }
        else if (display_event->expression_id == 0x04U)
        {
            MB_DisplayShowConfused();
        }
        else
        {
            MB_DisplayShowBootPattern();
        }
        break;
    }

    if (display_event->animate != 0U)
    {
        osDelay(50U);
    }
}

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

static void MB_HandleCommand(const MB_VoiceEvent_t *voice_event)
{
    MB_MotionEvent_t motion_event;
    MB_DisplayEvent_t display_event;
    MB_TtsEvent_t tts_event;
    (void)memset(&motion_event, 0, sizeof(motion_event));
    (void)memset(&display_event, 0, sizeof(display_event));
    (void)memset(&tts_event, 0, sizeof(tts_event));

    motion_event.command = voice_event->command;
    display_event.command = voice_event->command;
    tts_event.source_command = voice_event->command;

    switch (voice_event->command)
    {
    case MB_CMD_FORWARD:
        motion_event.motion_group_id = 0x01;
        motion_event.duration_ms = 1500;
        display_event.expression_id = 0x01;
        display_event.animate = 1;
        tts_event.tts_id = MB_TTS_EXEC_FORWARD;
        break;
    case MB_CMD_BACKWARD:
        motion_event.motion_group_id = 0x02;
        motion_event.duration_ms = 1500;
        display_event.expression_id = 0x01;
        display_event.animate = 1;
        tts_event.tts_id = MB_TTS_EXEC_BACKWARD;
        break;
    case MB_CMD_TURN_LEFT:
        motion_event.motion_group_id = 0x03;
        motion_event.duration_ms = 900;
        display_event.expression_id = 0x04;
        display_event.animate = 1;
        tts_event.tts_id = MB_TTS_EXEC_TURN_LEFT;
        break;
    case MB_CMD_TURN_RIGHT:
        motion_event.motion_group_id = 0x04;
        motion_event.duration_ms = 900;
        display_event.expression_id = 0x04;
        display_event.animate = 1;
        tts_event.tts_id = MB_TTS_EXEC_TURN_RIGHT;
        break;
    case MB_CMD_SIT:
        motion_event.motion_group_id = 0x05;
        motion_event.duration_ms = 1000;
        display_event.expression_id = 0x03;
        display_event.animate = 0;
        tts_event.tts_id = MB_TTS_EXEC_SIT;
        break;
    case MB_CMD_WAVE:
        motion_event.motion_group_id = 0x06;
        motion_event.duration_ms = 1200;
        display_event.expression_id = 0x01;
        display_event.animate = 1;
        tts_event.tts_id = MB_TTS_EXEC_WAVE;
        break;
    case MB_CMD_EXPRESSION_HAPPY:
        display_event.expression_id = 0x01;
        display_event.animate = 1;
        break;
    case MB_CMD_EXPRESSION_SAD:
        display_event.expression_id = 0x02;
        display_event.animate = 1;
        break;
    case MB_CMD_EXPRESSION_SLEEPY:
        display_event.expression_id = 0x03;
        display_event.animate = 0;
        break;
    case MB_CMD_EXPRESSION_CONFUSED:
        display_event.expression_id = 0x04;
        display_event.animate = 1;
        break;
    case MB_CMD_SLEEP:
        tts_event.tts_id = MB_TTS_SLEEP_ACK;
        MB_SetState(MB_STATE_LOW_POWER);
        break;
    case MB_CMD_WAKEUP:
        tts_event.tts_id = MB_TTS_WAKE_ACK;
        MB_SetState(MB_STATE_AWAKE);
        break;
    default:
        tts_event.tts_id = MB_TTS_UNKNOWN_CMD;
        break;
    }

    if (motion_event.motion_group_id != 0U)
    {
        (void)osMessageQueuePut(g_mb_app.main_to_motion_queue, &motion_event, 0U, 0U);
    }
    if (display_event.expression_id != 0U)
    {
        (void)osMessageQueuePut(g_mb_app.main_to_display_queue, &display_event, 0U, 0U);
    }
    (void)osMessageQueuePut(g_mb_app.main_to_tts_queue, &tts_event, 0U, 0U);
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

    osDelay(2000U);   /* wait for serial monitor to connect */
    MB_DisplayHwInit();

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.main_to_display_queue, &display_event, NULL, osWaitForever) == osOK)
        {
            if (g_mb_display_inited != 0U)
            {
                MB_LOG("[DISPLAY] draw\r\n");
                MB_DisplayHandleEvent(&display_event);
            }
            else
            {
                MB_LOG("[DISPLAY] SKIP (not inited)\r\n");
            }
            osDelay(display_event.animate ? 40U : 10U);
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
