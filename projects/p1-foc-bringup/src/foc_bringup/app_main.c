/**
 * p1 — 전류 센싱 브링업 (3션트, OPAMP PGA ×16 → ADC injected, TIM1 꼭대기 샘플링)
 *
 * 1) 부팅 시 자동: bsp_cs_init() 으로 ADC 트리거를 카운터 꼭대기에 정렬하고,
 *    PWM 을 끈 상태(전류 0)에서 세 상 ADC 를 2000번 읽어 영점과 노이즈를 잰다.
 *    모터는 움직이지 않는다.
 * 2) 'v': 정지 벡터를 0 / 120 / 240도(전기)에 세우고 세 상 전류를 평균해 찍는다.
 *    기대: 0도에서 U = +I, V = W = −I/2, 세 상 합 ≈ 0. 각도를 바꾸면 패턴이 돈다.
 *    m=0.45, 12V, 상저항 10.5Ω 이면 I ≈ 4.05V / 15.75Ω ≈ 0.26 A ≈ 9 count.
 * 'o': 영점 재측정.
 *
 * 이전 실험(오프셋 캘리브레이션 + 정류)은 커밋 2587dce 에 있다.
 */
#include "bsp.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define DEG2RAD   0.01745329f
#define VEC_M     0.45f

extern UART_HandleTypeDef huart2;

static float off_u, off_v, off_w;   /* 영점 [count] */

static const char *f1(float x, char *b)
{
    long t = lroundf(x * 10.0f);
    snprintf(b, 16, "%s%ld.%ld", (t < 0) ? "-" : "", labs(t) / 10, labs(t) % 10);
    return b;
}

/* n 회 평균 (0.5 ms 간격 = PWM 10주기마다 새 값) */
static void sample_avg(int n, float *mu, float *mv, float *mw, float *su, float *sv, float *sw)
{
    double au = 0, av = 0, aw = 0, qu = 0, qv = 0, qw = 0;
    for (int i = 0; i < n; i++) {
        uint16_t u, v, w;
        bsp_cs_read_raw(&u, &v, &w);
        au += u; av += v; aw += w;
        qu += (double)u * u; qv += (double)v * v; qw += (double)w * w;
        for (volatile int k = 0; k < 20000; k++) { }   /* 약 0.5 ms */
    }
    *mu = (float)(au / n); *mv = (float)(av / n); *mw = (float)(aw / n);
    if (su) { *su = sqrtf((float)(qu / n) - *mu * *mu); }
    if (sv) { *sv = sqrtf((float)(qv / n) - *mv * *mv); }
    if (sw) { *sw = sqrtf((float)(qw / n) - *mw * *mw); }
}

static void measure_offset(void)
{
    float su, sv, sw;
    sample_avg(2000, &off_u, &off_v, &off_w, &su, &sv, &sw);
    char a[16], b[16], c[16], d[16], e[16], f[16];
    bsp_uart_puts("\r\n[영점] PWM off, 2000 샘플\r\n");
    bsp_printf("  U: 평균 %s count (%s mV)  노이즈 σ %s count\r\n",
               f1(off_u, a), f1(off_u * 3300.0f / 4096.0f, b), f1(su, c));
    bsp_printf("  V: 평균 %s count (%s mV)  노이즈 σ %s count\r\n",
               f1(off_v, d), f1(off_v * 3300.0f / 4096.0f, e), f1(sv, f));
    bsp_printf("  W: 평균 %s count (%s mV)  노이즈 σ %s count\r\n",
               f1(off_w, a), f1(off_w * 3300.0f / 4096.0f, b), f1(sw, c));
}

static void vector_test(void)
{
    static const int angs[3] = { 0, 120, 240 };
    uint32_t c, d;

    bsp_pwm_start();
    bsp_uart_puts("\r\n[정지 벡터] m=0.45, 각 위치 600 ms 정착 후 1000 샘플 평균\r\n");
    bsp_uart_puts("  전기각   U[mA]   V[mA]   W[mA]   합[mA]   (count U/V/W)\r\n");
    for (int k = 0; k < 3; k++) {
        bsp_pwm_set_vector((float)angs[k] * DEG2RAD, VEC_M);
        HAL_Delay(600);
        float mu, mv, mw;
        sample_avg(1000, &mu, &mv, &mw, NULL, NULL, NULL);
        float du = mu - off_u, dv = mv - off_v, dw = mw - off_w;
        float iu = du * BSP_CS_A_PER_COUNT * 1000.0f;
        float iv = dv * BSP_CS_A_PER_COUNT * 1000.0f;
        float iw = dw * BSP_CS_A_PER_COUNT * 1000.0f;
        char a[16], b[16], e[16], s[16], x[16], y[16], z[16];
        bsp_printf("  %4d    %6s  %6s  %6s  %6s    (%s / %s / %s)\r\n", angs[k],
                   f1(iu, a), f1(iv, b), f1(iw, e), f1(iu + iv + iw, s),
                   f1(du, x), f1(dv, y), f1(dw, z));
    }
    if (bsp_cs_trigger_point(&c, &d) == 0) {
        bsp_printf("  (PWM 중 트리거 시점: CNT=%lu %s)\r\n", (unsigned long)c, d ? "하강=꼭대기 직후" : "상승=바닥 직후 !!");
    }
    bsp_pwm_set_vector(0.0f, 0.0f);
    HAL_Delay(200);
    bsp_pwm_stop();
    bsp_uart_puts("  PWM 정지\r\n");
}

static void wait_cmd(uint8_t *ch)
{
    *ch = 0;
    while (HAL_UART_Receive(&huart2, ch, 1, 500) != HAL_OK) { bsp_led_toggle(); }
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== current sense bring-up ===\r\n");

    uint32_t tc = 0;
    int r = bsp_cs_init(&tc);
    bsp_printf("ADC 트리거 정렬: %s (변환 직후 CNT=%lu, ARR=%u)\r\n",
               r == 0 ? "꼭대기 OK" : "실패 !!", (unsigned long)tc, BSP_PWM_ARR);

    measure_offset();

    while (1) {
        bsp_uart_puts("\r\n명령: v=정지 벡터 전류 측정(모터 딸깍)  o=영점 재측정\r\n");
        uint8_t ch;
        wait_cmd(&ch);
        if (ch == 'v') { vector_test(); }
        if (ch == 'o') { measure_offset(); }
    }
}
