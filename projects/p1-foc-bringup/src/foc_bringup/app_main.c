/**
 * p1-foc-bringup — 단계 (b) 왕복 스윕 (오픈루프 연속 구동)
 *
 * 전기각을 부드럽게 증감시켜 로터가 따라오게 한다. 속도를 단계적으로 올려
 * 탈조(stall) 한계를 찾는다.
 *
 * ⚠️ 연속 회전을 하지 않는다 — 모터 선이 함께 돌아가는 구조라 기계각
 *    약 180도가 가동 한계다. 감기면 납땜부가 뜯어진다.
 *    그래서 기계각 +-MECH_AMP_DEG 범위를 왕복한다.
 *
 * 단계 (a) 결과 반영:
 *   - 12스텝(11전진) 기대 94도에 대해 실측 약 70도 -> 토크 부족으로 추종 지연
 *     -> 변조 크기를 0.30 -> 0.45 로 올렸다 (선간 약 4.7V -> 약 0.22A @ 21Ω)
 *   - PWM off 시 반대로 살짝 움직인 것은 유지토크 상실 후 코깅 디텐트(4.29도)
 *     안착. 정상 동작이다.
 *
 * 실행 전: 전류 리밋 0.8A, 무부하, 모터 고정, 선 여유 확인
 *
 * 결과 해석
 *   - 모든 속도에서 부드럽게 왕복        -> 오픈루프 성공. 다음은 전류 센싱
 *   - 특정 속도부터 덜컹거리거나 멈춤    -> 그게 오픈루프 탈조 한계
 *   - 저속에서도 못 따라옴               -> m 을 더 올리거나 결선 재확인
 */
#include "bsp.h"

#define POLE_PAIRS      7.0f
#define MECH_AMP_DEG    50.0f    /* 기계각 진폭. 총 가동 100도 (180도 제한 안쪽) */
#define M_AMP           0.45f    /* 변조 크기. 약 0.22A @ 12V, 21Ω */
#define UPDATE_MS       2u       /* 전기각 갱신 주기 500Hz */
#define DEG2RAD         0.01745329f

/* 전기각 진폭 = 기계각 x 극쌍수 */
#define EL_AMP_DEG      (MECH_AMP_DEG * POLE_PAIRS)

/* 시험할 전기 속도 [deg/s]. 기계 속도 = /7, rpm = /6 */
static const float speeds_el[] = { 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f };
#define N_SPEEDS  (sizeof(speeds_el) / sizeof(speeds_el[0]))

/** 전기각 a[deg] 위치로 벡터를 세운다. */
static void hold(float a_deg)
{
    bsp_pwm_set_vector(a_deg * DEG2RAD, M_AMP);
}

/** 한 방향으로 from -> to 까지 el_dps 속도로 쓸어간다. */
static void ramp(float from_deg, float to_deg, float el_dps)
{
    const float step = el_dps * ((float)UPDATE_MS / 1000.0f);
    const float dir  = (to_deg > from_deg) ? 1.0f : -1.0f;
    float a = from_deg;

    while ((dir > 0.0f && a < to_deg) || (dir < 0.0f && a > to_deg)) {
        hold(a);
        HAL_Delay(UPDATE_MS);
        a += dir * step;
    }
    hold(to_deg);
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== p1 단계(b) 왕복 스윕 ===\r\n");
    bsp_printf("PWM        : 20kHz, deadtime 1us, ARR=%u\r\n", BSP_PWM_ARR);
    bsp_printf("modulation : %d %%  (약 0.22A @ 12V, 21ohm)\r\n", (int)(M_AMP * 100.0f));
    bsp_printf("기계각 진폭: +-%d도 (총 %d도)  <- 선 감김 방지\r\n",
               (int)MECH_AMP_DEG, (int)(2.0f * MECH_AMP_DEG));
    bsp_printf("전기각 진폭: +-%d도\r\n", (int)EL_AMP_DEG);
    bsp_uart_puts("\r\n[확인] 전류 리밋 0.8A / 무부하 / 모터 고정 / 선 여유\r\n");
    bsp_uart_puts("5초 후 통전.\r\n\r\n");

    for (int i = 5; i > 0; i--) {
        bsp_printf("  %d...\r\n", i);
        bsp_led_toggle();
        HAL_Delay(1000);
    }

    bsp_pwm_start();

    /* 1) 한쪽 끝으로 천천히 정렬 — 갑자기 튀지 않게 */
    bsp_uart_puts("정렬 중 (한쪽 끝으로)\r\n");
    hold(0.0f);
    HAL_Delay(500);
    ramp(0.0f, -EL_AMP_DEG, 150.0f);
    HAL_Delay(300);

    /* 2) 속도를 올리며 왕복 */
    for (uint32_t s = 0; s < N_SPEEDS; s++) {
        const float el = speeds_el[s];
        const float mech_dps = el / POLE_PAIRS;

        bsp_printf("\r\n[속도 %lu/%u] 전기 %d deg/s = 기계 %d deg/s = %d rpm\r\n",
                   (unsigned long)(s + 1), (unsigned)N_SPEEDS,
                   (int)el, (int)mech_dps, (int)(mech_dps / 6.0f));

        for (uint32_t pass = 0; pass < 2u; pass++) {
            bsp_led_toggle();
            ramp(-EL_AMP_DEG, +EL_AMP_DEG, el);
            HAL_Delay(150);
            bsp_led_toggle();
            ramp(+EL_AMP_DEG, -EL_AMP_DEG, el);
            HAL_Delay(150);
        }
    }

    /* 3) 중앙으로 되돌리고 정지 */
    bsp_uart_puts("\r\n중앙 복귀\r\n");
    ramp(-EL_AMP_DEG, 0.0f, 150.0f);
    HAL_Delay(300);
    bsp_pwm_stop();

    bsp_uart_puts("PWM 정지 (6게이트 off)\r\n\r\n");
    bsp_uart_puts("결과 해석:\r\n");
    bsp_uart_puts("  전 속도 부드럽게 왕복 -> 오픈루프 성공. 다음은 전류 센싱\r\n");
    bsp_uart_puts("  특정 속도부터 덜컹     -> 그게 오픈루프 탈조 한계\r\n");
    bsp_uart_puts("  저속에서도 못 따라옴   -> m 을 올리거나 결선 재확인\r\n");
    bsp_uart_puts("\r\n다시 하려면 리셋.\r\n");

    while (1) {
        bsp_led_toggle();
        HAL_Delay(1000);
    }
}
