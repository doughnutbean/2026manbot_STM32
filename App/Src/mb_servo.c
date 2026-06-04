#include "mb_servo.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define PCA_ADDR   0x80U

static const uint8_t PCA_MODE1 = 0x00U;
static const uint8_t PCA_PRESCALE = 0xFEU;
static const uint8_t PCA_LED0 = 0x06U;

#define SCL_H()  (GPIOB->BSRR = GPIO_Pin_6)
#define SCL_L()  (GPIOB->BRR  = GPIO_Pin_6)
#define SDA_H()  (GPIOB->BSRR = GPIO_Pin_7)
#define SDA_L()  (GPIOB->BRR  = GPIO_Pin_7)
#define SDA_RD() ((GPIOB->IDR & GPIO_Pin_7) ? 1U : 0U)

uint8_t g_servo_inited = 0U;

static void dly(void) { volatile uint32_t i = 30U; while (i--) {} }

static void I2C_Start(void) { SDA_H();dly();SCL_H();dly();SDA_L();dly();SCL_L();dly(); }
static void I2C_Stop(void)  { SDA_L();dly();SCL_H();dly();SDA_H();dly(); }

static void I2C_Write(uint8_t b) {
    uint8_t i;
    for (i = 0U; i < 8U; i++) { if (b & 0x80U) SDA_H(); else SDA_L(); dly(); SCL_H(); dly(); SCL_L(); b <<= 1; }
    SDA_H(); dly(); SCL_H(); dly(); SCL_L(); dly();
}

static void PCA_Write(uint8_t reg, uint8_t val) {
    I2C_Start(); I2C_Write(PCA_ADDR); I2C_Write(reg); I2C_Write(val); I2C_Stop();
}

void Servo_Init(void)
{
    if (g_servo_inited) return;
    g_servo_inited = 1U;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef g;
    g.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    g.GPIO_Mode = GPIO_Mode_Out_OD;
    g.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &g);
    SCL_H(); SDA_H();

    PCA_Write(PCA_MODE1, 0x11U);
    PCA_Write(PCA_PRESCALE, 121U);
    PCA_Write(PCA_MODE1, 0xA1U);
    { volatile uint32_t d = 10000U; while (d--) {} }
    PCA_Write(PCA_MODE1, 0x21U);  /* AI=1 保持 */
}

void Servo_SetAngle(uint8_t ch, float angle)
{
    if (ch > 15U) return;
    if (!g_servo_inited) Servo_Init();
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;

    uint16_t off = (uint16_t)(102.0f + angle * 410.0f / 180.0f);

    I2C_Start(); I2C_Write(PCA_ADDR); I2C_Write(PCA_LED0 + ch * 4U);
    I2C_Write(0x00); I2C_Write(0x00);
    I2C_Write((uint8_t)(off & 0xFFU)); I2C_Write((uint8_t)(off >> 8));
    I2C_Stop();
}
