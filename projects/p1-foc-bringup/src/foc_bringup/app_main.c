/**
 * p1 — 전기각 오프셋 캘리브레이션 + 엔코더 기반 정류 (전압 모드 FOC)
 *
 * 1) 캘리브레이션: 엔코더 0도와 자석(d축) 0도의 차이를 잰다.
 *    전기각을 30도씩 앞으로 한 바퀴, 뒤로 한 바퀴 돌리며 매 위치에서
 *      offset_k = θe_k − sign·PP·θm_k
 *    를 구해 원형 평균한다. 앞뒤 마찰 지연이 서로 상쇄된다. 2회 반복해 재현성 확인.
 *
 * 2) 정류: 매 루프 엔코더로 전기각을 계산하고 벡터를 θe ± 90도에 세운다.
 *    부하각이 항상 최대토크 각(90도)에 고정 -> 오픈루프와 달리 부하가 걸려도
 *    탈조하지 않아야 한다. 전류 센서 없이 전압 크기 m 으로 토크를 정한다.
 *
 * 시리얼 명령으로 단계별 실행 (사람이 손을 대는 단계를 분리하려고):
 *   c = 캘리브레이션만, r = 손 안 대는 주행, b = 브레이크 테스트(정방향 20s)
 *   r/b 를 처음 받으면 캘리브레이션부터 한다. 실행 중 'q' 면 즉시 정지.
 *   실행 사이에는 PWM off (로터 자유), 오프셋은 RAM 에 유지.
 * 실행 전: 12V / 전류 리밋 0.8A / 무부하 / 모터·엔코더 고정
 */
#include "bsp.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define PP          7
#define CPR         4096
#define TWO_PI      6.2831853f
#define HALF_PI     1.5707963f
#define RAD2DEG     57.295780f
#define DEG2RAD     0.01745329f

#define CAL_M       0.45f     /* 캘리브레이션 벡터 크기 (오픈루프 실험에서 검증된 값) */
#define CAL_STEPS   12        /* 30도 간격 */
#define CAL_HOLD_MS 250u
#define RPM_LIMIT   1200      /* 이보다 빠르면 정지 (안전) */

extern UART_HandleTypeDef huart2;

/* ---------------- 엔코더: 절대각 + 언랩 누적 ---------------- */
static uint16_t enc_raw;     /* 마지막 raw (절대각) */
static int32_t  enc_acc;     /* 누적 카운트 (속도 계산용) */

static void enc_init(void)
{
    uint16_t r = 0;
    bsp_as5600_read_raw(&r);
    enc_raw = r;
    enc_acc = 0;
}

static void enc_update(void)
{
    uint16_t r = 0;
    if (bsp_as5600_read_raw(&r) == 0) {
        int32_t d = (int32_t)r - (int32_t)enc_raw;
        if (d >  CPR / 2) { d -= CPR; }
        if (d < -CPR / 2) { d += CPR; }
        enc_acc += d;
        enc_raw  = r;
    }
}

static float wrap_pi(float a)
{
    while (a >  3.1415927f) { a -= TWO_PI; }
    while (a < -3.1415927f) { a += TWO_PI; }
    return a;
}

static float mech_rad(void) { return (float)enc_raw * (TWO_PI / (float)CPR); }

/* 정지 상태 n 회 원형 평균 [rad] */
static float mech_avg(int n)
{
    float s = 0.0f, c = 0.0f;
    for (int i = 0; i < n; i++) {
        enc_update();
        float t = mech_rad();
        s += sinf(t);
        c += cosf(t);
        HAL_Delay(3);
    }
    return atan2f(s, c);
}

/* 소수 1자리 문자열 (nano printf 에 %f 가 없어서). 버퍼는 호출마다 따로 */
static const char *f1(float x, char *b)
{
    long t = lroundf(x * 10.0f);
    snprintf(b, 16, "%s%ld.%ld", (t < 0) ? "-" : "", labs(t) / 10, labs(t) % 10);
    return b;
}

static int quit_requested(void)
{
    uint8_t ch = 0;
    return (HAL_UART_Receive(&huart2, &ch, 1, 0) == HAL_OK) && (ch == 'q');
}

/* ---------------- 캘리브레이션 ---------------- */
static int   g_sign = +1;
static float g_off  = 0.0f;   /* rad, 전기각 */

static void hold_and_track(float the, uint32_t ms)
{
    bsp_pwm_set_vector(the, CAL_M);
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < ms) { enc_update(); HAL_Delay(2); }
}

/* 결과: 오프셋[rad], 흩어짐[deg 전기], 앞/뒤 평균 차[deg 전기], 1바퀴 기계 이동[deg] */
static void calibrate(float *off, float *spread_deg, float *hyst_deg, float *span_deg)
{
    float th[2 * CAL_STEPS + 2], tm[2 * CAL_STEPS + 2];
    int   n = 0;

    /* 0 에 정렬 */
    hold_and_track(0.0f, 700u);
    enc_init();
    int32_t acc0 = enc_acc;

    /* 앞으로: 30 .. 360 */
    for (int k = 1; k <= CAL_STEPS; k++) {
        float the = TWO_PI * (float)k / (float)CAL_STEPS;
        hold_and_track(the, CAL_HOLD_MS);
        th[n] = the; tm[n] = mech_avg(6); n++;
    }
    *span_deg = (float)(enc_acc - acc0) * 360.0f / (float)CPR;
    g_sign = (*span_deg >= 0.0f) ? +1 : -1;

    /* 뒤로: 330 .. 0 */
    for (int k = CAL_STEPS - 1; k >= 0; k--) {
        float the = TWO_PI * (float)k / (float)CAL_STEPS;
        hold_and_track(the, CAL_HOLD_MS);
        th[n] = the; tm[n] = mech_avg(6); n++;
    }

    /* 오프셋 원형 평균 (전체 / 앞 / 뒤) */
    float s = 0, c = 0, sf = 0, cf = 0, sb = 0, cb = 0;
    float o[2 * CAL_STEPS + 2];
    for (int i = 0; i < n; i++) {
        o[i] = wrap_pi(th[i] - (float)g_sign * (float)PP * tm[i]);
        s += sinf(o[i]); c += cosf(o[i]);
        if (i < CAL_STEPS) { sf += sinf(o[i]); cf += cosf(o[i]); }
        else               { sb += sinf(o[i]); cb += cosf(o[i]); }
    }
    *off = atan2f(s, c);

    float mx = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = fabsf(wrap_pi(o[i] - *off)) * RAD2DEG;
        if (d > mx) { mx = d; }
    }
    *spread_deg = mx;
    *hyst_deg   = wrap_pi(atan2f(sf, cf) - atan2f(sb, cb)) * RAD2DEG;
}

/* ---------------- 정류 (엔코더 기반) ---------------- */
static int run_commutation(float m, int lead, uint32_t ms, const char *label)
{
    bsp_printf("\r\n[%s] m=%d%%  lead %c90deg  %lus\r\n",
               label, (int)(m * 100.0f), lead > 0 ? '+' : '-', (unsigned long)(ms / 1000u));

    uint32_t t0 = HAL_GetTick(), tlog = t0, loops = 0;
    int32_t  acc_log = enc_acc;

    while (HAL_GetTick() - t0 < ms) {
        enc_update();
        float the = (float)g_sign * (float)PP * mech_rad() + g_off;
        bsp_pwm_set_vector(the + (float)lead * HALF_PI, m);
        loops++;

        uint32_t now = HAL_GetTick();
        if (now - tlog >= 100u) {
            float dt  = (float)(now - tlog) / 1000.0f;
            float dps = (float)(enc_acc - acc_log) * 360.0f / (float)CPR / dt;
            float rpm = dps / 6.0f;
            char b1[16];
            bsp_printf("  t %5lu ms  rpm %7s  loop %4lu Hz\r\n",
                       (unsigned long)(now - t0), f1(rpm, b1), (unsigned long)((float)loops / dt));
            acc_log = enc_acc; tlog = now; loops = 0;

            if (fabsf(rpm) > (float)RPM_LIMIT) { bsp_uart_puts("  !! 속도 한계 초과 — 정지\r\n"); return -1; }
            if (quit_requested())              { bsp_uart_puts("  !! 'q' 수신 — 정지\r\n");      return -1; }
        }
    }
    return 0;
}

/* 단계 사이: 3상 같은 듀티(전압 0) = 권선 단락 브레이크로 감속 */
static void coast(uint32_t ms)
{
    bsp_pwm_set_vector(0.0f, 0.0f);
    uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < ms) { enc_update(); HAL_Delay(2); }
}

static int g_calibrated = 0;

static void do_calibration(void)
{
    /* ---- 캘리브레이션 2회 ---- */
    float off[2], spr, hys, span;
    for (int r = 0; r < 2; r++) {
        bsp_printf("\r\n[캘리브레이션 %d/2] 30도씩 앞으로 1바퀴, 뒤로 1바퀴\r\n", r + 1);
        calibrate(&off[r], &spr, &hys, &span);
        char b1[16], b2[16], b3[16];
        bsp_printf("  1바퀴 기계 이동 %s deg (기대 51.4) -> 부호 %+d\r\n", f1(span, b1), g_sign);
        bsp_printf("  오프셋 %s deg(전기)  흩어짐 최대 %s  앞뒤 차 %s\r\n",
                   f1(off[r] * RAD2DEG, b1), f1(spr, b2), f1(hys, b3));
    }
    float dsum = wrap_pi(off[1] - off[0]);
    g_off = wrap_pi(off[0] + 0.5f * dsum);
    {
        char b1[16], b2[16];
        bsp_printf("\r\n=> 오프셋 확정 %s deg(전기)  (1회차-2회차 차이 %s)\r\n",
                   f1(g_off * RAD2DEG, b1), f1(dsum * RAD2DEG, b2));
    }
    g_calibrated = 1;
}

/* r: 손 안 대는 주행 */
static void do_handsoff_run(void)
{
    int ok = 0;
    ok |= run_commutation(0.25f, +1, 5000u, "정류 1: 저속 정방향");
    if (ok == 0) { ok |= run_commutation(0.30f, +1, 5000u, "정류 2: 정방향"); }
    if (ok == 0) { coast(1500u); }
    if (ok == 0) { ok |= run_commutation(0.30f, -1, 5000u, "정류 3: 역방향"); }
}

/* b: 브레이크 테스트 — 정방향으로 계속 돈다. 이 동안 아무 때나 손가락으로 누른다 */
static void do_brake_test(void)
{
    run_commutation(0.30f, +1, 20000u, "브레이크 테스트: 정방향 20s — 아무 때나 눌러보세요");
}

static void wait_cmd(uint8_t *ch)
{
    *ch = 0;
    while (HAL_UART_Receive(&huart2, ch, 1, 500) != HAL_OK) { bsp_led_toggle(); }
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== offset calibration + encoder commutation ===\r\n");
    {
        uint8_t st = 0, agc = 0;
        bsp_as5600_magnet_ok(&st);
        bsp_as5600_read_agc(&agc);
        bsp_printf("엔코더: MD=%d AGC=%u\r\n", (st >> 5) & 1, agc);
    }
    bsp_uart_puts("[확인] 12V / 전류 리밋 0.8A / 무부하 / 고정\r\n");

    /* 명령 대기 루프. 실행 사이에는 PWM 을 끈다(로터 자유). 오프셋은 RAM 에 유지 */
    while (1) {
        bsp_uart_puts("\r\n명령: c=캘리브레이션  r=손 안 대는 주행  b=브레이크 테스트(20s)  (실행 중 q=정지)\r\n");
        uint8_t ch;
        wait_cmd(&ch);
        if (ch != 'c' && ch != 'r' && ch != 'b') { continue; }

        bsp_pwm_start();
        if (ch == 'c' || !g_calibrated) { do_calibration(); }
        if (ch == 'r') { do_handsoff_run(); }
        if (ch == 'b') { do_brake_test(); }
        coast(800u);
        bsp_pwm_stop();
        bsp_uart_puts("PWM 정지 (6게이트 off)\r\n");
    }
}
