/**
 * p0-setup / uart_hello — 시리얼 "hello" (p0 완료 기준의 나머지 절반)
 *
 * USART2 (PB3=TX, PB4=RX) → 도터보드의 ST-LINK VCP → PC 의 COM 포트.
 * 플래싱에 쓰는 USB 케이블 그대로 시리얼도 나온다. 어댑터 필요 없음.
 *
 * 터미널 설정: 115200 8N1, 흐름제어 없음.
 *
 * 첫 줄에 SYSCLK 을 찍는 이유:
 *   보드레이트는 페리페럴 클럭에서 나눠서 만든다. 클럭 설정이 틀리면
 *   보드레이트도 틀려서 글자가 깨진다. 즉 이 줄이 제대로 읽히면
 *   "UART 도 되고 클럭 설정도 맞다"가 한 번에 확인된다.
 *   170000000 이 나와야 정상.
 */
#include "bsp.h"

void app_main(void)
{
    bsp_uart_puts("\r\n");
    bsp_uart_puts("=== uart_hello (B-G431B-ESC1) ===\r\n");
    bsp_printf("SYSCLK : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    bsp_printf("USART2 : PB3(TX) / PB4(RX) @ 115200 8N1\r\n");
    bsp_uart_puts("hello 를 1초마다 보낸다. LED 는 같이 토글.\r\n\r\n");

    uint32_t n = 0;

    while (1)
    {
        bsp_printf("hello %lu\r\n", (unsigned long)n);
        n++;

        bsp_led_toggle();
        HAL_Delay(1000);
    }
}
