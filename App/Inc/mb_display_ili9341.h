#ifndef MB_DISPLAY_ILI9341_H
#define MB_DISPLAY_ILI9341_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ILI9341_WIDTH   240U
#define ILI9341_HEIGHT  320U

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

/* 表情占位: 用纯色块代替 */
#define ILI9341_COLOR_HAPPY    0x07E0U   /* 绿色 */
#define ILI9341_COLOR_SAD      0x001FU   /* 蓝色 */
#define ILI9341_COLOR_SLEEPY   0xFFE0U   /* 黄色 */
#define ILI9341_COLOR_CONFUSED 0xF81FU   /* 品红 */
#define ILI9341_COLOR_BOOT     0xFFFFU   /* 白色 */

#ifdef __cplusplus
}
#endif

#endif
