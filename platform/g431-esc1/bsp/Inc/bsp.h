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
