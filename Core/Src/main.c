#include "main.h"
#include "cmsis_os2.h"
#include "mb_app.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

static void SystemClock_Config(void);
static void BoardLed_Init(void);
static void DebugUart_Init(void);

int main(void)
{
     SystemClock_Config();
    BoardLed_Init();
  // DebugUart_Init();   // <-- 注释掉，避免与语音接收冲突

    osKernelInitialize();
    MB_AppInit();
    MB_AppStartTasks();
    osKernelStart();

    while (1)
    {
    }
}

static void SystemClock_Config(void)
{
    SystemInit();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

static void BoardLed_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_0;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio_init);

    GPIO_SetBits(GPIOB, GPIO_Pin_0);
}

void MB_DebugLog(const char *text)
{
    if (text == 0)
    {
        return;
    }

    while (*text != '\0')
    {
        USART_SendData(USART1, (uint16_t)(uint8_t)(*text));
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
        }
        text++;
    }
}

static void DebugUart_Init(void)
{
    GPIO_InitTypeDef gpio_init;
    USART_InitTypeDef usart_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_init);

    usart_init.USART_BaudRate = 115200;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Tx;
    USART_Init(USART1, &usart_init);
    USART_Cmd(USART1, ENABLE);
}
