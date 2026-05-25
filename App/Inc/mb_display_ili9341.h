#ifndef MB_DISPLAY_ILI9341_H
#define MB_DISPLAY_ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ILI9341_WIDTH   320U   /* 横屏 */
#define ILI9341_HEIGHT  240U

/* FSMC 基地址（参考野火指南者 TFT 端口） */
#define ILI9341_CMD_ADDR   ((uint32_t)0x60000000U)
#define ILI9341_DATA_ADDR  ((uint32_t)0x60020000U)

/* 背光控制: PD12 */
#define LCD_BK_ON()   GPIO_ResetBits(GPIOD, GPIO_Pin_12)
#define LCD_BK_OFF()  GPIO_SetBits(GPIOD, GPIO_Pin_12)

void ILI9341_Init(void);
void ILI9341_OpenWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ILI9341_WritePixel(uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_FillScreen(uint16_t color);

/* 阶段二：绘图基础函数 */
void ILI9341_SetPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void ILI9341_FillCircle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);
void ILI9341_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ILI9341_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);

/* RGB565 常用颜色 */
#define COLOR_WHITE   0xFFFFU
#define COLOR_BLACK   0x0000U
#define COLOR_RED     0xF800U
#define COLOR_GREEN   0x07E0U
#define COLOR_BLUE    0x001FU
#define COLOR_YELLOW  0xFFE0U
#define COLOR_MAGENTA 0xF81FU
#define COLOR_CYAN    0x07FFU
#define COLOR_ORANGE  0xFD20U
#define COLOR_GRAY    0x8410U

/* 表情背景色 */
#define ILI9341_COLOR_HAPPY     0xFFF0U   /* 浅黄 */
#define ILI9341_COLOR_SAD       0xC7FFU   /* 浅蓝 */
#define ILI9341_COLOR_SLEEPY    0xE73FU   /* 浅紫 */
#define ILI9341_COLOR_CONFUSED  0xFFD0U   /* 浅橙 */
#define ILI9341_COLOR_BOOT      0xFFFFU   /* 白色 */

#ifdef __cplusplus
}
#endif

#endif
