/**
 * bsp.h — B-G431B-ESC1 보드 공용 계층
 *
 * 실험마다 반복되는 보드 의존 코드를 여기에 모은다.
 * 실험 코드는 이 헤더만 include하면 된다.
 */
#ifndef BSP_H
#define BSP_H

#include "main.h"   /* CubeMX 생성 핀 정의 + stm32g4xx_hal.h */

#ifdef __cplusplus
extern "C" {
#endif

/* --- 온보드 LED (LD2 = PC6) ---------------------------------------------- */
#define BSP_LED_PORT  LD2_GPIO_Port
#define BSP_LED_PIN   LD2_Pin

void bsp_led_on(void);
void bsp_led_off(void);
void bsp_led_toggle(void);

/* --- 시리얼 (USART2 -> 도터보드 ST-LINK VCP, 115200 8N1) ------------------
 * 핀: PB3=TX, PB4=RX  (UM2516 Table 4)
 * USB 케이블 하나로 디버깅과 시리얼이 동시에 된다.
 */
void bsp_uart_write(const uint8_t *data, uint16_t len);
void bsp_uart_puts(const char *s);

/**
 * printf 형식 송신. 한 번에 최대 127자(넘으면 잘린다).
 * 주의: %f 는 기본으로 동작하지 않는다 — newlib-nano 는 float 포맷을 빼고
 *       링크한다. 필요하면 링커 옵션에 -u _printf_float 를 추가할 것.
 *       당장은 정수로 스케일해서(예: 밀리단위) 찍는 쪽이 싸고 빠르다.
 */
void bsp_printf(const char *fmt, ...);

/* --- 3상 PWM (TIM1) ------------------------------------------------------
 * U: CH1  PA8  / CH1N PC13      (UM2516 Table 4)
 * V: CH2  PA9  / CH2N PA12
 * W: CH3  PA10 / CH3N PB15
 *
 * Center-aligned, ARR=4249 -> 20kHz. 데드타임 DTG=149 -> 1us.
 *
 * ⚠️ TIM1_BKIN 이 MCU 에 연결돼 있지 않다 = 하드웨어 과전류 차단이 없다.
 *    전원공급장치의 전류 리밋이 유일한 실질 보호장치다.
 * ⚠️ 게이트 드라이버 EN 핀이 없다. 출력을 끊는 유일한 수단이 bsp_pwm_stop() 이다.
 */
#define BSP_PWM_ARR       4249u   /* .ioc 의 Period 와 반드시 일치 */

/**
 * 브링업 안전 상한. 듀티는 이 값을 넘지 못하게 클램프된다.
 * 12V / 상간 21Ω 이므로 듀티 100% 여도 모터 전류는 0.57A 지만,
 * 배선이나 설정 실수를 여기서 한 번 더 막는다.
 * 올릴 때는 전원 전류 리밋을 먼저 확인하고 의도적으로 올릴 것.
 */
#define BSP_PWM_DUTY_MIN  0.20f
#define BSP_PWM_DUTY_MAX  0.80f

/** 듀티 0 으로 맞춘 뒤 6채널(상보 포함) 출력을 시작한다. */
void bsp_pwm_start(void);

/** 6채널 출력을 끊는다. MOE=0 -> Idle State(Reset) = 6게이트 전부 off. */
void bsp_pwm_stop(void);

/** 각 상 듀티 0.0~1.0. BSP_PWM_DUTY_MIN/MAX 로 클램프된다. */
void bsp_pwm_set_duty(float du, float dv, float dw);

/**
 * 전기각 theta_e[rad] 방향으로 크기 m(0.0~1.0)의 전압 벡터를 인가한다.
 * 사인 변조(3차 고조파 주입 없음) — 선간 최대 진폭은 m x Vbus x sqrt(3)/2.
 * m=0.30, Vbus=12V 이면 선간 약 3.1V -> 21Ω 에서 약 0.15A.
 */
void bsp_pwm_set_vector(float theta_e, float m);

/* --- AS5600 자기 엔코더 (I2C1: PB7=SDA, PB8=SCL, 100kHz) -------------------
 * J8 커넥터 경유. 보드 홀입력 회로의 +3.3V 10K 풀업을 그대로 쓴다.
 * 주소 0x36 고정. 12비트(0~4095) = 기계각 1회전.
 */
#define BSP_AS5600_ADDR   0x36u
#define BSP_AS5600_CPR    4096u   /* counts per revolution */

/** I2C 버스 스캔 — 응답한 7비트 주소를 시리얼로 출력. 배선 검증용 */
void bsp_i2c_scan(void);

/**
 * 자석 상태 확인. 1=정상(MD, 자석 감지), 0=문제.
 * detail 이 NULL 이 아니면 STATUS 레지스터 원값(0x0B)을 넣어준다:
 *   bit5 MD=자석 감지, bit4 ML=자석 너무 약함/멀다, bit3 MH=너무 강함/가깝다
 */
int bsp_as5600_magnet_ok(uint8_t *detail);

/**
 * 기계각 원시값 읽기 (RAW ANGLE 0x0C/0x0D, 0~4095).
 * 성공 시 0, 실패(I2C 에러) 시 -1.
 */
int bsp_as5600_read_raw(uint16_t *raw);

/** AGC(자동 게인) 읽기. 3.3V 모드에서 0~128, 클수록 자석이 멀다. 이상적 ~64 */
int bsp_as5600_read_agc(uint8_t *agc);

/* --- 앞으로 추가될 자리 ---------------------------------------------------
 * CubeMX(platform/g431-esc1/g431-esc1.ioc)에서 주변장치를 켤 때마다
 * 여기에 래퍼를 추가한다. 실험 코드가 레지스터/핀을 직접 만지지 않게.
 *   예) bsp_pwm_set_duty(), bsp_current_read_abc(), bsp_encoder_read_angle()
 * 핀 배치는 .ioc 를 정본으로 삼고, 확인한 것만 여기에 적는다.
 * ------------------------------------------------------------------------ */

/**
 * 실험별 진입점. 각 실험 프로젝트가 이 함수를 구현한다.
 * main()이 HAL_Init / 클럭 / 주변장치 초기화를 끝낸 뒤 호출하며, 돌아오지 않는다.
 * 구현하지 않으면 bsp.c의 weak 기본 구현(LED 빠른 점멸)이 링크된다.
 */
void app_main(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
