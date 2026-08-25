/**
 * p0-setup / uart_hello — 시리얼 "hello" (p0 완료 기준의 나머지 절반)
 *
 * 목표: 온보드 ST-LINK 의 VCP 로 문자열을 보내 PC 터미널에서 확인.
 *       이게 뚫리면 이후 전기각·듀티·전류값 로깅이 전부 이 경로로 간다.
 *
 * ── 현재 상태: UART 미구현 ─────────────────────────────────────────────
 * platform/g431-esc1/g431-esc1.ioc 에 UART 가 아직 없다.
 * 켜기 전에 확인이 필요하다:
 *
 *   1. UM2516 에서 ST-LINK VCP 가 어느 페리페럴/핀에 물려 있는지 확인
 *      (LPUART1 로 추정되나 미확인. 감으로 USART2 등을 켜면 아무것도
 *       안 나오고 원인 찾는 데 오래 걸린다 — 2026-08-05 로그 참고)
 *   2. CubeMX 로 g431-esc1.ioc 를 열어 해당 UART 활성화, 115200 8N1
 *      (실험 폴더에서 CubeMX 를 새로 돌리지 않는다)
 *   3. platform/g431-esc1/bsp/ 에 송신 래퍼 추가 — 다음 실험에서 재사용된다
 *   4. 아래 while 루프를 HAL_UART_Transmit() 호출로 교체
 * ──────────────────────────────────────────────────────────────────────
 *
 * 그때까지는 LED 패턴으로 "이 펌웨어가 올라가 있다"만 표시한다.
 * 짧게 두 번 + 긴 정지 = uart_hello  (blink_test 는 균일 점멸이라 눈으로 구분된다)
 */
#include "bsp.h"

void app_main(void)
{
    while (1)
    {
        /* 짧게 두 번 */
        for (int i = 0; i < 2; i++)
        {
            bsp_led_on();
            HAL_Delay(80);
            bsp_led_off();
            HAL_Delay(160);
        }

        /* 긴 정지 — 패턴 구분용 */
        HAL_Delay(700);

        /* TODO: UART 확인 후 여기에 송신 추가
         * 예) bsp_uart_puts("hello\r\n");
         */
    }
}
