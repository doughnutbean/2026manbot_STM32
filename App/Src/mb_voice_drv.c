// mb_voice_drv.c
#include "mb_voice_drv.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "cmsis_os2.h"

#define VOICE_USART          USART1          // 使用 USART1
#define VOICE_RX_BUF_SIZE    8

static volatile uint8_t  rx_buf[VOICE_RX_BUF_SIZE];
static volatile uint8_t  rx_head = 0, rx_tail = 0, rx_count = 0;

/* USART1 接收中断 */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(VOICE_USART, USART_IT_RXNE) != RESET)
    {
        uint8_t data = (uint8_t)USART_ReceiveData(VOICE_USART);
        if (rx_count < VOICE_RX_BUF_SIZE)
        {
            rx_buf[rx_head] = data;
            rx_head = (rx_head + 1) % VOICE_RX_BUF_SIZE;
            rx_count++;
        }
        else  // 缓冲区满，丢弃最早数据
        {
            rx_tail = (rx_tail + 1) % VOICE_RX_BUF_SIZE;
            rx_buf[rx_head] = data;
            rx_head = (rx_head + 1) % VOICE_RX_BUF_SIZE;
        }
    }
}

void MB_VoiceDrv_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    // PA9: TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA10: RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 9600;                // 与 Python 一致
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(VOICE_USART, &USART_InitStructure);

    USART_ITConfig(VOICE_USART, USART_IT_RXNE, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(VOICE_USART, ENABLE);

    rx_head = rx_tail = rx_count = 0;
}

uint8_t MB_VoiceDrv_GetCommand(void)
{
    uint8_t cmd = 0;
    if (rx_count > 0)
    {
        cmd = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % VOICE_RX_BUF_SIZE;
        __disable_irq();
        rx_count--;
        __enable_irq();
    }
    return cmd;
}
