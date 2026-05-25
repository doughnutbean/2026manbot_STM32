#ifndef MB_APP_H
#define MB_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"
#include "mb_protocol.h"
#include <stddef.h>

typedef struct
{
    osMessageQueueId_t voice_to_main_queue;
    osMessageQueueId_t main_to_motion_queue;
    osMessageQueueId_t main_to_display_queue;
    osMessageQueueId_t main_to_tts_queue;
    osMutexId_t state_lock;
    MB_SystemState_t state;
} MB_AppContext_t;

extern MB_AppContext_t g_mb_app;

void MB_AppInit(void);
void MB_AppStartTasks(void);

#ifdef __cplusplus
}
#endif

#endif
