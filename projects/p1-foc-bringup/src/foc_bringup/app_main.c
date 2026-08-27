/**
 * p1-foc-bringup — 2804 짐벌모터 첫 회전 (오픈루프)
 *
 * 목표: 전류 피드백 없이 전기각을 강제로 돌려 로터를 따라오게 한다.
 *       클로즈드루프 FOC 는 이게 된 다음.
 *
 * ── 현재 상태: TIM1 미설정. 아직 모터를 돌리지 않는다 ──────────────────
 * platform/g431-esc1/g431-esc1.ioc 에 TIM1 이 아직 없다.
 * 켜기 전에 필요한 것:
 *
 *   1. L6387 데이터시트에서 전파지연·턴오프 시간 확인 → 데드타임 값 계산
 *      ⚠️ 데드타임 없이 상보 PWM 을 내면 상·하단이 겹쳐 도통(션트-스루)한다.
 *         L6387E 에 인터록이 있지만 그건 입력 보호이지 스위칭 과도구간은 못 막는다.
 *   2. CubeMX 로 TIM1 설정 (핀은 확인 완료 — docs/pinmap.md 3.1):
 *        U: CH1  PA8  / CH1N PC13
 *        V: CH2  PA9  / CH2N PA12
 *        W: CH3  PA10 / CH3N PB15
 *      Center-aligned, ARR=4249 → 20kHz (TIM1 clk 170MHz / (2 x 4250))
 *   3. 전원: 전류 리밋 0.8~1A. 상저항 21Ω, 12V 이므로 정상 최대는 0.57A.
 *      TIM1_BKIN 이 MCU 에 연결돼 있지 않아 하드웨어 과전류 차단이 없다 —
 *      전원공급장치 리밋이 유일한 보호장치다.
 *   4. 회전 순서: (a) 정지 벡터로 로터 고정 확인 → (b) 전기각 천천히 증가
 *      → (c) 주파수 올려 탈조 한계 확인
 * ──────────────────────────────────────────────────────────────────────
 *
 * 모터 파라미터 (2026-08-20 로그에서 실측/정리):
 *   - 2804 짐벌모터, 12N14P → pole_pairs = 7
 *   - 전기각 = 7 x 기계각. 전기 360도 = 기계 51.43도
 *   - 상간 저항 21Ω (U-V, V-W, W-U 모두 동일) → Y결선 상저항 10.5Ω
 *
 * 지금은 시리얼로 준비 상태만 찍는다. LED 는 2Hz 로 토글.
 */
#include "bsp.h"

#define POLE_PAIRS        7u
#define R_PHASE_TO_PHASE  21   /* Ω, 실측 */

void app_main(void)
{
    bsp_uart_puts("\r\n=== p1-foc-bringup ===\r\n");
    bsp_printf("SYSCLK      : %lu Hz\r\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    bsp_printf("pole pairs  : %u  (12N14P)\r\n", POLE_PAIRS);
    bsp_printf("R (phase-phase) : %d ohm\r\n", R_PHASE_TO_PHASE);
    bsp_uart_puts("TIM1        : NOT CONFIGURED - 모터를 돌리지 않는다\r\n");
    bsp_uart_puts("다음: CubeMX 에서 TIM1 상보 PWM + 데드타임 설정\r\n\r\n");

    while (1)
    {
        bsp_led_toggle();
        HAL_Delay(250);
    }
}
