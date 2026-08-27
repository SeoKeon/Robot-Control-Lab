#include "bsp.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* CubeMX 가 Core/Src/main.c 에 만든 핸들. MX_USART2_UART_Init() 이
   app_main() 보다 먼저 불리므로 여기서 바로 써도 된다. */
extern UART_HandleTypeDef huart2;

#define BSP_UART          (&huart2)
#define BSP_UART_TIMEOUT  100u   /* ms. 블로킹 송신이라 무한대기는 피한다 */

void bsp_uart_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(BSP_UART, (uint8_t *)data, len, BSP_UART_TIMEOUT);
}

void bsp_uart_puts(const char *s)
{
    bsp_uart_write((const uint8_t *)s, (uint16_t)strlen(s));
}

void bsp_printf(const char *fmt, ...)
{
    char    buf[128];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    if (n <= 0) {
        return;
    }
    /* vsnprintf 는 "잘리지 않았다면 필요했을 길이"를 준다 */
    if (n > (int)sizeof buf - 1) {
        n = (int)sizeof buf - 1;
    }
    bsp_uart_write((const uint8_t *)buf, (uint16_t)n);
}

void bsp_led_on(void)     { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_SET); }
void bsp_led_off(void)    { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_RESET); }
void bsp_led_toggle(void) { HAL_GPIO_TogglePin(BSP_LED_PORT, BSP_LED_PIN); }

/**
 * app_main() 을 구현하지 않은 프로젝트를 위한 기본 구현.
 * 링크 에러 대신 LED 100ms 점멸 — "실험 코드가 안 붙었다"를 보드에서 바로 알 수 있게.
 */
__attribute__((weak)) void app_main(void)
{
    while (1)
    {
        bsp_led_toggle();
        HAL_Delay(100);
    }
}
