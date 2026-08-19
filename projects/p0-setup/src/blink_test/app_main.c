/**
 * p0-setup / blink_test — 개발환경 완주 확인
 *
 * 빌드 → 플래싱 → 디버깅이 끝까지 되는지 확인하는 최소 펌웨어.
 * LD2(PC6)를 500ms 주기로 토글한다.
 *
 * 초기화(HAL_Init, 클럭, GPIO)는 platform/g431-esc1 의 CubeMX 생성 main() 이 끝낸 뒤
 * 이 함수를 호출한다.
 */
#include "bsp.h"

void app_main(void)
{
    while (1)
    {
        bsp_led_toggle();
        HAL_Delay(500);
    }
}
