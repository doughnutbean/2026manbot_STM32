/**
 * @file    mb_display_ili9341.c
 * @brief   ILI9341 TFT 显示驱动（FSMC 8080 并口）
 *          基于野火指南者开发板参考代码适配
 */

#include "mb_display_ili9341.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_fsmc.h"

/* ========================== 内部辅助 ========================== */

static void ILI9341_WriteCmd(uint16_t cmd)
{
    *(volatile uint16_t *)ILI9341_CMD_ADDR = cmd;
}

static void ILI9341_WriteData(uint16_t data)
{
    *(volatile uint16_t *)ILI9341_DATA_ADDR = data;
}

static void ILI9341_Delay(volatile uint32_t n)
{
    for (; n != 0U; n--) { }
}

/* ========================== GPIO 配置 ========================== */

static void ILI9341_GPIO_Config(void)
{
    GPIO_InitTypeDef gpio;

    /* 使能 FSMC 及相关 IO 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    /* FSMC 数据线 D0~D15 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;

    /* PD14(FSMC_D0), PD15(FSMC_D1), PD0(FSMC_D2), PD1(FSMC_D3),
       PD8(FSMC_D13), PD9(FSMC_D14), PD10(FSMC_D15) */
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8
                  | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOD, &gpio);

    /* PE7~PE15 (FSMC_D4~D12) */
    gpio.GPIO_Pin = GPIO_Pin_7  | GPIO_Pin_8  | GPIO_Pin_9
                  | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12
                  | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &gpio);

    /* FSMC 控制线: PD4(NOE/RD), PD5(NWE/WR) */
    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOD, &gpio);

    /* PD7(FSMC_NE1/CS), PD11(FSMC_A16/DC) */
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_11;
    GPIO_Init(GPIOD, &gpio);

    /* PE1: LCD 复位 (推挽输出) */
    gpio.GPIO_Pin  = GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOE, &gpio);

    /* PD12: 背光 (推挽输出) */
    gpio.GPIO_Pin  = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOD, &gpio);

    /* 先关背光 */
    GPIO_SetBits(GPIOD, GPIO_Pin_12);
}

/* ========================== FSMC 配置 ========================== */

static void ILI9341_FSMC_Config(void)
{
    FSMC_NORSRAMTimingInitTypeDef  timing;
    FSMC_NORSRAMInitTypeDef        fsmc_cfg;

    timing.FSMC_AddressSetupTime      = 0x02;
    timing.FSMC_AddressHoldTime       = 0x00;
    timing.FSMC_DataSetupTime         = 0x05;
    timing.FSMC_BusTurnAroundDuration = 0x00;
    timing.FSMC_CLKDivision           = 0x00;
    timing.FSMC_DataLatency           = 0x00;
    timing.FSMC_AccessMode            = FSMC_AccessMode_B;

    fsmc_cfg.FSMC_Bank                  = FSMC_Bank1_NORSRAM1;
    fsmc_cfg.FSMC_DataAddressMux        = FSMC_DataAddressMux_Disable;
    fsmc_cfg.FSMC_MemoryType            = FSMC_MemoryType_NOR;
    fsmc_cfg.FSMC_MemoryDataWidth       = FSMC_MemoryDataWidth_16b;
    fsmc_cfg.FSMC_BurstAccessMode       = FSMC_BurstAccessMode_Disable;
    fsmc_cfg.FSMC_WaitSignalPolarity    = FSMC_WaitSignalPolarity_Low;
    fsmc_cfg.FSMC_WrapMode              = FSMC_WrapMode_Disable;
    fsmc_cfg.FSMC_WaitSignalActive      = FSMC_WaitSignalActive_BeforeWaitState;
    fsmc_cfg.FSMC_WriteOperation        = FSMC_WriteOperation_Enable;
    fsmc_cfg.FSMC_WaitSignal            = FSMC_WaitSignal_Disable;
    fsmc_cfg.FSMC_ExtendedMode          = FSMC_ExtendedMode_Disable;
    fsmc_cfg.FSMC_WriteBurst            = FSMC_WriteBurst_Disable;
    fsmc_cfg.FSMC_ReadWriteTimingStruct = &timing;
    fsmc_cfg.FSMC_WriteTimingStruct     = &timing;

    FSMC_NORSRAMInit(&fsmc_cfg);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
}

/* ========================== 硬件复位 ========================== */

static void ILI9341_Rst(void)
{
    GPIO_ResetBits(GPIOE, GPIO_Pin_1);
    ILI9341_Delay(0xAFFFU << 2);
    GPIO_SetBits(GPIOE, GPIO_Pin_1);
    ILI9341_Delay(0xAFFFU << 2);
}

/* ========================== 寄存器初始化序列 ========================== */

static void ILI9341_REG_Config(void)
{
    /* Power control B */
    ILI9341_WriteCmd(0xCF);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x81);
    ILI9341_WriteData(0x30);

    /* Power on sequence control */
    ILI9341_WriteCmd(0xED);
    ILI9341_WriteData(0x64);
    ILI9341_WriteData(0x03);
    ILI9341_WriteData(0x12);
    ILI9341_WriteData(0x81);

    /* Driver timing control A */
    ILI9341_WriteCmd(0xE8);
    ILI9341_WriteData(0x85);
    ILI9341_WriteData(0x10);
    ILI9341_WriteData(0x78);

    /* Power control A */
    ILI9341_WriteCmd(0xCB);
    ILI9341_WriteData(0x39);
    ILI9341_WriteData(0x2C);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x34);
    ILI9341_WriteData(0x02);

    /* Pump ratio control */
    ILI9341_WriteCmd(0xF7);
    ILI9341_WriteData(0x20);

    /* Driver timing control B */
    ILI9341_WriteCmd(0xEA);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x00);

    /* Frame Rate Control */
    ILI9341_WriteCmd(0xB1);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x1B);

    /* Display Function Control */
    ILI9341_WriteCmd(0xB6);
    ILI9341_WriteData(0x0A);
    ILI9341_WriteData(0xA2);

    /* Power Control 1 */
    ILI9341_WriteCmd(0xC0);
    ILI9341_WriteData(0x35);

    /* Power Control 2 */
    ILI9341_WriteCmd(0xC1);
    ILI9341_WriteData(0x11);

    /* VCOM Control 1 */
    ILI9341_WriteCmd(0xC5);
    ILI9341_WriteData(0x45);
    ILI9341_WriteData(0x45);

    /* VCOM Control 2 */
    ILI9341_WriteCmd(0xC7);
    ILI9341_WriteData(0xA2);

    /* Enable 3G */
    ILI9341_WriteCmd(0xF2);
    ILI9341_WriteData(0x00);

    /* Gamma Set */
    ILI9341_WriteCmd(0x26);
    ILI9341_WriteData(0x01);

    /* Positive Gamma Correction */
    ILI9341_WriteCmd(0xE0);
    ILI9341_WriteData(0x0F);
    ILI9341_WriteData(0x26);
    ILI9341_WriteData(0x24);
    ILI9341_WriteData(0x0B);
    ILI9341_WriteData(0x0E);
    ILI9341_WriteData(0x09);
    ILI9341_WriteData(0x54);
    ILI9341_WriteData(0xA8);
    ILI9341_WriteData(0x46);
    ILI9341_WriteData(0x0C);
    ILI9341_WriteData(0x17);
    ILI9341_WriteData(0x09);
    ILI9341_WriteData(0x0F);
    ILI9341_WriteData(0x07);
    ILI9341_WriteData(0x00);

    /* Negative Gamma Correction */
    ILI9341_WriteCmd(0xE1);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x19);
    ILI9341_WriteData(0x1B);
    ILI9341_WriteData(0x04);
    ILI9341_WriteData(0x10);
    ILI9341_WriteData(0x07);
    ILI9341_WriteData(0x2A);
    ILI9341_WriteData(0x47);
    ILI9341_WriteData(0x39);
    ILI9341_WriteData(0x03);
    ILI9341_WriteData(0x06);
    ILI9341_WriteData(0x06);
    ILI9341_WriteData(0x30);
    ILI9341_WriteData(0x38);
    ILI9341_WriteData(0x0F);

    /* Memory Access Control: 竖屏, 左上角起点 */
    ILI9341_WriteCmd(0x36);
    ILI9341_WriteData(0xC8);

    /* Column address: 0 ~ 239 */
    ILI9341_WriteCmd(0x2A);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0xEF);

    /* Page address: 0 ~ 319 */
    ILI9341_WriteCmd(0x2B);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x00);
    ILI9341_WriteData(0x01);
    ILI9341_WriteData(0x3F);

    /* Pixel Format: 16-bit (RGB565) */
    ILI9341_WriteCmd(0x3A);
    ILI9341_WriteData(0x55);

    /* Sleep Out */
    ILI9341_WriteCmd(0x11);
    ILI9341_Delay(0xAFFFU << 2);

    /* Display ON */
    ILI9341_WriteCmd(0x29);
}

/* ========================== 公开 API ========================== */

void ILI9341_Init(void)
{
    ILI9341_GPIO_Config();
    ILI9341_FSMC_Config();
    ILI9341_Rst();
    ILI9341_REG_Config();
    LCD_BK_ON();  /* 开背光 */
}

void ILI9341_OpenWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ILI9341_WriteCmd(0x2A);
    ILI9341_WriteData((uint8_t)(x >> 8));
    ILI9341_WriteData((uint8_t)(x & 0xFF));
    ILI9341_WriteData((uint8_t)((x + w - 1U) >> 8));
    ILI9341_WriteData((uint8_t)((x + w - 1U) & 0xFF));

    ILI9341_WriteCmd(0x2B);
    ILI9341_WriteData((uint8_t)(y >> 8));
    ILI9341_WriteData((uint8_t)(y & 0xFF));
    ILI9341_WriteData((uint8_t)((y + h - 1U) >> 8));
    ILI9341_WriteData((uint8_t)((y + h - 1U) & 0xFF));

    ILI9341_WriteCmd(0x2C);
}

void ILI9341_WritePixel(uint16_t color)
{
    ILI9341_WriteData(color);
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t total;
    ILI9341_OpenWindow(x, y, w, h);
    total = (uint32_t)w * (uint32_t)h;
    while (total > 0U)
    {
        ILI9341_WritePixel(color);
        total--;
    }
}

void ILI9341_FillScreen(uint16_t color)
{
    ILI9341_FillRect(0U, 0U, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}
