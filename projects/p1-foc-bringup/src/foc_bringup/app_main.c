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
 * 전류 루프 (FOC): 'i' / 'k'. id 지령 0, iq 지령으로 토크를 직접 명령한다.
 *   V = 피드포워드(R·i_ref + ψ·ω) + 적분기. 전압 m ≤ 0.40, 포화 중 적분 정지.
 *   'k' 에서 잡았을 때 iq 가 지령(100 mA)에 머물면 성공 — 전압 모드는 같은 상황에서
 *   V/R 까지 올라갔다.
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
#define TAU_LAG_S   0.00085f  /* τ 스윕 실측: id 가 0 을 지나는 점 0.87 ms */
static float g_tau = TAU_LAG_S;   /* [s] */

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

/* ---------------- 전류 루프 (FOC) ----------------
 * 플랜트: 이 짐벌모터는 L/R 이 루프 주기(약 0.54 ms)보다 짧아서 사실상 저항이다.
 *   다음 루프에서 측정되는 전류 ≈ (V − E) / R  (한 루프 지연)
 * 제어기 (d, q 각각):
 *   V = 피드포워드 + 적분기
 *   피드포워드 Vq = R·iq_ref + ψ·ω   (필요한 전압을 미리 계산 — 대부분을 맡는다)
 *   적분기      += KI·오차            (나머지만 천천히. KI·(1/R) = 0.2 → 루프마다 오차 20% 감소)
 * 전압 제한은 d축 우선: |Vd| ≤ Vmax 를 먼저 지키고, 남는 전압 sqrt(Vmax² − Vd²) 안에서 Vq.
 * 포화된 축은 오차가 포화를 더 키우는 방향일 때 적분을 멈춘다 (anti-windup).
 *
 * 속도 루프 (캐스케이드 바깥): 속도 PI → iq 지령 (±IQ_LIM 으로 제한) → 위 전류 루프.
 *   플랜트 J·dω/dt = Kt·iq − 마찰.  J ≈ 5e-6 kg·m² (가속 데이터 역산), Kt = 1.5·PP·ψ.
 *   대역폭 목표 ωc → Kp = J·ωc/Kt, Ki = Kp·ωc/4. 전류 루프(수 ms)보다 충분히 느리게 둔다.
 */
#define R_PHASE   10.5f       /* Ω, 실측 상저항 (상간 21Ω / 2) */
#define PSI_V_S   0.00814f    /* 역기전력 상수 [V·s/rad 전기], τ 스윕 데이터에서 계산 */
#define V_BUS     12.0f
#define KI_V_A    (0.2f * R_PHASE)   /* 전류 적분 게인 [V/A per loop] */
#define M_MAX     0.40f       /* 전압 크기 상한 (상 진폭 = 0.5·m·Vbus = 2.4 V) */
#define I_TRIP_A  0.30f       /* 100 ms 평균 |iq| 가 이걸 넘으면 정지 */

#define J_KGM2    5.0e-6f     /* 로터 관성 (추정) */
#define KT_NM_A   (1.5f * (float)PP * PSI_V_S)   /* 토크 상수 ≈ 0.086 N·m/A */
#define BW_SPD_HZ 15.0f       /* 속도 루프 기본 목표 대역폭 → Kp ≈ 5.5e-3 A/(rad/s). 5 Hz 는 한 바퀴 부하를 못 누른다 */
#define IQ_LIM    0.15f       /* 속도 루프가 낼 수 있는 최대 iq [A] */

#define MODE_CURRENT 0
#define MODE_SPEED   1
#define MODE_POS     2

typedef struct {
    float    ref;        /* 전류: iq [A] · 속도: rpm · 위치: 목표각 [deg, 실행 시작 위치 기준] */
    int      mode;       /* MODE_CURRENT / MODE_SPEED / MODE_POS */
    uint32_t ms;
    const char *label;
    float    bw_hz;      /* 속도 루프 대역폭, 0 이면 BW_SPD_HZ */
    int      raw_step;   /* 위치: 1 이면 궤적 없이 목표를 바로 준다 (계단) */
} cl_step_t;

/* ---------------- 위치 루프 (캐스케이드 맨 바깥) ----------------
 * 위치 P → 속도 지령 → 속도 PI → iq 지령 → 전류 루프.
 *   ω_ref = v_ff + KP_POS·(θ_궤적 − θ)       (속도 피드포워드 + 위치 P, ±POS_WLIM 으로 제한)
 *   iq_ff = J·a_ff / Kt                       (가속 피드포워드 — 궤적이 원하는 가속에 필요한 토크)
 * 위치 루프 대역폭(4 Hz)은 속도 루프(15 Hz)의 약 1/4. 속도 PI 의 적분기 덕분에 일정한 부하가
 * 걸려도 정지 위치 오차는 0 으로 간다. 목표는 사다리꼴 궤적(최고 속도·가속도 제한)으로 준다.
 */
#define KP_POS       (TWO_PI * 4.0f)              /* [1/s] 오차 1도 → 속도 지령 약 4 rpm */
#define POS_VMAX     (120.0f * TWO_PI / 60.0f)    /* 궤적 최고 속도 120 rpm [rad/s] */
#define POS_AMAX     300.0f                       /* 궤적 가속도 [rad/s²] → 가속 FF ≈ 17.5 mA */
#define POS_WLIM     (200.0f * TWO_PI / 60.0f)    /* 위치 루프가 내는 속도 지령 한계 200 rpm */
#define POS_ERR_TRIP (90.0f * DEG2RAD)            /* 오차가 이보다 크면 정지 (안전) */

/* 사다리꼴 궤적: 정지 → 등가속 → 등속 → 등감속 → 정지. 거리가 짧으면 삼각형(등속 없음) */
typedef struct { float p0, d, sgn, ta, tc, vp, a; } traj_t;

static void traj_plan(traj_t *tj, float p0, float p1, float vmax, float amax)
{
    float D = p1 - p0;
    tj->p0 = p0; tj->sgn = (D >= 0.0f) ? 1.0f : -1.0f; tj->d = fabsf(D); tj->a = amax;
    if (tj->d * amax < vmax * vmax) { tj->vp = sqrtf(tj->d * amax); tj->tc = 0.0f; }
    else                            { tj->vp = vmax; tj->tc = (tj->d - vmax * vmax / amax) / vmax; }
    tj->ta = tj->vp / amax;
}

static void traj_eval(const traj_t *tj, float t, float *p, float *v, float *a)
{
    const float ta = tj->ta, tc = tj->tc, A = tj->a, vp = tj->vp;
    float s, ds, dds;
    if (t <= 0.0f)                 { s = 0.0f; ds = 0.0f; dds = 0.0f; }
    else if (t < ta)               { s = 0.5f * A * t * t; ds = A * t; dds = A; }
    else if (t < ta + tc)          { s = 0.5f * A * ta * ta + vp * (t - ta); ds = vp; dds = 0.0f; }
    else if (t < 2.0f * ta + tc)   { float r = 2.0f * ta + tc - t; s = tj->d - 0.5f * A * r * r; ds = A * r; dds = -A; }
    else                           { s = tj->d; ds = 0.0f; dds = 0.0f; }
    *p = tj->p0 + tj->sgn * s; *v = tj->sgn * ds; *a = tj->sgn * dds;
}

static const char *f2(float x, char *b)
{
    long t = lroundf(x * 100.0f);
    snprintf(b, 16, "%s%ld.%02ld", (t < 0) ? "-" : "", labs(t) / 100, labs(t) % 100);
    return b;
}

/* 고해상도 기록: 단계마다 처음 CAP_N 칸, 한 칸 = g_cap_dt ms 안의 루프 샘플 평균.
 * 실행이 끝난 뒤(PWM 정지 후) 출력한다. 속도는 제어기가 보는 값(5 ms 필터 후) 그대로.
 *   속도 모드: a = rpm, b = iq 지령, c = iq        (0.1 단위)
 *   위치 모드: a = 오차 [0.01도], b = rpm, c = iq 지령, d = 위치 [0.1도] */
#define CAP_STEPS 6
#define CAP_N     300u
typedef struct { uint16_t ms; int16_t a, b, c, d; } cap_t;
static cap_t    g_cap[CAP_STEPS][CAP_N];
static uint16_t g_cap_n[CAP_STEPS];
static float    g_cap_ref[CAP_STEPS];
static int      g_cap_mode[CAP_STEPS];
static uint32_t g_cap_dt = 1u;
static int      g_cap_on, g_cap_pending;

static int run_loop_body(const cl_step_t *steps, int n_steps);

/* 루프 중에는 송신을 인터럽트 큐로 — 블로킹 송신은 로그 한 줄마다 루프를 약 6 ms 멈춘다 */
static int run_current_loop(const cl_step_t *steps, int n_steps)
{
    bsp_uart_set_async(1);
    int r = run_loop_body(steps, n_steps);
    bsp_pwm_set_vector(0.0f, 0.0f);
    uint32_t dropped = bsp_uart_set_async(0);
    if (dropped) { bsp_printf("  (송신 큐 넘침: 누적 %lu 바이트 버림)\r\n", (unsigned long)dropped); }
    return r;
}

static int16_t sat16(float x)
{
    return (int16_t)((x > 32767.0f) ? 32767.0f : ((x < -32768.0f) ? -32768.0f : x));
}

static int run_loop_body(const cl_step_t *steps, int n_steps)
{
    float vd_i = 0.0f, vq_i = 0.0f;    /* 전류 적분기 [V] — 단계가 바뀌어도 이어간다 */
    float w_i  = 0.0f;                 /* 속도 적분기 [A] */
    uint32_t cyc_prev = DWT->CYCCNT;
    int32_t  acc_prev = enc_acc;
    float    w_e      = 0.0f;

    const int32_t acc0  = enc_acc;     /* 위치 0 = 실행 시작 위치 (엔코더 방향, 멀티턴) */
    float         p_cmd = 0.0f;        /* 궤적이 지금 가리키는 위치 [rad] — 단계가 바뀌어도 이어간다 */

    for (int sidx = 0; sidx < n_steps; sidx++) {
        const cl_step_t *st = &steps[sidx];
        char b0[16];
        const float wc   = TWO_PI * ((st->bw_hz > 0.0f) ? st->bw_hz : BW_SPD_HZ);
        const float kp_w = J_KGM2 * wc / KT_NM_A;   /* [A per rad/s 기계] */
        const float ki_w = kp_w * wc / 4.0f;        /* [A per rad 기계] */
        const float p_tgt = st->ref * DEG2RAD;
        traj_t tj;
        traj_plan(&tj, p_cmd, (st->raw_step ? p_cmd : p_tgt), POS_VMAX, POS_AMAX);

        if (st->mode == MODE_POS) {
            char bk[16];
            bsp_printf("\r\n[%s] 목표 %s deg (%s), 위치 Kp %d /s, 속도 루프 %s Hz, %lus\r\n",
                       st->label, f1(st->ref, b0),
                       st->raw_step ? "궤적 없이 계단" : "사다리꼴 궤적 120 rpm, 300 rad/s²",
                       (int)KP_POS, f1(wc / TWO_PI, bk), (unsigned long)(st->ms / 1000u));
            bsp_uart_puts("      t    궤적[deg]  위치[deg]  오차[deg] |오차|최대     rpm   iq지령   iq[mA]  loop\r\n");
        } else {
            if (st->mode == MODE_SPEED) {
                char bk[16];
                bsp_printf("\r\n[%s] 속도 지령 %s rpm, 대역폭 %s Hz (Kp %d.%02d mA/(rad/s)), iq 한계 ±%d mA, %lus\r\n",
                           st->label, f1(st->ref, b0), f1(wc / TWO_PI, bk),
                           (int)(kp_w * 1000.0f), (int)(kp_w * 100000.0f) % 100,
                           (int)(IQ_LIM * 1000.0f), (unsigned long)(st->ms / 1000u));
            } else {
                bsp_printf("\r\n[%s] iq 지령 %s mA, id 지령 0, %lus\r\n",
                           st->label, f1(st->ref * 1000.0f, b0), (unsigned long)(st->ms / 1000u));
            }
            bsp_uart_puts("      t      지령      rpm   iq지령   iq[mA]   id[mA]    m[%]  포화[%]  loop\r\n");
        }

        /* 속도 지령: 엔코더 rpm 기준 -> 전기 방향 기준 기계 rad/s */
        const float w_ref_spd = st->ref * (TWO_PI / 60.0f) * (float)g_sign;

        uint32_t t0 = HAL_GetTick(), tlog = t0, loops = 0, nsat = 0;
        int32_t  acc_log = enc_acc;
        double   s_id = 0, s_iq = 0, s_m = 0, s_iqr = 0;
        float    t_step = 0.0f, e_p = 0.0f, e_max = 0.0f;

        const int cap = g_cap_on && sidx < CAP_STEPS;
        uint32_t  c_slot = 0;
        float     c_a = 0, c_b = 0, c_c = 0, c_d = 0;
        int       c_k = 0;
        if (cap) { g_cap_n[sidx] = 0; g_cap_ref[sidx] = st->ref; g_cap_mode[sidx] = st->mode; }

        while (HAL_GetTick() - t0 < st->ms) {
            enc_update();

            uint32_t cyc = DWT->CYCCNT;
            float dt_s = (float)(cyc - cyc_prev) / (float)SystemCoreClock;
            cyc_prev = cyc;
            if (dt_s > 0.0f) {
                float w_inst = (float)(enc_acc - acc_prev) * (TWO_PI / (float)CPR) / dt_s
                               * (float)g_sign * (float)PP;
                w_e += dt_s / (W_FILT_S + dt_s) * (w_inst - w_e);
            }
            acc_prev = enc_acc;
            t_step += dt_s;

            float the = (float)g_sign * (float)PP * mech_rad() + g_off + w_e * g_tau;

            /* 측정 */
            float id, iq;
            read_dq(the, &id, &iq);

            /* 맨 바깥: 위치 P (+ 궤적 피드포워드) -> 속도 지령 */
            float w_ref = w_ref_spd, iq_ff = 0.0f;
            float pos = (float)(enc_acc - acc0) * (TWO_PI / (float)CPR);
            if (st->mode == MODE_POS) {
                float v_ff = 0.0f, a_ff = 0.0f;
                if (st->raw_step) { p_cmd = p_tgt; }
                else              { traj_eval(&tj, t_step, &p_cmd, &v_ff, &a_ff); }
                e_p = p_cmd - pos;
                if (fabsf(e_p) > e_max) { e_max = fabsf(e_p); }
                if (fabsf(e_p) > POS_ERR_TRIP) { bsp_uart_puts("  !! 위치 오차 90도 초과 — 정지\r\n"); return -1; }
                float w_enc = v_ff + KP_POS * e_p;
                w_enc = (w_enc > POS_WLIM) ? POS_WLIM : ((w_enc < -POS_WLIM) ? -POS_WLIM : w_enc);
                w_ref = w_enc * (float)g_sign;
                iq_ff = J_KGM2 * a_ff / KT_NM_A * (float)g_sign;
            }

            /* 가운데: 속도 PI (+ 가속 피드포워드) -> iq 지령 */
            float iq_ref;
            if (st->mode != MODE_CURRENT) {
                float w_m   = w_e / (float)PP;               /* 기계 rad/s (전기 방향 기준) */
                float e_w   = w_ref - w_m;
                float iq_un = kp_w * e_w + w_i + iq_ff;
                iq_ref = (iq_un > IQ_LIM) ? IQ_LIM : ((iq_un < -IQ_LIM) ? -IQ_LIM : iq_un);
                int limited = (iq_ref != iq_un);
                if (!limited || (e_w * iq_ref < 0.0f)) { w_i += ki_w * e_w * dt_s; }
            } else {
                iq_ref = st->ref;
            }

            /* 안쪽: 전류 (피드포워드 + 적분) */
            float ed = 0.0f - id, eq = iq_ref - iq;
            float vd = vd_i;
            float vq = vq_i + R_PHASE * iq_ref + PSI_V_S * w_e;

            /* d축 우선 전압 제한 */
            float vmax = 0.5f * M_MAX * V_BUS;
            int sat_d = 0, sat_q = 0;
            if (vd >  vmax) { vd =  vmax; sat_d = 1; }
            if (vd < -vmax) { vd = -vmax; sat_d = 1; }
            float vqmax = sqrtf(vmax * vmax - vd * vd);
            if (vq >  vqmax) { vq =  vqmax; sat_q = 1; }
            if (vq < -vqmax) { vq = -vqmax; sat_q = 1; }
            if (!sat_d || ed * vd < 0.0f) { vd_i += KI_V_A * ed; }
            if (!sat_q || eq * vq < 0.0f) { vq_i += KI_V_A * eq; }
            if (sat_d || sat_q) { nsat++; }

            float vmag = sqrtf(vd * vd + vq * vq);
            float m = vmag / (0.5f * V_BUS);
            bsp_pwm_set_vector(the + atan2f(vq, vd), m);

            s_id += id; s_iq += iq; s_m += m; s_iqr += iq_ref;
            loops++;

            if (cap && g_cap_n[sidx] < CAP_N) {
                uint32_t slot = (HAL_GetTick() - t0) / g_cap_dt;
                if (slot != c_slot && c_k > 0) {
                    cap_t *c = &g_cap[sidx][g_cap_n[sidx]++];
                    float k = (float)c_k;
                    c->ms = (uint16_t)(c_slot * g_cap_dt);
                    c->a = sat16(c_a / k); c->b = sat16(c_b / k); c->c = sat16(c_c / k); c->d = sat16(c_d / k);
                    c_a = c_b = c_c = c_d = 0.0f; c_k = 0; c_slot = slot;
                }
                float rpm_now = w_e / (float)PP * (float)g_sign * (60.0f / TWO_PI);
                if (st->mode == MODE_POS) {
                    c_a += e_p * RAD2DEG * 100.0f; c_b += rpm_now * 10.0f;
                    c_c += iq_ref * 10000.0f;      c_d += pos * RAD2DEG * 10.0f;
                } else {
                    c_a += rpm_now * 10.0f; c_b += iq_ref * 10000.0f; c_c += iq * 10000.0f;
                }
                c_k++;
            }

            uint32_t now = HAL_GetTick();
            if (now - tlog >= 100u) {
                float dt  = (float)(now - tlog) / 1000.0f;
                float rpm = (float)(enc_acc - acc_log) * 360.0f / (float)CPR / dt / 6.0f;
                float mid = (float)(s_id / loops), miq = (float)(s_iq / loops);
                char b1[16], b2[16], b3[16], b4[16], b5[16], b6[16];
                if (st->mode == MODE_POS) {
                    char b7[16];
                    bsp_printf("  %5lu ms  %8s  %8s  %8s  %8s  %7s  %7s  %7s  %4lu Hz\r\n",
                               (unsigned long)(now - t0), f1(p_cmd * RAD2DEG, b1), f1(pos * RAD2DEG, b2),
                               f2(e_p * RAD2DEG, b3), f2(e_max * RAD2DEG, b4), f1(rpm, b5),
                               f1((float)(s_iqr / loops) * 1000.0f, b6), f1(miq * 1000.0f, b7),
                               (unsigned long)((float)loops / dt));
                    e_max = 0.0f;
                } else {
                    float ref_show = (st->mode == MODE_SPEED) ? st->ref : st->ref * 1000.0f;
                    bsp_printf("  %5lu ms  %7s  %7s  %7s  %7s  %7s  %6s  %5lu  %4lu Hz\r\n",
                               (unsigned long)(now - t0), f1(ref_show, b6), f1(rpm, b1),
                               f1((float)(s_iqr / loops) * 1000.0f, b2),
                               f1(miq * 1000.0f, b3), f1(mid * 1000.0f, b4),
                               f1((float)(s_m / loops) * 100.0f, b5),
                               (unsigned long)(100u * nsat / loops), (unsigned long)((float)loops / dt));
                }
                acc_log = enc_acc; tlog = now; loops = 0; nsat = 0;
                s_id = s_iq = s_m = s_iqr = 0;

                if (fabsf(rpm) > (float)RPM_LIMIT) { bsp_uart_puts("  !! 속도 한계 초과 — 정지\r\n"); return -1; }
                if (fabsf(miq) > I_TRIP_A)          { bsp_uart_puts("  !! 전류 한계 초과 — 정지\r\n"); return -1; }
                if (quit_requested())               { bsp_uart_puts("  !! 'q' 수신 — 정지\r\n");   return -1; }
            }
        }
    }
    return 0;
}

/* i: 손 안 대는 전류 루프 확인 */
static void do_current_loop_handsoff(void)
{
    static const cl_step_t seq[] = {
        {  0.000f, MODE_CURRENT, 2000u, "전류 1: iq=0 (토크 0, 그대로 있어야)" },
        {  0.040f, MODE_CURRENT, 4000u, "전류 2: iq=+40mA (가속)" },
        { -0.040f, MODE_CURRENT, 4000u, "전류 3: iq=-40mA (감속 후 역회전)" },
        {  0.000f, MODE_CURRENT, 2000u, "전류 4: iq=0 (토크 0, 관성으로 감속)" },
    };
    run_current_loop(seq, (int)(sizeof(seq) / sizeof(seq[0])));
}

/* k: 전류 루프 브레이크 테스트 — 잡아도 iq 가 지령에 머무는지 */
static void do_current_loop_brake(void)
{
    static const cl_step_t seq[] = {
        { 0.100f, MODE_CURRENT, 20000u, "전류 브레이크: iq=+100mA 20s — 아무 때나 잡아보세요" },
    };
    run_current_loop(seq, 1);
}

/* s: 속도 루프 계단 응답 (손 안 댐) */
static void do_speed_steps(void)
{
    static const cl_step_t seq[] = {
        {    0.0f, MODE_SPEED, 2000u, "속도 1: 0 rpm 유지" },
        {  100.0f, MODE_SPEED, 3000u, "속도 2: 100 rpm" },
        {  200.0f, MODE_SPEED, 3000u, "속도 3: 200 rpm" },
        { -150.0f, MODE_SPEED, 3000u, "속도 4: -150 rpm (역방향)" },
        {    0.0f, MODE_SPEED, 2000u, "속도 5: 0 rpm" },
    };
    g_cap_on = 1; g_cap_dt = 1u;
    for (int s = 0; s < CAP_STEPS; s++) { g_cap_n[s] = 0; }
    run_current_loop(seq, (int)(sizeof(seq) / sizeof(seq[0])));
    g_cap_on = 0;
    g_cap_pending = 1;
}

/* p: 위치 루프 (손 안 댐) — 궤적 이동, 한 바퀴 넘는 멀티턴, 복귀, 궤적 없는 계단과 비교 */
static void do_position_moves(void)
{
    static const cl_step_t seq[] = {
        {   0.0f, MODE_POS, 2000u, "위치 1: 0도 유지" },
        {  90.0f, MODE_POS, 2000u, "위치 2: +90도" },
        { 450.0f, MODE_POS, 3000u, "위치 3: +450도 (한 바퀴 더)" },
        {   0.0f, MODE_POS, 3000u, "위치 4: 0도로 복귀 (-450도)" },
        {  30.0f, MODE_POS, 2000u, "위치 5: +30도 계단", 0.0f, 1 },
        {   0.0f, MODE_POS, 2000u, "위치 6: 0도" },
    };
    g_cap_on = 1; g_cap_dt = 3u;
    for (int s = 0; s < CAP_STEPS; s++) { g_cap_n[s] = 0; }
    run_current_loop(seq, (int)(sizeof(seq) / sizeof(seq[0])));
    g_cap_on = 0;
    g_cap_pending = 1;
}

/* h: 위치 유지 브레이크 테스트 — 밀면 버티고, 놓으면 제자리로 돌아오는지 */
static void do_position_hold(void)
{
    static const cl_step_t seq[] = {
        { 0.0f, MODE_POS, 20000u, "위치 유지: 0도 20s — 밀었다 놓아보세요 (90도 넘게 밀면 정지)" },
    };
    run_current_loop(seq, 1);
}

/* 고해상도 기록 출력 — PWM 을 끈 뒤에 부른다 (출력에 수 초 걸림) */
static void dump_capture(void)
{
    for (int s = 0; s < CAP_STEPS; s++) {
        if (g_cap_n[s] == 0) { continue; }
        char b0[16];
        if (g_cap_mode[s] == MODE_POS) {
            bsp_printf("\r\n[%lums 기록 %d] 목표 %s deg, %u 샘플\r\n   ms  위치[deg]  오차[deg]    rpm   iq지령\r\n",
                       (unsigned long)g_cap_dt, s + 1, f1(g_cap_ref[s], b0), (unsigned)g_cap_n[s]);
        } else {
            bsp_printf("\r\n[%lums 기록 %d] 지령 %s rpm, %u 샘플\r\n   ms     rpm   iq지령   iq[mA]\r\n",
                       (unsigned long)g_cap_dt, s + 1, f1(g_cap_ref[s], b0), (unsigned)g_cap_n[s]);
        }
        for (int i = 0; i < g_cap_n[s]; i++) {
            const cap_t *c = &g_cap[s][i];
            char b1[16], b2[16], b3[16], b4[16];
            if (g_cap_mode[s] == MODE_POS) {
                bsp_printf("  %3u  %8s  %8s  %7s  %7s\r\n", (unsigned)c->ms,
                           f1((float)c->d / 10.0f, b1), f2((float)c->a / 100.0f, b2),
                           f1((float)c->b / 10.0f, b3), f1((float)c->c / 10.0f, b4));
            } else {
                bsp_printf("  %3u  %7s  %7s  %7s\r\n", (unsigned)c->ms,
                           f1((float)c->a / 10.0f, b1), f1((float)c->b / 10.0f, b2),
                           f1((float)c->c / 10.0f, b3));
            }
        }
    }
    bsp_uart_puts("[기록 끝]\r\n");
}

/* v: 대역폭 비교 — 같은 100 rpm 에서 게인만 바꿔 한 바퀴 주기 속도 출렁임이 줄어드는지 */
static void do_speed_bw_compare(void)
{
    static const cl_step_t seq[] = {
        { 100.0f, MODE_SPEED, 4000u, "대역폭 1: 5 Hz",  5.0f },
        { 100.0f, MODE_SPEED, 4000u, "대역폭 2: 10 Hz", 10.0f },
        { 100.0f, MODE_SPEED, 4000u, "대역폭 3: 15 Hz", 15.0f },
        {   0.0f, MODE_SPEED, 2000u, "대역폭 4: 0 rpm 정지 (15 Hz)", 15.0f },
    };
    run_current_loop(seq, (int)(sizeof(seq) / sizeof(seq[0])));
}

/* w: 속도 루프 브레이크 테스트 — 눌러도 속도를 지키는지 */
static void do_speed_brake(void)
{
    static const cl_step_t seq[] = {
        { 150.0f, MODE_SPEED, 20000u, "속도 브레이크: 150 rpm 20s — 아무 때나 눌러보세요" },
    };
    run_current_loop(seq, 1);
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

    bsp_uart_puts("\r\n[필터 비교 2/2] 회전 중 dq (m=0.30 정방향, 각 4s, 외삽 보정 끔)\r\n");
    bsp_pwm_start();
    g_tau = 0.0f;   /* 필터 자체의 지연을 보려는 실험이라 보정 없이 */
    int ok = 0;
    for (uint32_t k = 0; k < N_FILTERS && ok == 0; k++) {
        bsp_as5600_set_filter(FILTERS[k].sf, FILTERS[k].fth, &conf);
        ok = run_commutation(0.30f, +1, 4000u, FILTERS[k].name);
    }
    bsp_as5600_set_filter(3u, 0u, &conf);
    g_tau = TAU_LAG_S;
    bsp_printf("\r\n필터 SF=2x, 외삽 보정 복귀 (CONF=0x%04X)\r\n", conf);
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
    g_tau = TAU_LAG_S;
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
        bsp_uart_puts("\r\n명령: c=캘리브레이션  r=손 안 대는 주행  b=브레이크 테스트(20s)  f=엔코더 필터 비교  x=각도 외삽 τ 스윕\r\n"
                      "      i=전류 루프 (손 안 댐)  k=전류 루프 브레이크 테스트(20s)\r\n"
                      "      s=속도 루프 계단 응답 (손 안 댐)  v=속도 대역폭 비교  w=속도 루프 브레이크 테스트(20s)\r\n"
                      "      p=위치 루프 이동 (손 안 댐)  h=위치 유지 브레이크 테스트(20s)  (실행 중 q=정지)\r\n");
        uint8_t ch;
        wait_cmd(&ch);
        if (ch != 'c' && ch != 'r' && ch != 'b' && ch != 'f' && ch != 'x' && ch != 'i' && ch != 'k' && ch != 's' && ch != 'v' && ch != 'w' && ch != 'p' && ch != 'h') { continue; }

        measure_cs_offset();   /* PWM off 인 지금 영점을 새로 잰다 (온도 드리프트 대비) */
        bsp_pwm_start();
        if (ch == 'c' || !g_calibrated) { do_calibration(); }
        if (ch == 'r') { do_handsoff_run(); }
        if (ch == 'b') { do_brake_test(); }
        if (ch == 'f') { do_filter_compare(); }
        if (ch == 'x') { do_tau_sweep(); }
        if (ch == 'i') { do_current_loop_handsoff(); }
        if (ch == 'k') { do_current_loop_brake(); }
        if (ch == 's') { do_speed_steps(); }
        if (ch == 'v') { do_speed_bw_compare(); }
        if (ch == 'w') { do_speed_brake(); }
        if (ch == 'p') { do_position_moves(); }
        if (ch == 'h') { do_position_hold(); }
        coast(800u);
        bsp_pwm_stop();
        bsp_uart_puts("PWM 정지 (6게이트 off)\r\n");
        if (g_cap_pending) { g_cap_pending = 0; dump_capture(); }
    }
}
