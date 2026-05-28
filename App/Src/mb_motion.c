#include "mb_motion.h"
#include "mb_servo.h"
#include "cmsis_os2.h"

/* 舵机通道: 0=左前 4=右前 8=左后 12=右后 */
#define FL  0U
#define FR  4U
#define BL  8U
#define BR  12U

/* 角度参数（根据实际机械结构调整） */
#define ANG_STAND   100.0f    /* 站立中立位 */
#define ANG_FORWARD 130.0f    /* 前摆 */
#define ANG_BACK    70.0f     /* 后摆 */
#define ANG_SIT_S   120.0f    /* 坐下（前腿前伸） */
#define ANG_SIT_B   80.0f     /* 坐下（后腿后收） */
#define ANG_LIE     40.0f     /* 趴下最低 */
#define STEP_MS     150U      /* 每步时长 ms */
#define TURN_STEP   120U      /* 转弯步长 ms */

static void AllServos(float a0, float a1, float a2, float a3)
{
    Servo_SetAngle(FL, a0);
    Servo_SetAngle(FR, a1);
    Servo_SetAngle(BL, a2);
    Servo_SetAngle(BR, a3);
}

void Motion_Init(void)
{
    Servo_Init();
    AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
}

/* 前进: 对角步态 (trot) */
void Motion_Forward(uint16_t dur)
{
    uint32_t steps = dur / (STEP_MS * 2U);
    if (steps == 0U) steps = 1U;
    while (steps--)
    {
        AllServos(ANG_FORWARD, ANG_BACK, ANG_BACK, ANG_FORWARD);
        osDelay(STEP_MS);
        AllServos(ANG_BACK, ANG_FORWARD, ANG_FORWARD, ANG_BACK);
        osDelay(STEP_MS);
    }
    AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
}

/* 后退: 对角步态反向 */
void Motion_Backward(uint16_t dur)
{
    uint32_t steps = dur / (STEP_MS * 2U);
    if (steps == 0U) steps = 1U;
    while (steps--)
    {
        AllServos(ANG_BACK, ANG_FORWARD, ANG_FORWARD, ANG_BACK);
        osDelay(STEP_MS);
        AllServos(ANG_FORWARD, ANG_BACK, ANG_BACK, ANG_FORWARD);
        osDelay(STEP_MS);
    }
    AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
}

void Motion_Sit(void)
{
    AllServos(ANG_SIT_S, ANG_SIT_S, ANG_SIT_B, ANG_SIT_B);
}

void Motion_LieDown(void)
{
    AllServos(ANG_LIE, ANG_LIE, ANG_LIE, ANG_LIE);
}

/* 左转: 右侧前摆, 左侧后摆 */
void Motion_TurnLeft(uint16_t dur)
{
    uint32_t steps = dur / (TURN_STEP * 2U);
    if (steps == 0U) steps = 1U;
    while (steps--)
    {
        AllServos(ANG_BACK, ANG_FORWARD, ANG_BACK, ANG_FORWARD);
        osDelay(TURN_STEP);
        AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
        osDelay(TURN_STEP);
    }
    AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
}

/* 右转: 左侧前摆, 右侧后摆 */
void Motion_TurnRight(uint16_t dur)
{
    uint32_t steps = dur / (TURN_STEP * 2U);
    if (steps == 0U) steps = 1U;
    while (steps--)
    {
        AllServos(ANG_FORWARD, ANG_BACK, ANG_FORWARD, ANG_BACK);
        osDelay(TURN_STEP);
        AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
        osDelay(TURN_STEP);
    }
    AllServos(ANG_STAND, ANG_STAND, ANG_STAND, ANG_STAND);
}
