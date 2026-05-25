#ifndef MB_PROTOCOL_H
#define MB_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    MB_CMD_NONE = 0x00,
    MB_CMD_WAKEUP = 0x10,
    MB_CMD_SLEEP = 0x11,
    MB_CMD_FORWARD = 0x20,
    MB_CMD_BACKWARD = 0x21,
    MB_CMD_TURN_LEFT = 0x22,
    MB_CMD_TURN_RIGHT = 0x23,
    MB_CMD_SIT = 0x24,
    MB_CMD_WAVE = 0x25,
    MB_CMD_EXPRESSION_HAPPY = 0x30,
    MB_CMD_EXPRESSION_SAD = 0x31,
    MB_CMD_EXPRESSION_SLEEPY = 0x32,
    MB_CMD_EXPRESSION_CONFUSED = 0x33
} MB_CommandId_t;

typedef enum
{
    MB_STATE_IDLE = 0,
    MB_STATE_AWAKE,
    MB_STATE_RECOGNIZING,
    MB_STATE_EXECUTING,
    MB_STATE_REPLYING,
    MB_STATE_LOW_POWER
} MB_SystemState_t;

typedef enum
{
    MB_TTS_NONE = 0,
    MB_TTS_WAKE_ACK,
    MB_TTS_EXEC_FORWARD,
    MB_TTS_EXEC_BACKWARD,
    MB_TTS_EXEC_TURN_LEFT,
    MB_TTS_EXEC_TURN_RIGHT,
    MB_TTS_EXEC_SIT,
    MB_TTS_EXEC_WAVE,
    MB_TTS_UNKNOWN_CMD,
    MB_TTS_SLEEP_ACK
} MB_TtsId_t;

typedef struct
{
    MB_CommandId_t command;
    uint32_t timestamp_ms;
    uint8_t confidence;
} MB_VoiceEvent_t;

typedef struct
{
    MB_CommandId_t command;
    uint8_t motion_group_id;
    uint16_t duration_ms;
} MB_MotionEvent_t;

typedef struct
{
    MB_CommandId_t command;
    uint8_t expression_id;
    uint8_t animate;
} MB_DisplayEvent_t;

typedef struct
{
    MB_TtsId_t tts_id;
    MB_CommandId_t source_command;
} MB_TtsEvent_t;

#ifdef __cplusplus
}
#endif

#endif
