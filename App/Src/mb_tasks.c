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

/* ==================== 漫波小狗 ILI9341 表情绘制 ==================== */

static uint8_t g_mb_display_inited = 0U;

/*
 * 基于 asset 参考图的比例设计：
 *   超大圆眼（半径 28）+ 双高光 + 小椭圆鼻 + 小嘴 + 腮红
 *   无外圆，纯表情居中
 */

/* ---- 面部参数 ---- */
#define F_CX          120    /* 中心 X */
#define F_CY          145    /* 中心 Y */

/* 眼睛 */
#define F_EYE_LX      72     /* 左眼中心 X */
#define F_EYE_RX      168    /* 右眼中心 X */
#define F_EYE_Y       125    /* 眼 Y */
#define F_EYE_R       28     /* 眼半径（大！） */

/* 鼻子（小椭圆） */
#define F_NOSE_X      120
#define F_NOSE_Y      178
#define F_NOSE_RX     8      /* 鼻子水平半轴 */
#define F_NOSE_RY     5      /* 鼻子垂直半轴 */

/* 嘴 */
#define F_MOUTH_Y     225
#define F_MOUTH_R     26

/* 腮红 */
#define F_BLUSH_LX    36
#define F_BLUSH_RX    204
#define F_BLUSH_Y     175
#define F_BLUSH_R     18

/* ---- 颜色 ---- */
#define CLR_BLUSH     0xF9B0U  /* 柔粉腮红 */
#define CLR_NOSE      0x3145U  /* 深灰鼻 */
#define CLR_TEAR      0x07FFU  /* 青泪 */

/* ====== 绘图辅助 ====== */

/* 椭圆填充（水平扫描） */
static void MB_Ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
    int16_t dy;
    int32_t a2 = (int32_t)rx * rx;
    int32_t b2 = (int32_t)ry * ry;
    for (dy = -ry; dy <= ry; dy++)
    {
        int32_t val = a2 * (b2 - (int32_t)dy * dy) / b2;
        if (val <= 0) continue;
        int32_t s = 1;
        while (s * s <= val) s++;
        int16_t dx = (int16_t)(s - 1);
        int16_t y = cy + dy;
        if (y < 0 || y >= (int16_t)ILI9341_HEIGHT) continue;
        int16_t x0 = cx - dx;
        int16_t x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= (int16_t)ILI9341_WIDTH) x1 = (int16_t)ILI9341_WIDTH - 1;
        if (x0 <= x1)
            ILI9341_DrawHLine((uint16_t)x0, (uint16_t)y, (uint16_t)(x1 - x0 + 1), color);
    }
}

/* 弧形嘴（>0 微笑, <0 悲伤） */
static void MB_MouthArc(int8_t dir)
{
    int16_t x;
    int32_t r2 = (int32_t)F_MOUTH_R * F_MOUTH_R;
    for (x = -(int16_t)F_MOUTH_R; x <= (int16_t)F_MOUTH_R; x++)
    {
        int32_t dx2 = x * x;
        if (dx2 > r2) continue;
        int32_t val = r2 - dx2;
        int32_t s = 1; while (s * s <= val) s++;
        int16_t dy = (int16_t)(s - 1);
        uint16_t py = (dir > 0) ? (uint16_t)(F_MOUTH_Y + dy) : (uint16_t)(F_MOUTH_Y - dy);
        ILI9341_SetPixel((uint16_t)(F_CX + x), py, COLOR_BLACK);
    }
}

/* 大眼睛（黑圆 + 双高光） */
static void MB_BigEye(uint16_t ex, uint16_t ey)
{
    ILI9341_FillCircle(ex, ey, F_EYE_R, COLOR_BLACK);
    /* 大高光：左上 */
    ILI9341_FillCircle(ex - 9, ey - 9, 10, COLOR_WHITE);
    /* 小高光：右下 */
    ILI9341_FillCircle(ex + 10, ey + 8, 4, COLOR_WHITE);
}

/* ====== 公共基础 ====== */
static void MB_DrawBase(uint16_t bg)
{
    ILI9341_FillScreen(bg);
    /* 腮红 */
    ILI9341_FillCircle(F_BLUSH_LX, F_BLUSH_Y, F_BLUSH_R, CLR_BLUSH);
    ILI9341_FillCircle(F_BLUSH_RX, F_BLUSH_Y, F_BLUSH_R, CLR_BLUSH);
    /* 大眼 */
    MB_BigEye(F_EYE_LX, F_EYE_Y);
    MB_BigEye(F_EYE_RX, F_EYE_Y);
    /* 小椭圆鼻 */
    MB_Ellipse(F_NOSE_X, F_NOSE_Y, F_NOSE_RX, F_NOSE_RY, CLR_NOSE);
    /* 鼻高光 */
    ILI9341_FillCircle(F_NOSE_X - 2, F_NOSE_Y - 2, 3, COLOR_WHITE);
}

/* ====== 四种表情 ====== */

static void MB_DisplayShowHappy(void)
{
    MB_DrawBase(ILI9341_COLOR_HAPPY);
    MB_MouthArc(1);
}

static void MB_DisplayShowSad(void)
{
    MB_DrawBase(ILI9341_COLOR_SAD);
    MB_MouthArc(-1);
    /* 泪滴 */
    ILI9341_FillCircle(F_EYE_LX - 5, F_EYE_Y + F_EYE_R + 5, 5, CLR_TEAR);
    ILI9341_FillCircle(F_EYE_RX + 5, F_EYE_Y + F_EYE_R + 5, 5, CLR_TEAR);
    /* 泪痕 */
    ILI9341_DrawVLine(F_EYE_LX - 5, F_EYE_Y + F_EYE_R + 10, 10, CLR_TEAR);
    ILI9341_DrawVLine(F_EYE_RX + 5, F_EYE_Y + F_EYE_R + 10, 10, CLR_TEAR);
}

static void MB_DisplayShowSleepy(void)
{
    MB_DrawBase(ILI9341_COLOR_SLEEPY);
    /* 用背景色盖掉圆眼上半 → 眯眼 */
    ILI9341_FillRect(F_EYE_LX - F_EYE_R - 1, F_EYE_Y - F_EYE_R - 1,
                     (uint16_t)(F_EYE_R * 2 + 2), (uint16_t)F_EYE_R, ILI9341_COLOR_SLEEPY);
    ILI9341_FillRect(F_EYE_RX - F_EYE_R - 1, F_EYE_Y - F_EYE_R - 1,
                     (uint16_t)(F_EYE_R * 2 + 2), (uint16_t)F_EYE_R, ILI9341_COLOR_SLEEPY);
    /* 眯眼底部弧线 */
    {
        int16_t ex;
        for (ex = -(int16_t)F_EYE_R; ex <= (int16_t)F_EYE_R; ex++)
        {
            int32_t d2 = (int32_t)F_EYE_R * F_EYE_R - ex * ex;
            int32_t s = 1; while (s * s <= d2) s++;
            uint16_t ey = (uint16_t)(F_EYE_Y + s - 1);
            ILI9341_SetPixel((uint16_t)(F_EYE_LX + ex), ey, COLOR_BLACK);
            ILI9341_SetPixel((uint16_t)(F_EYE_RX + ex), ey, COLOR_BLACK);
        }
    }
    /* 小圆嘴 */
    ILI9341_FillCircle(F_CX, F_MOUTH_Y, 7, COLOR_BLACK);
}

static void MB_DisplayShowConfused(void)
{
    MB_DrawBase(ILI9341_COLOR_CONFUSED);
    /* 左小眼：擦除→画小眼 */
    ILI9341_FillCircle(F_EYE_LX, F_EYE_Y, F_EYE_R, ILI9341_COLOR_CONFUSED);
    ILI9341_FillCircle(F_EYE_LX, F_EYE_Y, 11, COLOR_BLACK);
    ILI9341_FillCircle(F_EYE_LX - 3, F_EYE_Y - 3, 4, COLOR_WHITE);
    /* 右大眼保持 */
    /* 波浪嘴 */
    {
        int16_t mx;
        for (mx = -(int16_t)F_MOUTH_R; mx <= (int16_t)F_MOUTH_R; mx += 4)
        {
            ILI9341_SetPixel((uint16_t)(F_CX + mx), (uint16_t)F_MOUTH_Y, COLOR_BLACK);
            ILI9341_SetPixel((uint16_t)(F_CX + mx + 2), (uint16_t)(F_MOUTH_Y + 7), COLOR_BLACK);
        }
    }
}

static void MB_DisplayShowBootPattern(void)
{
    ILI9341_FillScreen(ILI9341_COLOR_BOOT);
    MB_Ellipse(F_NOSE_X, F_NOSE_Y, F_NOSE_RX, F_NOSE_RY, CLR_NOSE);
    ILI9341_FillCircle(F_CX, F_CY, 15, COLOR_BLACK);
    ILI9341_FillCircle(F_CX - 5, F_CY - 5, 5, COLOR_WHITE);
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
