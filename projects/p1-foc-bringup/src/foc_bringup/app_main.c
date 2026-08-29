/**
 * p1 — 오픈루프 연속 회전 + 엔코더 로깅 (명령각 vs 실제각)
 *
 * 오픈루프로 벡터를 돌리면서 AS5600 으로 로터 실제 각도를 같이 읽는다.
 * "명령각 - 실제각" 오차에 부하각(δ/극쌍수)과 추종 지연이 그대로 드러난다.
 * FOC 직전의 마지막 관측 실험.
 *
 * 주의: 현재 엔코더는 MD=0/AGC=128 (신호 한계선). 정지 안정성 ±2카운트로
 *       실험엔 충분하지만, PWM 노이즈에 취약할 수 있다 — 그것도 관측 대상.
 *
 * 실행 전: 전류 리밋 0.8A / 무부하 / 모터·엔코더 고정 / 선 연속회전 OK
 */
#include "bsp.h"
#include <stdlib.h>

#define POLE_PAIRS   7
#define M_AMP        0.45f
#define TICK_MS      2u                 /* 벡터 갱신 500Hz */
#define LOG_EVERY    50u                /* 100ms 마다 로그 */
#define TWO_PI       6.2831853f
#define CPR          4096

/* ---- 엔코더 언랩: 랩어라운드를 누적각으로 ---- */
static uint16_t enc_prev;
static int32_t  enc_acc;    /* counts, 시작점 기준 누적 */

static void enc_zero(void)
{
    uint16_t r = 0;
    bsp_as5600_read_raw(&r);
    enc_prev = r;
    enc_acc  = 0;
}

static int32_t enc_update(void)   /* 호출할 때마다 읽고 누적 */
{
    uint16_t r = 0;
    if (bsp_as5600_read_raw(&r) == 0) {
        int32_t d = (int32_t)r - (int32_t)enc_prev;
        if (d >  CPR / 2) { d -= CPR; }
        if (d < -CPR / 2) { d += CPR; }
        enc_acc += d;
        enc_prev = r;
    }
    return enc_acc;
}

/* counts -> 기계각 x10 [deg*10] */
static int32_t cnt_to_deg10(int32_t c) { return (c * 3600) / CPR; }

/**
 * 전기각 el_from -> el_to [deg] 를 el_dps 속도로 돌리며 로깅.
 * cmd0_deg10: 이 구간 시작 시점의 누적 명령 기계각 x10 (연속 표시용)
 * sign: 엔코더 부호 (+1/-1, 명령 방향과 맞추기)
 */
static int32_t sweep(float el_from, float el_to, float el_dps,
                     int32_t cmd0_deg10, int sign)
{
    const float step = el_dps * ((float)TICK_MS / 1000.0f);
    const float dir  = (el_to > el_from) ? 1.0f : -1.0f;
    float    el   = el_from;
    uint32_t tick = 0;

    while ((dir > 0.0f && el < el_to) || (dir < 0.0f && el > el_to)) {
        bsp_pwm_set_vector(el * 0.01745329f, M_AMP);
        enc_update();

        if (++tick % LOG_EVERY == 0u) {
            int32_t cmd10 = cmd0_deg10 + (int32_t)((el - el_from) * 10.0f / POLE_PAIRS);
            int32_t enc10 = sign * cnt_to_deg10(enc_acc);
            bsp_printf("cmd %6ld.%ld  enc %6ld.%ld  err %5ld.%ld\r\n",
                       (long)(cmd10 / 10), (long)labs(cmd10 % 10),
                       (long)(enc10 / 10), (long)labs(enc10 % 10),
                       (long)((cmd10 - enc10) / 10), (long)labs((cmd10 - enc10) % 10));
        }
        HAL_Delay(TICK_MS);
        el += dir * step;
    }
    return cmd0_deg10 + (int32_t)((el_to - el_from) * 10.0f / POLE_PAIRS);
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== open-loop + encoder logging ===\r\n");
    bsp_uart_puts("[확인] 전류 리밋 0.8A / 무부하 / 고정. 5초 후 통전.\r\n");
    for (int i = 5; i > 0; i--) { bsp_printf("  %d...\r\n", i); HAL_Delay(1000); }

    bsp_pwm_start();

    /* 정렬: 벡터 0 에 로터를 붙이고 그 지점을 기계각 0 으로 삼는다 */
    bsp_pwm_set_vector(0.0f, M_AMP);
    HAL_Delay(800);
    enc_zero();

    /* 부호 판정: 전기 1바퀴(기계 51.4도) 돌려 엔코더 방향 확인 */
    bsp_uart_puts("\r\n[부호 판정] 전기 360도 = 기계 51.4도 회전\r\n");
    int32_t cmd10 = sweep(0.0f, 360.0f, 300.0f, 0, +1);
    int sign = (enc_acc >= 0) ? +1 : -1;
    bsp_printf("-> 엔코더 부호 %+d, 측정 %ld.%ld도 (기대 51.4)\r\n\r\n",
               sign, (long)(sign * cnt_to_deg10(enc_acc) / 10),
               (long)labs(cnt_to_deg10(enc_acc) % 10));

    /* 본 실험: 기계 1바퀴씩 두 속도로 정방향, 이어 1바퀴 역방향 */
    bsp_uart_puts("[속도 1] 전기 400 dps = 기계 57 dps, 기계 1바퀴\r\n");
    cmd10 = sweep(360.0f, 360.0f + 2520.0f, 400.0f, cmd10, sign);

    bsp_uart_puts("\r\n[속도 2] 전기 1200 dps = 기계 171 dps, 기계 1바퀴\r\n");
    cmd10 = sweep(360.0f + 2520.0f, 360.0f + 5040.0f, 1200.0f, cmd10, sign);

    bsp_uart_puts("\r\n[역방향] 전기 800 dps, 기계 1바퀴\r\n");
    cmd10 = sweep(360.0f + 5040.0f, 360.0f + 2520.0f, 800.0f, cmd10, sign);

    bsp_pwm_stop();

    int32_t enc10 = sign * cnt_to_deg10(enc_acc);
    bsp_uart_puts("\r\nPWM 정지. 최종:\r\n");
    bsp_printf("  누적 명령 %ld.%ld도 / 누적 실측 %ld.%ld도 / 오차 %ld.%ld도\r\n",
               (long)(cmd10 / 10), (long)labs(cmd10 % 10),
               (long)(enc10 / 10), (long)labs(enc10 % 10),
               (long)((cmd10 - enc10) / 10), (long)labs((cmd10 - enc10) % 10));
    bsp_uart_puts("err 가 속도에 따라 커졌다 줄어드는 것 = 부하각 δ. FOC 가 없앨 대상.\r\n");

    while (1) { bsp_led_toggle(); HAL_Delay(1000); }
}
