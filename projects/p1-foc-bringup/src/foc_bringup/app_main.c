/**
 * p1-foc-bringup — 단계 (a) 정지 벡터 테스트
 *
 * 회전시키지 않는다. 고정된 전기각으로 전압 벡터를 인가해서
 * 로터가 그 방향으로 "딸깍" 붙는지만 본다. 가장 안전한 첫 통전 테스트다.
 *
 * 전기각을 60도씩 6번 옮겨 한 바퀴(전기각 360도)를 돌린다. 2회 반복.
 * 로터가 매번 새 위치로 스텝하면 3상 PWM 과 게이트 드라이버가 다 살아있는 것.
 *
 *   전기각 60도  =  기계각 60/7 = 8.57도       (12N14P, pole pairs 7)
 *   전기각 360도 =  기계각 51.43도
 *   12스텝 총    =  기계각 약 103도            <- 눈으로 보인다
 *
 * ── 실행 전 확인 ───────────────────────────────────────────────────────
 *   1. 전원 전류 리밋  0.8~1A     <- BKIN 이 없어 이게 유일한 보호장치
 *   2. 무부하 (프로펠러 없음), 모터 고정
 *   3. 12V 인가, UVW 결선 확인 (상간 21Ω 세 쌍 동일)
 * ──────────────────────────────────────────────────────────────────────
 *
 * 결과 해석
 *   - 6위치로 또박또박 스텝  -> 정상. 다음은 단계 (b) 연속 회전
 *   - 떨기만 하고 안 움직임  -> 상 순서 문제. UVW 중 두 가닥을 바꿔볼 것
 *   - 한 방향만 움직이고 멈춤 -> 한 상이 안 나옴. 그 상의 게이트/납땜 확인
 *   - 아무 반응 없음         -> m 을 올려보고(0.4), 그래도 없으면 전원/MOE 확인
 */
#include "bsp.h"

#define POLE_PAIRS   7u
#define STEPS        6u        /* 전기각 한 바퀴를 몇 등분할지 */
#define REVS         2u        /* 전기각 몇 바퀴 */
#define HOLD_MS      600u      /* 한 위치 유지 시간. 길게 하면 권선이 뜨거워진다 */
#define M_AMP        0.30f     /* 변조 크기. 12V/21Ω 에서 선간 약 3.1V -> 약 0.15A */

#define TWO_PI       6.2831853f

static void countdown(uint32_t sec)
{
    for (uint32_t i = sec; i > 0; i--) {
        bsp_printf("  %lu...\r\n", (unsigned long)i);
        bsp_led_toggle();
        HAL_Delay(1000);
    }
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== p1 단계(a) 정지 벡터 테스트 ===\r\n");
    bsp_printf("SYSCLK    : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    bsp_printf("PWM       : 20kHz center-aligned, ARR=%u, deadtime 1us\r\n", BSP_PWM_ARR);
    bsp_printf("modulation: %d %% (선간 약 3.1V -> 약 0.15A @ 21ohm)\r\n", (int)(M_AMP * 100.0f));
    bsp_printf("전기각 60도 = 기계각 %d.%02d도\r\n", 60 / POLE_PAIRS,
               (int)((60.0f / POLE_PAIRS - (float)(60 / POLE_PAIRS)) * 100.0f));
    bsp_uart_puts("\r\n[확인] 전류 리밋 0.8~1A / 무부하 / 모터 고정\r\n");
    bsp_uart_puts("5초 후 통전한다. 중단하려면 전원을 끊을 것.\r\n\r\n");

    countdown(5);

    bsp_uart_puts("PWM 출력 시작\r\n\r\n");
    bsp_pwm_start();

    for (uint32_t rev = 0; rev < REVS; rev++) {
        for (uint32_t k = 0; k < STEPS; k++) {
            const float theta_e = TWO_PI * (float)k / (float)STEPS;
            const int   deg_e   = (int)(360 * k / STEPS);

            bsp_pwm_set_vector(theta_e, M_AMP);
            bsp_printf("rev %lu  step %lu/%u  전기각 %3d도\r\n",
                       (unsigned long)(rev + 1), (unsigned long)(k + 1), STEPS, deg_e);

            bsp_led_toggle();
            HAL_Delay(HOLD_MS);
        }
    }

    bsp_pwm_stop();
    bsp_uart_puts("\r\nPWM 출력 정지 (6게이트 전부 off)\r\n\r\n");

    bsp_uart_puts("결과 해석:\r\n");
    bsp_uart_puts("  6위치로 또박또박 스텝  -> 정상. 다음은 단계(b) 연속 회전\r\n");
    bsp_uart_puts("  떨기만 함              -> 상 순서. UVW 중 두 가닥 교체\r\n");
    bsp_uart_puts("  한 방향만 움직이다 멈춤 -> 한 상 불량. 게이트/납땜 확인\r\n");
    bsp_uart_puts("  아무 반응 없음         -> m 을 0.4 로 올려보고, 전원 확인\r\n");
    bsp_uart_puts("\r\n다시 하려면 리셋.\r\n");

    while (1) {
        bsp_led_toggle();
        HAL_Delay(1000);
    }
}
