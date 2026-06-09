#include "mb_app.h"
#include "mb_tasks.h"
#include "mb_display_ili9341.h"
#include "mb_face_bitmap.h"
#include "mb_motion.h"
#include "mb_voice_drv.h"
#include <string.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "mb_face_bitmap.c"
#include "mb_servo.c"
#include "mb_motion.c"   /* 内联位图数据，避免 Keil 单独编译 */

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

    ILI9341_FillScreen(COLOR_WHITE);

    for (row = 0U; row < FACE_H; row++)
    {
        in_run = 0U;
        for (col = 0U; col < FACE_W; col++)
        {
            uint16_t byte_idx = col + (row / 8U) * FACE_W;
            uint8_t  bit_pos  = row & 0x07U;
            uint8_t  is_black = (bmp[byte_idx] & (1U << bit_pos)) ? 1U : 0U;

            if (is_black && !in_run)     { run_start = col; in_run = 1U; }
            else if (!is_black && in_run)
            {
                ILI9341_FillRect(FACE_OX + run_start * FACE_SCALE_X,
                                 FACE_OY + row * FACE_SCALE_Y,
                                 (col - run_start) * FACE_SCALE_X,
                                 FACE_SCALE_Y, COLOR_BLACK);
                in_run = 0U;
            }
        }
        if (in_run)
        {
            ILI9341_FillRect(FACE_OX + run_start * FACE_SCALE_X,
                             FACE_OY + row * FACE_SCALE_Y,
                             (FACE_W - run_start) * FACE_SCALE_X,
                             FACE_SCALE_Y, COLOR_BLACK);
        }
    }
}

/* 3x5 数字字模 (0-9) */
static const uint8_t g_num_font[10][5] = {
    {0x07,0x05,0x05,0x05,0x07},
    {0x02,0x06,0x02,0x02,0x07},
    {0x07,0x01,0x07,0x04,0x07},
    {0x07,0x01,0x07,0x01,0x07},
    {0x05,0x05,0x07,0x01,0x01},
    {0x07,0x04,0x07,0x01,0x07},
    {0x07,0x04,0x07,0x05,0x07},
    {0x07,0x01,0x01,0x01,0x01},
    {0x07,0x05,0x07,0x05,0x07},
    {0x07,0x05,0x07,0x01,0x07},
};
#define NUM_S  3

static void MB_DrawLabel(uint8_t n)
{
    uint8_t row, col;
    for (row = 0U; row < 5U; row++)
        for (col = 0U; col < 3U; col++)
            if (g_num_font[n][row] & (1U << col))
                ILI9341_FillRect(4U + col * NUM_S, 4U + row * NUM_S, NUM_S, NUM_S, COLOR_BLACK);
}

/* 7 个表情: 0-5 位图, 6 自定义大小眼 */
static const uint8_t *g_face_list[] = {
    Face_happy, Face_stare, Face_sleep, Face_mania,
    Face_very_happy, Face_eyes,
};
#define FACE_COUNT  7U
static uint8_t g_face_idx = 0U;

/* 自定义表情6: 大小眼（实心矩形眼） */
static void MB_DrawFace6(void)
{
    ILI9341_FillScreen(COLOR_WHITE);
    /* 左眼 */
    ILI9341_FillRect(60, 85, 48, 68, COLOR_BLACK);
    /* 右眼 */
    ILI9341_FillRect(200, 70, 56, 84, COLOR_BLACK);
    MB_DrawLabel(6U);
}

static void MB_NextFace(void);
static void MB_PrevFace(void);

static void MB_ShowFace(uint8_t idx)
{
    if (idx == 6U) { MB_DrawFace6(); }
    else           { MB_RenderFace(g_face_list[idx]); MB_DrawLabel(idx); }
}

static void MB_DisplayShowHappy(void)     { g_face_idx = 0U; MB_ShowFace(0U); }
static void MB_DisplayShowSad(void)       { MB_PrevFace(); }
static void MB_DisplayShowSleepy(void)    { g_face_idx = 2U; MB_ShowFace(2U); }
static void MB_DisplayShowConfused(void)  { MB_NextFace(); }
static void MB_DisplayShowBootPattern(void){ g_face_idx = 0U; MB_ShowFace(0U); }

static void MB_NextFace(void)
{
    g_face_idx++;
    if (g_face_idx >= FACE_COUNT) g_face_idx = 0U;
    MB_ShowFace(g_face_idx);
}

static void MB_PrevFace(void)
{
    if (g_face_idx == 0U) g_face_idx = FACE_COUNT - 1U;
    else g_face_idx--;
    MB_ShowFace(g_face_idx);
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

void MB_TaskVoice(void *argument)
{
    (void)argument;

#if (MB_TEST_INJECT_ENABLE == 1U)
    /* ===== 舵机 + 动作测试 ===== */
    extern void Servo_Init(void);
    extern void Servo_SetAngle(uint8_t ch, float angle);
    extern uint8_t g_servo_inited;
    Servo_Init();
    if (g_servo_inited) MB_LOG("[VOICE] servo OK\r\n");
    else                MB_LOG("[VOICE] servo FAIL\r\n");

    extern void Motion_Sit(void);
    extern void Motion_LieDown(void);
    extern void Motion_Forward(uint16_t d);
    extern void Motion_Backward(uint16_t d);
    extern void Motion_TurnLeft(uint16_t d);
    extern void Motion_TurnRight(uint16_t d);
    MB_LOG("[VOICE] motion test\r\n");

    extern void Motion_Wave(void);
    extern void Motion_Swing(uint16_t d);
    extern void Motion_Forward(uint16_t d);

    for (;;)
    {
        Stand();                osDelay(3000U);
        Motion_LieDown();       osDelay(3000U);
        Motion_Sit();           osDelay(3000U);
        Motion_Wave();          osDelay(3000U);
        Motion_Swing(3000U);    osDelay(3000U);
        Motion_Forward(3000U);  osDelay(3000U);
    }
#else
    /* 语音指令监听 */
    MB_VoiceEvent_t voice_event;
    uint8_t raw_cmd;
    uint8_t last_cmd = 0;
    uint32_t last_cmd_tick = 0;
    const uint32_t debounce_ticks = 300;
    MB_VoiceDrv_Init();
    for (;;)
    {
        raw_cmd = MB_VoiceDrv_GetCommand();
        if (raw_cmd != 0)
        {
            uint32_t now = g_mb_heartbeat_ticks * 500;
            if (raw_cmd == last_cmd && (now - last_cmd_tick) < debounce_ticks)
            {
                /* 忽略防抖 */
            }
            else
            {
                voice_event.command = (MB_CommandId_t)raw_cmd;
                voice_event.timestamp_ms = now;
                voice_event.confidence = 100U;
                osMessageQueuePut(g_mb_app.voice_to_main_queue, &voice_event, 0U, 0U);
                last_cmd = raw_cmd;
                last_cmd_tick = now;
            }
        }
        osDelay(20);
    }
#endif
}
void MB_TaskMotion(void *arg) {
    MB_MotionEvent_t me; (void)memset(&me, 0, sizeof(me)); (void)arg;
    Motion_Init();
    for (;;) {
        if (osMessageQueueGet(g_mb_app.main_to_motion_queue, &me, NULL, osWaitForever) == osOK) {
            MB_LOG("[MOTION] run\r\n");
            switch (me.motion_group_id) {
                case 1U: Motion_Forward(me.duration_ms);  break;
                case 2U: Motion_Backward(me.duration_ms); break;
                case 3U: Motion_TurnLeft(me.duration_ms); break;
                case 4U: Motion_TurnRight(me.duration_ms);break;
                case 5U: Motion_Sit();                    break;
                case 6U: Motion_LieDown();                break;
                default: break;
            }
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
