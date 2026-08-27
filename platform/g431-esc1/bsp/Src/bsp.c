#include "bsp.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* CubeMX 가 Core/Src/main.c 에 만든 핸들. MX_USART2_UART_Init() 이
   app_main() 보다 먼저 불리므로 여기서 바로 써도 된다. */
extern UART_HandleTypeDef huart2;

#define BSP_UART          (&huart2)
#define BSP_UART_TIMEOUT  100u   /* ms. 블로킹 송신이라 무한대기는 피한다 */

void bsp_uart_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(BSP_UART, (uint8_t *)data, len, BSP_UART_TIMEOUT);
}

void bsp_uart_puts(const char *s)
{
    bsp_uart_write((const uint8_t *)s, (uint16_t)strlen(s));
}

void bsp_printf(const char *fmt, ...)
{
    char    buf[128];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    if (n <= 0) {
        return;
    }
    /* vsnprintf 는 "잘리지 않았다면 필요했을 길이"를 준다 */
    if (n > (int)sizeof buf - 1) {
        n = (int)sizeof buf - 1;
    }
    bsp_uart_write((const uint8_t *)buf, (uint16_t)n);
}

void bsp_led_on(void)     { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_SET); }
void bsp_led_off(void)    { HAL_GPIO_WritePin(BSP_LED_PORT, BSP_LED_PIN, GPIO_PIN_RESET); }
void bsp_led_toggle(void) { HAL_GPIO_TogglePin(BSP_LED_PORT, BSP_LED_PIN); }

/* ===================== 3상 PWM (TIM1) ===================== */

extern TIM_HandleTypeDef htim1;   /* CubeMX 가 main.c 에 만든 핸들 */

#define BSP_TIM (&htim1)

static float bsp_clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void bsp_pwm_start(void)
{
    /* 켜기 전에 듀티 0. 상보 PWM 이므로 이 상태는 하단 3개만 ON = 3상 단락(브레이크).
       회전자가 정지 상태라면 전류가 흐르지 않는다. */
    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_3, 0);

    HAL_TIM_PWM_Start(BSP_TIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(BSP_TIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(BSP_TIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(BSP_TIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(BSP_TIM, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Start(BSP_TIM, TIM_CHANNEL_3);
}

void bsp_pwm_stop(void)
{
    HAL_TIM_PWM_Stop(BSP_TIM, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Stop(BSP_TIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(BSP_TIM, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Stop(BSP_TIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(BSP_TIM, TIM_CHANNEL_3);
    HAL_TIMEx_PWMN_Stop(BSP_TIM, TIM_CHANNEL_3);

    /* 확실히 끊는다. OCIdleState=Reset 이므로 6핀 모두 Low = MOSFET 전부 off */
    __HAL_TIM_MOE_DISABLE(BSP_TIM);
}

void bsp_pwm_set_duty(float du, float dv, float dw)
{
    du = bsp_clampf(du, BSP_PWM_DUTY_MIN, BSP_PWM_DUTY_MAX);
    dv = bsp_clampf(dv, BSP_PWM_DUTY_MIN, BSP_PWM_DUTY_MAX);
    dw = bsp_clampf(dw, BSP_PWM_DUTY_MIN, BSP_PWM_DUTY_MAX);

    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_1, (uint32_t)(du * (float)BSP_PWM_ARR));
    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_2, (uint32_t)(dv * (float)BSP_PWM_ARR));
    __HAL_TIM_SET_COMPARE(BSP_TIM, TIM_CHANNEL_3, (uint32_t)(dw * (float)BSP_PWM_ARR));
}

void bsp_pwm_set_vector(float theta_e, float m)
{
    static const float k120 = 2.0943951f;   /* 2*pi/3 */

    const float amp = 0.5f * m;
    bsp_pwm_set_duty(0.5f + amp * cosf(theta_e),
                     0.5f + amp * cosf(theta_e - k120),
                     0.5f + amp * cosf(theta_e + k120));
}

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
