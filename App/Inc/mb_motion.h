#ifndef MB_MOTION_H
#define MB_MOTION_H

#include <stdint.h>

void Motion_Init(void);
void Motion_Forward(uint16_t duration_ms);
void Motion_Backward(uint16_t duration_ms);
void Motion_Sit(void);
void Motion_LieDown(void);
void Motion_TurnLeft(uint16_t duration_ms);
void Motion_TurnRight(uint16_t duration_ms);

#endif
