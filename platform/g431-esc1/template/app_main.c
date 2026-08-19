/**
 * @실험 이름@ — @한 줄 설명@
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
