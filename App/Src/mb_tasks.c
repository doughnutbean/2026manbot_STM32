#include "mb_app.h"
#include "mb_tasks.h"
#include "mb_display_ili9341.h"
#include "mb_face_bitmap.h"

#include <string.h>

#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"


/* =========================
 * ????
 * ========================= */
#ifndef MB_DEBUG_LOG_ENABLE
#define MB_DEBUG_LOG_ENABLE     1U
#endif

#ifndef MB_TEST_INJECT_ENABLE
#define MB_TEST_INJECT_ENABLE   0U
#endif


volatile uint32_t g_mb_heartbeat_ticks = 0U;

extern void MB_DebugLog(const char *text);

#if (MB_DEBUG_LOG_ENABLE == 1U)
#define MB_LOG(text) MB_DebugLog(text)
#else
#define MB_LOG(text) ((void)0)
#endif


/* =========================
 * Display expression id compatibility
 * ?? mb_protocol.h ???? MB_EXPRESSION_xxx,
 * ??? mb_tasks.c ???????????
 * ========================= */
#ifndef MB_EXPRESSION_NONE
#define MB_EXPRESSION_NONE      0x00U
#endif

#ifndef MB_EXPRESSION_HAPPY
#define MB_EXPRESSION_HAPPY     0x01U
#endif

#ifndef MB_EXPRESSION_SAD
#define MB_EXPRESSION_SAD       0x02U
#endif

#ifndef MB_EXPRESSION_SLEEPY
#define MB_EXPRESSION_SLEEPY    0x03U
#endif

#ifndef MB_EXPRESSION_CONFUSED
#define MB_EXPRESSION_CONFUSED  0x04U
#endif
/* =========================
 * USART1 ???????
 * ========================= */
static uint8_t MB_DebugUart_ReadByte(uint8_t *data)
{
    if (data == NULL)
    {
        return 0U;
    }

    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
    {
        *data = (uint8_t)USART_ReceiveData(USART1);
        return 1U;
    }

    return 0U;
}


static uint8_t MB_IsValidPcCommand(uint8_t cmd)
{
    switch ((MB_CommandId_t)cmd)
    {
    case MB_CMD_WAKEUP:
    case MB_CMD_SLEEP:
    case MB_CMD_FORWARD:
    case MB_CMD_BACKWARD:
    case MB_CMD_TURN_LEFT:
    case MB_CMD_TURN_RIGHT:
    case MB_CMD_SIT:
    case MB_CMD_WAVE:
    case MB_CMD_EXPRESSION_HAPPY:
    case MB_CMD_EXPRESSION_SAD:
    case MB_CMD_EXPRESSION_SLEEPY:
    case MB_CMD_EXPRESSION_CONFUSED:
        return 1U;

    default:
        return 0U;
    }
}


/* =========================
 * ILI9341 ??????
 * 128×64 ???? ? 320×240 ????????
 * ========================= */
static uint8_t g_mb_display_inited = 0U;

#define FACE_SCALE_X  2U
#define FACE_SCALE_Y  3U

#define FACE_OX  ((ILI9341_WIDTH  - FACE_W * FACE_SCALE_X) / 2U)
#define FACE_OY  ((ILI9341_HEIGHT - FACE_H * FACE_SCALE_Y) / 2U)


static void MB_RenderFace(const uint8_t *bmp, uint16_t bg_color, uint16_t fg_color)
{
    uint16_t row;
    uint16_t col;
    uint16_t run_start;
    uint8_t in_run;

    if (bmp == NULL)
    {
        return;
    }

    ILI9341_FillScreen(bg_color);

    for (row = 0U; row < FACE_H; row++)
    {
        in_run = 0U;
        run_start = 0U;

        for (col = 0U; col < FACE_W; col++)
        {
            uint16_t byte_idx;
            uint8_t bit_pos;
            uint8_t is_black;

            byte_idx = (uint16_t)(col + (row / 8U) * FACE_W);
            bit_pos = (uint8_t)(row & 0x07U);
            is_black = (bmp[byte_idx] & (1U << bit_pos)) ? 1U : 0U;

            if ((is_black != 0U) && (in_run == 0U))
            {
                run_start = col;
                in_run = 1U;
            }
            else if ((is_black == 0U) && (in_run != 0U))
            {
                ILI9341_FillRect(
                    (uint16_t)(FACE_OX + run_start * FACE_SCALE_X),
                    (uint16_t)(FACE_OY + row * FACE_SCALE_Y),
                    (uint16_t)((col - run_start) * FACE_SCALE_X),
                    FACE_SCALE_Y,
                    fg_color
                );

                in_run = 0U;
            }
        }

        if (in_run != 0U)
        {
            ILI9341_FillRect(
                (uint16_t)(FACE_OX + run_start * FACE_SCALE_X),
                (uint16_t)(FACE_OY + row * FACE_SCALE_Y),
                (uint16_t)((FACE_W - run_start) * FACE_SCALE_X),
                FACE_SCALE_Y,
                fg_color
            );
        }
    }
}


static void MB_DisplayShowHappy(void)
{
    MB_RenderFace(Face_happy, COLOR_BLACK, COLOR_WHITE);
}


static void MB_DisplayShowSad(void)
{
    MB_RenderFace(Face_stare, COLOR_BLACK, COLOR_WHITE);
}


static void MB_DisplayShowSleepy(void)
{
    MB_RenderFace(Face_sleep, COLOR_BLACK, COLOR_WHITE);
}


static void MB_DisplayShowConfused(void)
{
    MB_RenderFace(Face_mania, COLOR_BLACK, COLOR_WHITE);
}


static void MB_DisplayShowBootPattern(void)
{
    MB_DisplayShowHappy();
}


static void MB_DisplayHwInit(void)
{
    MB_LOG("[DISPLAY] ILI9341 init start\r\n");

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
        switch (display_event->expression_id)
        {
        case MB_EXPRESSION_HAPPY:
            MB_DisplayShowHappy();
            break;

        case MB_EXPRESSION_SAD:
            MB_DisplayShowSad();
            break;

        case MB_EXPRESSION_SLEEPY:
            MB_DisplayShowSleepy();
            break;

        case MB_EXPRESSION_CONFUSED:
            MB_DisplayShowConfused();
            break;

        default:
            MB_DisplayShowBootPattern();
            break;
        }
        break;
    }

    if (display_event->animate != 0U)
    {
        osDelay(50U);
    }
}


/* =========================
 * ??????
 * ========================= */
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


/* =========================
 * Main ??????
 * Voice ? Main ? Motion / Display / TTS
 * ========================= */
static void MB_HandleCommand(const MB_VoiceEvent_t *voice_event)
{
    MB_MotionEvent_t motion_event;
    MB_DisplayEvent_t display_event;
    MB_TtsEvent_t tts_event;

    if (voice_event == NULL)
    {
        return;
    }

    (void)memset(&motion_event, 0, sizeof(motion_event));
    (void)memset(&display_event, 0, sizeof(display_event));
    (void)memset(&tts_event, 0, sizeof(tts_event));

    motion_event.command = voice_event->command;
    display_event.command = voice_event->command;
    tts_event.source_command = voice_event->command;

    switch (voice_event->command)
    {
    case MB_CMD_FORWARD:
        motion_event.motion_group_id = 0x01U;
        motion_event.duration_ms = 1500U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_HAPPY;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_EXEC_FORWARD;
        break;

    case MB_CMD_BACKWARD:
        motion_event.motion_group_id = 0x02U;
        motion_event.duration_ms = 1500U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_HAPPY;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_EXEC_BACKWARD;
        break;

    case MB_CMD_TURN_LEFT:
        motion_event.motion_group_id = 0x03U;
        motion_event.duration_ms = 900U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_CONFUSED;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_EXEC_TURN_LEFT;
        break;

    case MB_CMD_TURN_RIGHT:
        motion_event.motion_group_id = 0x04U;
        motion_event.duration_ms = 900U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_CONFUSED;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_EXEC_TURN_RIGHT;
        break;

    case MB_CMD_SIT:
        motion_event.motion_group_id = 0x05U;
        motion_event.duration_ms = 1000U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_SLEEPY;
        display_event.animate = 0U;
        tts_event.tts_id = MB_TTS_EXEC_SIT;
        break;

    case MB_CMD_WAVE:
        motion_event.motion_group_id = 0x06U;
        motion_event.duration_ms = 1200U;
        display_event.expression_id = (uint8_t)MB_EXPRESSION_HAPPY;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_EXEC_WAVE;
        break;

    case MB_CMD_SLEEP:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_SLEEPY;
        display_event.animate = 0U;
        tts_event.tts_id = MB_TTS_SLEEP_ACK;
        MB_SetState(MB_STATE_LOW_POWER);
        break;

    case MB_CMD_WAKEUP:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_HAPPY;
        display_event.animate = 1U;
        tts_event.tts_id = MB_TTS_WAKE_ACK;
        MB_SetState(MB_STATE_AWAKE);
        break;

    case MB_CMD_EXPRESSION_HAPPY:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_HAPPY;
        display_event.animate = 1U;
        break;

    case MB_CMD_EXPRESSION_SAD:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_SAD;
        display_event.animate = 1U;
        break;

    case MB_CMD_EXPRESSION_SLEEPY:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_SLEEPY;
        display_event.animate = 0U;
        break;

    case MB_CMD_EXPRESSION_CONFUSED:
        display_event.expression_id = (uint8_t)MB_EXPRESSION_CONFUSED;
        display_event.animate = 1U;
        break;

    default:
        tts_event.tts_id = MB_TTS_UNKNOWN_CMD;
        break;
    }

    if (motion_event.motion_group_id != 0U)
    {
        (void)osMessageQueuePut(
            g_mb_app.main_to_motion_queue,
            &motion_event,
            0U,
            0U
        );
    }

    if (display_event.expression_id != (uint8_t)MB_EXPRESSION_NONE)
    {
        (void)osMessageQueuePut(
            g_mb_app.main_to_display_queue,
            &display_event,
            0U,
            0U
        );
    }

    if (tts_event.tts_id != MB_TTS_NONE)
    {
        (void)osMessageQueuePut(
            g_mb_app.main_to_tts_queue,
            &tts_event,
            0U,
            0U
        );
    }
}


/* =========================
 * Main ??
 * ========================= */
void MB_TaskMain(void *argument)
{
    MB_VoiceEvent_t voice_event;

    (void)memset(&voice_event, 0, sizeof(voice_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(
                g_mb_app.voice_to_main_queue,
                &voice_event,
                NULL,
                osWaitForever) == osOK)
        {
            MB_LOG("[MAIN] recv voice cmd\r\n");

            MB_SetState(MB_STATE_EXECUTING);
            MB_HandleCommand(&voice_event);
            MB_SetState(MB_STATE_IDLE);
        }
    }
}


/* =========================
 * Voice ??
 * ????:? USART1 ?????????????
 * ========================= */
void MB_TaskVoice(void *argument)
{
    uint8_t rx_cmd;
    MB_VoiceEvent_t voice_event;

#if (MB_TEST_INJECT_ENABLE == 1U)
    static const MB_CommandId_t test_commands[] = {
        MB_CMD_EXPRESSION_HAPPY,
        MB_CMD_EXPRESSION_SAD,
        MB_CMD_EXPRESSION_SLEEPY,
        MB_CMD_EXPRESSION_CONFUSED,
        MB_CMD_FORWARD,
        MB_CMD_SIT,
        MB_CMD_WAVE
    };
    uint32_t idx = 0U;
    uint32_t last_inject_tick = 0U;
#endif

    (void)memset(&voice_event, 0, sizeof(voice_event));
    (void)argument;

    MB_LOG("[VOICE] started\r\n");

    for (;;)
    {
        if (MB_DebugUart_ReadByte(&rx_cmd) != 0U)
        {
            if (MB_IsValidPcCommand(rx_cmd) != 0U)
            {
                (void)memset(&voice_event, 0, sizeof(voice_event));

                voice_event.command = (MB_CommandId_t)rx_cmd;
                voice_event.timestamp_ms = g_mb_heartbeat_ticks * 500U;
                voice_event.confidence = 100U;

                if (osMessageQueuePut(
                        g_mb_app.voice_to_main_queue,
                        &voice_event,
                        0U,
                        0U) == osOK)
                {
                    MB_LOG("[PC] recv valid cmd\r\n");
                }
                else
                {
                    MB_LOG("[PC] voice queue full\r\n");
                }
            }
            else
            {
                MB_LOG("[PC] recv unknown cmd\r\n");
            }
        }

#if (MB_TEST_INJECT_ENABLE == 1U)
        if ((osKernelGetTickCount() - last_inject_tick) >= 2000U)
        {
            last_inject_tick = osKernelGetTickCount();

            (void)memset(&voice_event, 0, sizeof(voice_event));

            voice_event.command = test_commands[idx];
            voice_event.timestamp_ms = g_mb_heartbeat_ticks * 500U;
            voice_event.confidence = 100U;

            if (osMessageQueuePut(
                    g_mb_app.voice_to_main_queue,
                    &voice_event,
                    0U,
                    0U) == osOK)
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
        }
#endif

        osDelay(10U);
    }
}


/* =========================
 * Motion ??
 * ????:????????
 * ??????/??????????
 * ========================= */
void MB_TaskMotion(void *argument)
{
    MB_MotionEvent_t motion_event;

    (void)memset(&motion_event, 0, sizeof(motion_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(
                g_mb_app.main_to_motion_queue,
                &motion_event,
                NULL,
                osWaitForever) == osOK)
        {
            MB_LOG("[MOTION] run\r\n");
            osDelay(motion_event.duration_ms);
        }
    }
}


/* =========================
 * Display ??
 * ??? ILI9341,??? Main ??? display_event ????
 * ========================= */
void MB_TaskDisplay(void *argument)
{
    MB_DisplayEvent_t display_event;

    (void)memset(&display_event, 0, sizeof(display_event));
    (void)argument;

    /*
     * ???????,????? FSMC ??????
     * ?????????,???? 1000~2000U?
     */
    osDelay(1000U);
    MB_DisplayHwInit();

    for (;;)
    {
        if (osMessageQueueGet(
                g_mb_app.main_to_display_queue,
                &display_event,
                NULL,
                osWaitForever) == osOK)
        {
            if (g_mb_display_inited != 0U)
            {
                MB_LOG("[DISPLAY] draw\r\n");
                MB_DisplayHandleEvent(&display_event);
            }
            else
            {
                MB_LOG("[DISPLAY] skip, not init\r\n");
            }

            osDelay((display_event.animate != 0U) ? 40U : 10U);
        }
    }
}


/* =========================
 * TTS ??
 * ????:????
 * ========================= */
void MB_TaskTts(void *argument)
{
    MB_TtsEvent_t tts_event;

    (void)memset(&tts_event, 0, sizeof(tts_event));
    (void)argument;

    for (;;)
    {
        if (osMessageQueueGet(
                g_mb_app.main_to_tts_queue,
                &tts_event,
                NULL,
                osWaitForever) == osOK)
        {
            if (tts_event.tts_id != MB_TTS_NONE)
            {
                MB_LOG("[TTS] run\r\n");
                osDelay(60U);
            }
        }
    }
}


/* =========================
 * ????
 * ========================= */
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
