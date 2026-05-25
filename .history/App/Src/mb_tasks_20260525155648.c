#include "mb_app.h"
#include "mb_tasks.h"
#include <string.h>
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"

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

#define MB_OLED_I2C I2C1
#define MB_OLED_I2C_ADDR 0x3CU
#define MB_OLED_I2C_TIMEOUT 20000U

#ifndef MB_OLED_MODEL_SH1106
#define MB_OLED_MODEL_SH1106 0U
#endif

#if (MB_OLED_MODEL_SH1106 == 1U)
#define MB_OLED_COLUMN_OFFSET 2U
#else
#define MB_OLED_COLUMN_OFFSET 0U
#endif

static uint8_t g_mb_display_inited = 0U;

static uint8_t MB_I2C_WaitEvent(I2C_TypeDef *i2c, uint32_t event)
{
    uint32_t timeout = MB_OLED_I2C_TIMEOUT;
    while (I2C_CheckEvent(i2c, event) == ERROR)
    {
        if (timeout == 0U)
        {
            return 0U;
        }
        timeout--;
    }
    return 1U;
}

static uint8_t MB_OledWriteBytes(uint8_t control, const uint8_t *data, uint16_t len)
{
    uint16_t idx;

    if (data == NULL)
    {
        return 0U;
    }

    I2C_GenerateSTART(MB_OLED_I2C, ENABLE);
    if (MB_I2C_WaitEvent(MB_OLED_I2C, I2C_EVENT_MASTER_MODE_SELECT) == 0U)
    {
        return 0U;
    }

    I2C_Send7bitAddress(MB_OLED_I2C, (uint8_t)(MB_OLED_I2C_ADDR << 1), I2C_Direction_Transmitter);
    if (MB_I2C_WaitEvent(MB_OLED_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == 0U)
    {
        I2C_GenerateSTOP(MB_OLED_I2C, ENABLE);
        return 0U;
    }

    I2C_SendData(MB_OLED_I2C, control);
    if (MB_I2C_WaitEvent(MB_OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == 0U)
    {
        I2C_GenerateSTOP(MB_OLED_I2C, ENABLE);
        return 0U;
    }

    for (idx = 0U; idx < len; idx++)
    {
        I2C_SendData(MB_OLED_I2C, data[idx]);
        if (MB_I2C_WaitEvent(MB_OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == 0U)
        {
            I2C_GenerateSTOP(MB_OLED_I2C, ENABLE);
            return 0U;
        }
    }

    I2C_GenerateSTOP(MB_OLED_I2C, ENABLE);
    return 1U;
}

static uint8_t MB_OledWriteCmd(uint8_t cmd)
{
    return MB_OledWriteBytes(0x00U, &cmd, 1U);
}

static uint8_t MB_OledWriteData(const uint8_t *data, uint16_t len)
{
    return MB_OledWriteBytes(0x40U, data, len);
}

static void MB_DisplaySetPos(uint8_t page, uint8_t column)
{
    uint8_t real_column = (uint8_t)(column + MB_OLED_COLUMN_OFFSET);
    (void)MB_OledWriteCmd((uint8_t)(0xB0U + (page & 0x07U)));
    (void)MB_OledWriteCmd((uint8_t)(0x10U | ((real_column >> 4) & 0x0FU)));
    (void)MB_OledWriteCmd((uint8_t)(real_column & 0x0FU));
}

static void MB_DisplayClear(void)
{
    uint8_t page;
    uint8_t line[16];
    uint8_t chunk;
    (void)memset(line, 0, sizeof(line));

    for (page = 0U; page < 8U; page++)
    {
        MB_DisplaySetPos(page, 0U);
        for (chunk = 0U; chunk < 8U; chunk++)
        {
            (void)MB_OledWriteData(line, sizeof(line));
        }
    }
}

static void MB_DisplayFillPattern(uint8_t v0, uint8_t v1)
{
    uint8_t page;
    uint8_t col;
    uint8_t line[16];

    for (page = 0U; page < 8U; page++)
    {
        for (col = 0U; col < sizeof(line); col++)
        {
            line[col] = ((col & 0x01U) == 0U) ? v0 : v1;
        }

        MB_DisplaySetPos(page, 0U);
        for (col = 0U; col < 8U; col++)
        {
            (void)MB_OledWriteData(line, sizeof(line));
        }
    }
}

static void MB_DisplayShowHappy(void)
{
    MB_DisplayFillPattern(0x3CU, 0xC3U);
}

static void MB_DisplayShowSad(void)
{
    MB_DisplayFillPattern(0x18U, 0x81U);
}

static void MB_DisplayShowSleepy(void)
{
    MB_DisplayFillPattern(0x0CU, 0x30U);
}

static void MB_DisplayShowConfused(void)
{
    MB_DisplayFillPattern(0x66U, 0x99U);
}

static void MB_DisplayShowBootPattern(void)
{
    MB_DisplayFillPattern(0xAAU, 0x55U);
}

static void MB_DisplayHwInit(void)
{
    GPIO_InitTypeDef gpio_init;
    I2C_InitTypeDef i2c_init;
    uint8_t init_seq[] = {
        0xAEU,
        0x20U,
        0x00U,
        0xB0U,
        0xC8U,
        0x00U,
        0x10U,
        0x40U,
        0x81U,
        0x7FU,
        0xA1U,
        0xA6U,
        0xA8U,
        0x3FU,
        0xA4U,
        0xD3U,
        0x00U,
        0xD5U,
        0x80U,
        0xD9U,
        0xF1U,
        0xDAU,
        0x12U,
        0xDBU,
        0x40U,
        0x8DU,
        0x14U,
        0xAFU
    };
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_init);

    I2C_DeInit(MB_OLED_I2C);
    I2C_StructInit(&i2c_init);
    i2c_init.I2C_Mode = I2C_Mode_I2C;
    i2c_init.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c_init.I2C_OwnAddress1 = 0x00U;
    i2c_init.I2C_Ack = I2C_Ack_Disable;
    i2c_init.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c_init.I2C_ClockSpeed = 100000U;
    I2C_Init(MB_OLED_I2C, &i2c_init);
    I2C_Cmd(MB_OLED_I2C, ENABLE);

    for (i = 0U; i < sizeof(init_seq); i++)
    {
        if (MB_OledWriteCmd(init_seq[i]) == 0U)
        {
            MB_LOG("[DISPLAY] oled init fail\r\n");
            return;
        }
    }

    MB_DisplayClear();
    MB_DisplayShowBootPattern();
    g_mb_display_inited = 1U;
    MB_LOG("[DISPLAY] oled init ok\r\n");
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

    MB_DisplayHwInit();

    for (;;)
    {
        if (osMessageQueueGet(g_mb_app.main_to_display_queue, &display_event, NULL, osWaitForever) == osOK)
        {
            MB_LOG("[DISPLAY] run\r\n");
            MB_DisplayHandleEvent(&display_event);
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
