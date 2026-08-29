/**
 * AS5600 정렬 진단 모드 — 보드 위치를 움직이며 실시간으로 자석 상태 확인
 *
 * 200ms 마다 한 줄: 자석상태 / AGC(거리 미터) / 각도
 *   MD=1 이 목표. AGC 는 3.3V 모드에서 0~128, 클수록 멀다 (이상적 ~64)
 */
#include "bsp.h"

void app_main(void)
{
    bsp_uart_puts("\r\n=== AS5600 alignment mode ===\r\n");
    bsp_uart_puts("보드를 조금씩 움직여보세요. MD=1 + AGC 60~90 이 목표.\r\n\r\n");

    while (1)
    {
        uint8_t  st = 0, agc = 255;
        uint16_t raw = 0;
        bsp_as5600_magnet_ok(&st);
        bsp_as5600_read_agc(&agc);
        bsp_as5600_read_raw(&raw);

        const char *verdict =
            (st & 0x20u) ? ((st & 0x08u) ? "OK(gap slightly small)" : "** OK **")
                         : ((st & 0x10u) ? "too far" : "no magnet?");

        bsp_printf("MD=%d ML=%d MH=%d  AGC=%3u  raw=%4u  %s\r\n",
                   (st >> 5) & 1, (st >> 4) & 1, (st >> 3) & 1,
                   agc, raw, verdict);

        bsp_led_toggle();
        HAL_Delay(200);
    }
}
