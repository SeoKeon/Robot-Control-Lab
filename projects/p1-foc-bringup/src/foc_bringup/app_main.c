/**
 * p1 — 전압 모드 정류 + 전류 dq 관측 (Clarke/Park)
 *
 * 정류는 그대로 전압 모드(벡터를 θe ± 90도에, 크기 m)로 돌리고, 매 루프 세 상 전류를
 * 읽어 Clarke → Park 로 id, iq 를 계산해 100 ms 평균으로 찍는다. 아직 제어는 안 한다.
 *   기대: id ≈ 0 (벡터가 q축에 있으므로), iq > 0 정방향 / < 0 역방향,
 *         손가락으로 누르면 iq 증가. 정지 시 iq ≈ Vq/R = 0.5·m·12V / 10.5Ω.
 *   "iq 샘플 σ" 는 평균 전 한 샘플의 흔들림 — 전류 루프가 매 주기 보게 될 노이즈.
 *
 * 각도 지연 보정: 부팅 시 AS5600 필터를 SF=2x 로 바꾸고, 남은 지연은
 *   θ_보정 = θ + ω·τ 로 앞당긴다 (ω 는 사이클 카운터 기반 속도 추정 + 5 ms 평활).
 *   'x' 가 τ 를 0 / 0.4 / 0.8 / 1.2 ms 로 바꿔가며 무부하 id 를 잰다 — id 가 0 을
 *   지나는 τ 가 실제 지연이다.
 *
 * (이전 단계: 오프셋 캘리브레이션 + 정류 단독 = 커밋 2587dce)
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

/* 각도 외삽: θ_보정 = θ + ω·τ. 엔코더 각도가 τ 만큼 늦게 오는 것을 보정한다.
   2026-10-10 실측: AS5600 SF=2x 에서 약 0.85 ms (대부분 I2C 읽기) */
#define W_FILT_S    0.005f    /* 속도 평활 시정수 5 ms */
static float g_tau = 0.0f;    /* [s] */

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
/* ---------------- 전류: 영점, Clarke/Park ---------------- */
static float off_u, off_v, off_w;   /* 영점 [count] */

static void measure_cs_offset(void)    /* PWM off 상태에서 */
{
    double au = 0, av = 0, aw = 0;
    const int n = 2000;
    for (int i = 0; i < n; i++) {
        uint16_t u, v, w;
        bsp_cs_read_raw(&u, &v, &w);
        au += u; av += v; aw += w;
        for (volatile int k = 0; k < 20000; k++) { }   /* 약 0.5 ms */
    }
    off_u = (float)(au / n); off_v = (float)(av / n); off_w = (float)(aw / n);
    char a[16], b[16], c[16];
    bsp_printf("  전류 영점 U/V/W = %s / %s / %s count\r\n", f1(off_u, a), f1(off_v, b), f1(off_w, c));
}

/* 세 상 전류[A] -> id, iq [A]. the = 로터 전기각 */
static void read_dq(float the, float *id, float *iq)
{
    uint16_t ru, rv, rw;
    bsp_cs_read_raw(&ru, &rv, &rw);
    float iu = ((float)ru - off_u) * BSP_CS_A_PER_COUNT;
    float iv = ((float)rv - off_v) * BSP_CS_A_PER_COUNT;
    float iw = ((float)rw - off_w) * BSP_CS_A_PER_COUNT;

    /* Clarke (진폭 불변형): 세 상을 고정 2축으로. 세 상을 다 써서 노이즈를 평균 */
    float ia = (2.0f * iu - iv - iw) * (1.0f / 3.0f);
    float ib = (iv - iw) * 0.57735027f;              /* 1/sqrt(3) */

    /* Park: 로터 각도로 돌려 로터와 같이 도는 축으로 */
    float c = cosf(the), s = sinf(the);
    *id =  ia * c + ib * s;
    *iq = -ia * s + ib * c;
}

static int run_commutation(float m, int lead, uint32_t ms, const char *label)
{
    bsp_printf("\r\n[%s] m=%d%%  lead %c90deg  %lus\r\n",
               label, (int)(m * 100.0f), lead > 0 ? '+' : '-', (unsigned long)(ms / 1000u));
    bsp_uart_puts("      t       rpm     id[mA]   iq[mA]   iq 샘플 σ[mA]   loop\r\n");

    uint32_t t0 = HAL_GetTick(), tlog = t0, loops = 0;
    int32_t  acc_log = enc_acc;
    double   s_id = 0, s_iq = 0, s_iq2 = 0;

    /* 속도 추정: 루프 간격은 사이클 카운터로(µs 단위), 엔코더 변화량은 누적 카운트로 */
    uint32_t cyc_prev = DWT->CYCCNT;
    int32_t  acc_prev = enc_acc;
    float    w_e      = 0.0f;           /* 전기 각속도 [rad/s], 평활 */

    while (HAL_GetTick() - t0 < ms) {
        enc_update();

        uint32_t cyc = DWT->CYCCNT;
        float dt_s = (float)(cyc - cyc_prev) / (float)SystemCoreClock;
        cyc_prev = cyc;
        if (dt_s > 0.0f) {
            float w_inst = (float)(enc_acc - acc_prev) * (TWO_PI / (float)CPR) / dt_s
                           * (float)g_sign * (float)PP;
            float a = dt_s / (W_FILT_S + dt_s);          /* 1차 저역통과 */
            w_e += a * (w_inst - w_e);
        }
        acc_prev = enc_acc;

        /* 엔코더 각도는 g_tau 만큼 늦다 -> 그동안 더 돌아간 만큼 앞당긴다 */
        float the = (float)g_sign * (float)PP * mech_rad() + g_off + w_e * g_tau;
        bsp_pwm_set_vector(the + (float)lead * HALF_PI, m);

        float id, iq;
        read_dq(the, &id, &iq);
        s_id += id; s_iq += iq; s_iq2 += (double)iq * iq;
        loops++;

        uint32_t now = HAL_GetTick();
        if (now - tlog >= 100u) {
            float dt  = (float)(now - tlog) / 1000.0f;
            float dps = (float)(enc_acc - acc_log) * 360.0f / (float)CPR / dt;
            float rpm = dps / 6.0f;
            float mid = (float)(s_id / loops), miq = (float)(s_iq / loops);
            float siq = sqrtf((float)(s_iq2 / loops) - miq * miq);
            char b1[16], b2[16], b3[16], b4[16];
            bsp_printf("  %5lu ms  %7s  %7s  %7s  %7s        %4lu Hz\r\n",
                       (unsigned long)(now - t0), f1(rpm, b1),
                       f1(mid * 1000.0f, b2), f1(miq * 1000.0f, b3), f1(siq * 1000.0f, b4),
                       (unsigned long)((float)loops / dt));
            acc_log = enc_acc; tlog = now; loops = 0;
            s_id = s_iq = s_iq2 = 0;

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

/* f: AS5600 필터 비교 — 지연(회전 중 id) vs 노이즈(정지 σ) */
typedef struct { uint8_t sf, fth; const char *name; } enc_filter_t;
static const enc_filter_t FILTERS[] = {
    { 0u, 0u, "A: SF=16x (기본)" },
    { 3u, 0u, "B: SF=2x" },
    { 0u, 1u, "C: SF=16x + FTH=6LSB" },
};
#define N_FILTERS (sizeof(FILTERS) / sizeof(FILTERS[0]))

static void angle_noise_at_rest(float *sigma_cnt)   /* PWM off, 로터 정지 상태에서 */
{
    uint16_t r0 = 0;
    bsp_as5600_read_raw(&r0);
    double s = 0, q = 0;
    const int n = 300;
    for (int i = 0; i < n; i++) {
        uint16_t r = 0;
        bsp_as5600_read_raw(&r);
        int32_t d = (int32_t)r - (int32_t)r0;          /* 0/4095 경계 대비 */
        if (d >  CPR / 2) { d -= CPR; }
        if (d < -CPR / 2) { d += CPR; }
        s += d; q += (double)d * d;
    }
    float m = (float)(s / n);
    *sigma_cnt = sqrtf((float)(q / n) - m * m);
}

static void do_filter_compare(void)
{
    char b1[16], b2[16];
    uint16_t conf;

    bsp_uart_puts("\r\n[필터 비교 1/2] 정지 상태 각도 노이즈 (PWM off)\r\n");
    bsp_pwm_stop();
    HAL_Delay(300);
    for (uint32_t k = 0; k < N_FILTERS; k++) {
        float sg;
        bsp_as5600_set_filter(FILTERS[k].sf, FILTERS[k].fth, &conf);
        HAL_Delay(20);
        angle_noise_at_rest(&sg);
        bsp_printf("  %-22s CONF=0x%04X  σ = %s count (= %s mdeg 기계)\r\n",
                   FILTERS[k].name, conf, f1(sg, b1), f1(sg * 360.0f / CPR * 1000.0f, b2));
    }

    bsp_uart_puts("\r\n[필터 비교 2/2] 회전 중 dq (m=0.30 정방향, 각 4s)\r\n");
    bsp_pwm_start();
    int ok = 0;
    for (uint32_t k = 0; k < N_FILTERS && ok == 0; k++) {
        bsp_as5600_set_filter(FILTERS[k].sf, FILTERS[k].fth, &conf);
        ok = run_commutation(0.30f, +1, 4000u, FILTERS[k].name);
    }
    bsp_as5600_set_filter(3u, 0u, &conf);
    bsp_printf("\r\n필터 SF=2x 로 복귀 (CONF=0x%04X)\r\n", conf);
}

/* x: 각도 외삽 τ 를 바꿔가며 무부하 id 를 잰다. id 가 0 을 지나는 τ = 실제 지연 */
static void do_tau_sweep(void)
{
    static const float taus_ms[] = { 0.0f, 0.4f, 0.8f, 1.2f };
    char b1[16], name[48];
    int ok = 0;

    bsp_uart_puts("\r\n[τ 스윕] 엔코더 필터 SF=2x, m=0.30, 각 3s\r\n");
    for (uint32_t k = 0; k < sizeof(taus_ms) / sizeof(taus_ms[0]) && ok == 0; k++) {
        g_tau = taus_ms[k] / 1000.0f;
        snprintf(name, sizeof name, "정방향 tau=%s ms", f1(taus_ms[k], b1));
        ok = run_commutation(0.30f, +1, 3000u, name);
    }
    if (ok == 0) {
        coast(1500u);
        g_tau = 0.8f / 1000.0f;
        ok = run_commutation(0.30f, -1, 3000u, "역방향 tau=0.8 ms");
    }
    g_tau = 0.0f;
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

    /* 전류 센싱: PWM 출력을 켜기 전에 트리거를 꼭대기에 정렬 */
    uint32_t tc = 0;
    int r = bsp_cs_init(&tc);
    bsp_printf("ADC 트리거 정렬: %s (CNT=%lu)\r\n", r == 0 ? "꼭대기 OK" : "실패 !!", (unsigned long)tc);

    /* 루프 간격을 µs 단위로 재려고 사이클 카운터를 켠다 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* AS5600 필터를 2x 로 (기본 16x 는 회전 중 각도를 약 1.7 ms 늦게 준다). 휘발성이라 매 부팅 */
    {
        uint16_t conf = 0;
        int fr = bsp_as5600_set_filter(3u, 0u, &conf);
        bsp_printf("엔코더 필터 SF=2x: %s (CONF=0x%04X)\r\n", fr == 0 ? "OK" : "실패 !!", conf);
    }

    /* 명령 대기 루프. 실행 사이에는 PWM 을 끈다(로터 자유). 오프셋은 RAM 에 유지 */
    while (1) {
        bsp_uart_puts("\r\n명령: c=캘리브레이션  r=손 안 대는 주행  b=브레이크 테스트(20s)  f=엔코더 필터 비교  x=각도 외삽 τ 스윕  (실행 중 q=정지)\r\n");
        uint8_t ch;
        wait_cmd(&ch);
        if (ch != 'c' && ch != 'r' && ch != 'b' && ch != 'f' && ch != 'x') { continue; }

        measure_cs_offset();   /* PWM off 인 지금 영점을 새로 잰다 (온도 드리프트 대비) */
        bsp_pwm_start();
        if (ch == 'c' || !g_calibrated) { do_calibration(); }
        if (ch == 'r') { do_handsoff_run(); }
        if (ch == 'b') { do_brake_test(); }
        if (ch == 'f') { do_filter_compare(); }
        if (ch == 'x') { do_tau_sweep(); }
        coast(800u);
        bsp_pwm_stop();
        bsp_uart_puts("PWM 정지 (6게이트 off)\r\n");
    }
}
