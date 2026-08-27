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
