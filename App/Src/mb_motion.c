#include "mb_motion.h"
#include "mb_servo.h"
#include "cmsis_os2.h"

/* 通道映射: PWM0=右前 PWM4=左前 PWM8=右后 PWM12=左后 */
#define RF   0U    /* 右前 */
#define LF   4U    /* 左前 */
#define RB   8U    /* 右后 */
#define LB  12U    /* 左后 */

/*
 * 角度体系: 站立=100°
 *   右腿(RF,RB): 向前=大角度 向后=小角度
 *   左腿(LF,LB): 向前=小角度 向后=大角度
 */
#define ST     100.0f   /* 站立 */
#define RF_FW  130.0f   /* 右前-前摆 */
#define RF_BW  70.0f    /* 右前-后摆 */
#define LF_FW  70.0f    /* 左前-前摆 */
#define LF_BW  130.0f   /* 左前-后摆 */
#define RB_FW  RF_FW    /* 右后-前摆 */
#define RB_BW  RF_BW    /* 右后-后摆 */
#define LB_FW  LF_FW    /* 左后-前摆 */
#define LB_BW  LF_BW    /* 左后-后摆 */

#define STEP_MS    150U
#define TURN_STEP  120U

static void All(float rf, float lf, float rb, float lb)
{
    Servo_SetAngle(RF, rf);
    Servo_SetAngle(LF, lf);
    Servo_SetAngle(RB, rb);
    Servo_SetAngle(LB, lb);
}

static void Stand(void) { All(ST, ST, ST, ST); }

void Motion_Init(void) { Servo_Init(); Stand(); }

/* 前进: 对角步态 (RF+LB前 → LF+RB前) */
void Motion_Forward(uint16_t dur)
{
    uint32_t n = dur / (STEP_MS * 2U); if (!n) n = 1U;
    while (n--)
    {
        All(RF_FW, LF_BW, RB_BW, LF_FW);  /* 右前+左后 前, 左前+右后 后 */
        osDelay(STEP_MS);
        All(RF_BW, LF_FW, RB_FW, LB_BW);  /* 右前+左后 后, 左前+右后 前 */
        osDelay(STEP_MS);
    }
    Stand();
}

/* 后退: 对角步态反向 */
void Motion_Backward(uint16_t dur)
{
    uint32_t n = dur / (STEP_MS * 2U); if (!n) n = 1U;
    while (n--)
    {
        All(RF_BW, LF_FW, RB_FW, LB_BW);
        osDelay(STEP_MS);
        All(RF_FW, LF_BW, RB_BW, LF_FW);
        osDelay(STEP_MS);
    }
    Stand();
}

/* 坐下: 前腿伸 后腿收 */
void Motion_Sit(void)
{
    All(RF_FW, LF_FW, RB_BW, LB_BW);
}

/* 趴下: 全腿收 */
void Motion_LieDown(void)
{
    All(RF_BW, LF_FW, RB_BW, LF_FW);
}

/* 左转: 右腿前推 左腿后收 */
void Motion_TurnLeft(uint16_t dur)
{
    uint32_t n = dur / (TURN_STEP * 2U); if (!n) n = 1U;
    while (n--)
    {
        All(RF_FW, LF_BW, RB_FW, LB_BW);
        osDelay(TURN_STEP);
        Stand();
        osDelay(TURN_STEP);
    }
    Stand();
}

/* 右转: 左腿前推 右腿后收 */
void Motion_TurnRight(uint16_t dur)
{
    uint32_t n = dur / (TURN_STEP * 2U); if (!n) n = 1U;
    while (n--)
    {
        All(RF_BW, LF_FW, RB_BW, LB_FW);
        osDelay(TURN_STEP);
        Stand();
        osDelay(TURN_STEP);
    }
    Stand();
}
