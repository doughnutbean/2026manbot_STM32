#ifndef MB_SERVO_H
#define MB_SERVO_H

#include <stdint.h>

extern uint8_t g_servo_inited;

void Servo_Init(void);
void Servo_SetAngle(uint8_t ch, float angle);  /* ch=0~3, angle=0~180 */

#endif
