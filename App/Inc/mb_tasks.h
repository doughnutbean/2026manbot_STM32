#ifndef MB_TASKS_H
#define MB_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void MB_TaskMain(void *argument);
void MB_TaskVoice(void *argument);
void MB_TaskMotion(void *argument);
void MB_TaskDisplay(void *argument);
void MB_TaskTts(void *argument);
void MB_TaskHeartbeat(void *argument);
extern volatile uint32_t g_mb_heartbeat_ticks;

#ifdef __cplusplus
}
#endif

#endif
