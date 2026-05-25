#include "mb_app.h"
#include "mb_tasks.h"

#ifndef MB_QUEUE_LEN_VOICE
#define MB_QUEUE_LEN_VOICE 8U
#endif

#ifndef MB_QUEUE_LEN_MOTION
#define MB_QUEUE_LEN_MOTION 8U
#endif

#ifndef MB_QUEUE_LEN_DISPLAY
#define MB_QUEUE_LEN_DISPLAY 8U
#endif

#ifndef MB_QUEUE_LEN_TTS
#define MB_QUEUE_LEN_TTS 8U
#endif

MB_AppContext_t g_mb_app = {0};

static void MB_CreateQueues(void)
{
    g_mb_app.voice_to_main_queue = osMessageQueueNew(MB_QUEUE_LEN_VOICE, sizeof(MB_VoiceEvent_t), NULL);
    g_mb_app.main_to_motion_queue = osMessageQueueNew(MB_QUEUE_LEN_MOTION, sizeof(MB_MotionEvent_t), NULL);
    g_mb_app.main_to_display_queue = osMessageQueueNew(MB_QUEUE_LEN_DISPLAY, sizeof(MB_DisplayEvent_t), NULL);
    g_mb_app.main_to_tts_queue = osMessageQueueNew(MB_QUEUE_LEN_TTS, sizeof(MB_TtsEvent_t), NULL);
}

static void MB_CreateLocks(void)
{
    g_mb_app.state_lock = osMutexNew(NULL);
}

void MB_AppInit(void)
{
    MB_CreateQueues();
    MB_CreateLocks();
    g_mb_app.state = MB_STATE_IDLE;
}

void MB_AppStartTasks(void)
{
    (void)osThreadNew(MB_TaskMain, NULL, NULL);
    (void)osThreadNew(MB_TaskVoice, NULL, NULL);
    (void)osThreadNew(MB_TaskMotion, NULL, NULL);
    (void)osThreadNew(MB_TaskDisplay, NULL, NULL);
    (void)osThreadNew(MB_TaskTts, NULL, NULL);
    (void)osThreadNew(MB_TaskHeartbeat, NULL, NULL);
}
