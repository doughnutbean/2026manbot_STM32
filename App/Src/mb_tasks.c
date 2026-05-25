#include "mb_app.h"
#include "mb_tasks.h"
#include "mb_display_ili9341.h"
#include "mb_face_bitmap.h"
#include <string.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "mb_face_bitmap.c"   /* 内联位图数据，避免 Keil 单独编译 */

#define MB_DEBUG_LOG_ENABLE     1U
#define MB_TEST_INJECT_ENABLE   1U

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

static uint8_t g_mb_display_inited = 0U;

/* ========== 位图表情引擎（128x64 位图 → 横屏 320x240） ========== */

#define FACE_SCALE_X  2     /* 水平: 128*2=256 居中于320 */
#define FACE_SCALE_Y  3     /* 垂直: 64*3=192 居中于240 */
#define FACE_OX  ((ILI9341_WIDTH  - FACE_W * FACE_SCALE_X) / 2U)
#define FACE_OY  ((ILI9341_HEIGHT - FACE_H * FACE_SCALE_Y) / 2U)

/* 快速渲染：逐行扫描，连续黑像素合并成一次 FillRect */
static void MB_RenderFace(const uint8_t *bmp)
{
    uint16_t row, col, run_start;
    uint8_t  in_run;

    ILI9341_FillScreen(COLOR_BLACK);

    for (row = 0U; row < FACE_H; row++)
    {
        in_run = 0U;
        for (col = 0U; col < FACE_W; col++)
        {
            uint16_t byte_idx = col + (row / 8U) * FACE_W;
            uint8_t  bit_pos  = row & 0x07U;
            uint8_t  is_white = (bmp[byte_idx] & (1U << bit_pos)) ? 1U : 0U;

            if (is_white && !in_run)     { run_start = col; in_run = 1U; }
            else if (!is_white && in_run)
            {
                ILI9341_FillRect(FACE_OX + run_start * FACE_SCALE_X,
                                 FACE_OY + row * FACE_SCALE_Y,
                                 (col - run_start) * FACE_SCALE_X,
                                 FACE_SCALE_Y, COLOR_WHITE);
                in_run = 0U;
            }
        }
        if (in_run)
        {
            ILI9341_FillRect(FACE_OX + run_start * FACE_SCALE_X,
                             FACE_OY + row * FACE_SCALE_Y,
                             (FACE_W - run_start) * FACE_SCALE_X,
                             FACE_SCALE_Y, COLOR_WHITE);
        }
    }
}

/* 表情映射 */
static const uint8_t *g_face_list[] = {
    Face_happy,      /* 0: 快乐 */
    Face_stare,      /* 1: 瞪眼 */
    Face_sleep,      /* 2: 睡觉 */
    Face_mania,      /* 3: 狂热 */
    Face_very_happy, /* 4: 大喜 */
    Face_eyes,       /* 5: 眼睛 */
    Face_hello,      /* 6: 打招呼 */
};
static uint8_t g_face_idx = 0U;

static void MB_NextFace(void);  /* 前向声明 */

static void MB_DisplayShowHappy(void)     { g_face_idx = 0U; MB_RenderFace(g_face_list[0]); }
static void MB_DisplayShowSad(void)       { g_face_idx = 1U; MB_RenderFace(g_face_list[1]); }
static void MB_DisplayShowSleepy(void)    { g_face_idx = 2U; MB_RenderFace(g_face_list[2]); }
static void MB_DisplayShowConfused(void)  { MB_NextFace(); }
static void MB_DisplayShowBootPattern(void){ g_face_idx = 0U; MB_RenderFace(g_face_list[0]); }

static void MB_NextFace(void)
{
    g_face_idx++;
    if (g_face_idx >= 7U) g_face_idx = 0U;
    MB_RenderFace(g_face_list[g_face_idx]);
}

static void MB_DisplayHwInit(void)
{
    MB_LOG("[DISPLAY] ILI9341 init start...\r\n");
    ILI9341_Init();
    g_mb_display_inited = 1U;
    MB_DisplayShowBootPattern();
    MB_LOG("[DISPLAY] ILI9341 init ok\r\n");
}

static void MB_DisplayHandleEvent(const MB_DisplayEvent_t *ev)
{
    if ((ev == NULL) || (g_mb_display_inited == 0U)) return;
    switch (ev->command)
    {
    case MB_CMD_EXPRESSION_HAPPY:    MB_DisplayShowHappy();    break;
    case MB_CMD_EXPRESSION_SAD:      MB_DisplayShowSad();      break;
    case MB_CMD_EXPRESSION_SLEEPY:   MB_DisplayShowSleepy();   break;
    case MB_CMD_EXPRESSION_CONFUSED: MB_DisplayShowConfused(); break;
    default:
        if      (ev->expression_id == 0x01U) MB_DisplayShowHappy();
        else if (ev->expression_id == 0x02U) MB_DisplayShowSad();
        else if (ev->expression_id == 0x03U) MB_DisplayShowSleepy();
        else if (ev->expression_id == 0x04U) MB_DisplayShowConfused();
        else MB_DisplayShowBootPattern();
        break;
    }
    if (ev->animate != 0U) osDelay(50U);
}

static void MB_SetState(MB_SystemState_t state)
{
    if (g_mb_app.state_lock == NULL) return;
    if (osMutexAcquire(g_mb_app.state_lock, osWaitForever) == osOK)
    {
        g_mb_app.state = state;
        (void)osMutexRelease(g_mb_app.state_lock);
    }
}

static void MB_HandleCommand(const MB_VoiceEvent_t *ve)
{
    MB_MotionEvent_t me; (void)memset(&me, 0, sizeof(me));
    MB_DisplayEvent_t de; (void)memset(&de, 0, sizeof(de));
    MB_TtsEvent_t te; (void)memset(&te, 0, sizeof(te));
    me.command = ve->command;
    de.command = ve->command;
    te.source_command = ve->command;
    switch (ve->command)
    {
    case MB_CMD_FORWARD:      me.motion_group_id=1; me.duration_ms=1500; de.expression_id=1; de.animate=1; te.tts_id=MB_TTS_EXEC_FORWARD; break;
    case MB_CMD_BACKWARD:     me.motion_group_id=2; me.duration_ms=1500; de.expression_id=1; de.animate=1; te.tts_id=MB_TTS_EXEC_BACKWARD; break;
    case MB_CMD_TURN_LEFT:    me.motion_group_id=3; me.duration_ms=900;  de.expression_id=4; de.animate=1; te.tts_id=MB_TTS_EXEC_TURN_LEFT; break;
    case MB_CMD_TURN_RIGHT:   me.motion_group_id=4; me.duration_ms=900;  de.expression_id=4; de.animate=1; te.tts_id=MB_TTS_EXEC_TURN_RIGHT; break;
    case MB_CMD_SIT:          me.motion_group_id=5; me.duration_ms=1000; de.expression_id=3; de.animate=0; te.tts_id=MB_TTS_EXEC_SIT; break;
    case MB_CMD_WAVE:         me.motion_group_id=6; me.duration_ms=1200; de.expression_id=1; de.animate=1; te.tts_id=MB_TTS_EXEC_WAVE; break;
    case MB_CMD_WAKEUP:       te.tts_id=MB_TTS_WAKE_ACK;  MB_SetState(MB_STATE_AWAKE); break;
    case MB_CMD_SLEEP:        te.tts_id=MB_TTS_SLEEP_ACK; MB_SetState(MB_STATE_LOW_POWER); break;
    case MB_CMD_EXPRESSION_HAPPY:    de.expression_id=1; de.animate=1; break;
    case MB_CMD_EXPRESSION_SAD:      de.expression_id=2; de.animate=1; break;
    case MB_CMD_EXPRESSION_SLEEPY:   de.expression_id=3; de.animate=0; break;
    case MB_CMD_EXPRESSION_CONFUSED: de.expression_id=4; de.animate=1; break;
    default: te.tts_id=MB_TTS_UNKNOWN_CMD; break;
    }
    if (me.motion_group_id) osMessageQueuePut(g_mb_app.main_to_motion_queue, &me, 0U, 0U);
    if (de.expression_id)   osMessageQueuePut(g_mb_app.main_to_display_queue, &de, 0U, 0U);
    osMessageQueuePut(g_mb_app.main_to_tts_queue, &te, 0U, 0U);
}

void MB_TaskMain(void *arg) {
    MB_VoiceEvent_t ve; (void)memset(&ve, 0, sizeof(ve)); (void)arg;
    for (;;) {
        if (osMessageQueueGet(g_mb_app.voice_to_main_queue, &ve, NULL, osWaitForever) == osOK) {
            MB_LOG("[MAIN] recv\r\n");
            MB_SetState(MB_STATE_EXECUTING);
            MB_HandleCommand(&ve);
            MB_SetState(MB_STATE_IDLE);
        }
    }
}

void MB_TaskVoice(void *arg) {
#if (MB_TEST_INJECT_ENABLE == 1U)
    MB_VoiceEvent_t ve; (void)arg;
    /* 启动只注入一次 Happy，之后保持不动 */
    ve.command = MB_CMD_EXPRESSION_HAPPY; ve.timestamp_ms = 0U; ve.confidence = 100U;
    osMessageQueuePut(g_mb_app.voice_to_main_queue, &ve, 0U, 0U);
    MB_LOG("[VOICE] inject happy\r\n");

    /* 等待按钮切换（暂时用心跳节拍模拟：每 10 次心跳 = 5 秒切一次） */
    uint32_t last_tick = g_mb_heartbeat_ticks;
    for (;;)
    {
        osDelay(200U);
        if (g_mb_heartbeat_ticks - last_tick >= 10U)
        {
            last_tick = g_mb_heartbeat_ticks;
            /* TODO: 替换为真实按钮读 GPIO */
            ve.command = MB_CMD_EXPRESSION_CONFUSED; /* 用 Confused 触发 MB_NextFace */
            ve.timestamp_ms = g_mb_heartbeat_ticks * 500U; ve.confidence = 100U;
            osMessageQueuePut(g_mb_app.voice_to_main_queue, &ve, 0U, 0U);
            MB_LOG("[VOICE] next face\r\n");
        }
    }
#else
    (void)arg; for (;;) osDelay(20U);
#endif
}

void MB_TaskMotion(void *arg) {
    MB_MotionEvent_t me; (void)memset(&me, 0, sizeof(me)); (void)arg;
    for (;;) {
        if (osMessageQueueGet(g_mb_app.main_to_motion_queue, &me, NULL, osWaitForever) == osOK) {
            MB_LOG("[MOTION] run\r\n"); osDelay(me.duration_ms);
        }
    }
}

void MB_TaskDisplay(void *arg) {
    MB_DisplayEvent_t de; (void)memset(&de, 0, sizeof(de)); (void)arg;
    osDelay(2000U); MB_DisplayHwInit();
    for (;;) {
        if (osMessageQueueGet(g_mb_app.main_to_display_queue, &de, NULL, osWaitForever) == osOK) {
            if (g_mb_display_inited) { MB_LOG("[DISPLAY] draw\r\n"); MB_DisplayHandleEvent(&de); }
            else MB_LOG("[DISPLAY] SKIP\r\n");
            osDelay(de.animate ? 40U : 10U);
        }
    }
}

void MB_TaskTts(void *arg) {
    MB_TtsEvent_t te; (void)memset(&te, 0, sizeof(te)); (void)arg;
    for (;;) { if (osMessageQueueGet(g_mb_app.main_to_tts_queue, &te, NULL, osWaitForever) == osOK) { MB_LOG("[TTS] run\r\n"); osDelay(60U); } }
}

void MB_TaskHeartbeat(void *arg) {
    (void)arg;
    for (;;) { g_mb_heartbeat_ticks++; GPIOB->ODR ^= GPIO_Pin_0; MB_LOG("[HB] tick\r\n"); osDelay(500U); }
}
