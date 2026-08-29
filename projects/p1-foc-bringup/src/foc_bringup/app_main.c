/**
 * p1-foc-bringup — AS5600 엔코더 단독 테스트 (PWM 없음, 통전 없음)
 *
 * 배선: VCC->J4-3(3.3V), GND->J8 GND, SDA->J8 B+/H2(PB7), SCL->J8 Z+/H3(PB8),
 *       DIR->GND. I2C1 100kHz, 풀업은 보드 홀입력 회로의 10K@3.3V.
 *
 * 순서:
 *   1. I2C 스캔 — 0x36 이 응답해야 함 (배선·전원·풀업 검증)
 *   2. 자석 상태 — STATUS(0x0B)의 MD/ML/MH (자석 거리 검증)
 *   3. 각도 스트림 — 손으로 로터를 돌리면 숫자가 따라 변해야 함
 *
 * 성공 기준: 손으로 한 바퀴 돌리면 raw 가 0~4095 를 한 번 훑는다.
 */
#include "bsp.h"

void app_main(void)
{
    bsp_uart_puts("\r\n=== AS5600 encoder test (no PWM) ===\r\n");

    /* 1. 스캔 */
    bsp_i2c_scan();

    /* 2. 자석 상태 */
    uint8_t st = 0;
    if (bsp_as5600_magnet_ok(&st)) {
        bsp_printf("magnet: OK (STATUS=0x%02X)\r\n", st);
    } else {
        bsp_printf("magnet: FAIL (STATUS=0x%02X)  MD=%d ML=%d MH=%d\r\n",
                   st, (st >> 5) & 1, (st >> 4) & 1, (st >> 3) & 1);
        bsp_uart_puts("  ML=1: 자석이 멀거나 약함 / MH=1: 너무 가까움\r\n");
        bsp_uart_puts("  I2C 자체가 안 되면 STATUS=0x00 으로 나온다\r\n");
    }

    bsp_uart_puts("\r\n손으로 로터를 돌려보세요. 100ms 마다 각도 출력.\r\n\r\n");

    /* 3. 각도 스트림 */
    uint16_t raw = 0;
    uint32_t err = 0;

    while (1)
    {
        if (bsp_as5600_read_raw(&raw) == 0) {
            /* 0~4095 -> 도 단위 x10 (정수로. 3600 = 360.0도) */
            uint32_t deg10 = ((uint32_t)raw * 3600u) / BSP_AS5600_CPR;
            bsp_printf("raw %4u   %3lu.%lu deg   (err %lu)\r\n",
                       raw, (unsigned long)(deg10 / 10), (unsigned long)(deg10 % 10),
                       (unsigned long)err);
        } else {
            err++;
            bsp_printf("I2C read err (%lu)\r\n", (unsigned long)err);
        }
        bsp_led_toggle();
        HAL_Delay(100);
    }
}
