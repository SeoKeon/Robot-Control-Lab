#include "bsp.h"

void bsp_led_on(void)     { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_SET); }
void bsp_led_off(void)    { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_RESET); }
void bsp_led_toggle(void) { HAL_GPIO_TogglePin(BSP_LED_PORT, BSP_LED_PIN); }

/**
 * app_main() 을 구현하지 않은 프로젝트를 위한 기본 구현.
 * 링크 에러 대신 LED 100ms 점멸 — "실험 코드가 안 붙었다"를 보드에서 바로 알 수 있게.
 */
__attribute__((weak)) void app_main(void)
{
    while (1)
    {
        bsp_led_toggle();
        HAL_Delay(100);
    }
}
