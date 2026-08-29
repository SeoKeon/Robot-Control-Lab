/**
 * 엔코더 무전원 판별 테스트 — PWM 없이 손으로 돌려 드리프트 원인 가르기
 *
 * 모터에 전류가 전혀 흐르지 않는다 (간섭원 없음). 손으로 축을 돌리면
 * 누적 각도를 찍는다. 정확히 2바퀴 = 720.0도가 나와야 한다.
 *
 *   맞게 나옴  -> 기계 고정 정상. 전원 실험의 드리프트는 PWM/자기장 간섭
 *   틀리게 나옴 -> 자석 또는 보드가 기계적으로 미끄러지는 것
 */
#include "bsp.h"
#include <stdlib.h>

#define CPR 4096

static uint16_t enc_prev;
static int32_t  enc_acc;

static void enc_poll(void)
{
    uint16_t r = 0;
    if (bsp_as5600_read_raw(&r) == 0) {
        int32_t d = (int32_t)r - (int32_t)enc_prev;
        if (d >  CPR / 2) { d -= CPR; }
        if (d < -CPR / 2) { d += CPR; }
        enc_acc += d;
        enc_prev = r;
    }
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== encoder hand-turn test (PWM OFF) ===\r\n");
    bsp_uart_puts("축에 기준 표시를 하고, 정확히 2바퀴 돌린 뒤 2바퀴 되돌리세요.\r\n");
    bsp_uart_puts("2바퀴 = +720.0 / 되돌리면 0.0 근처가 나와야 한다.\r\n\r\n");

    uint16_t r0 = 0;
    bsp_as5600_read_raw(&r0);
    enc_prev = r0;
    enc_acc  = 0;

    uint32_t n = 0;
    while (1)
    {
        /* 촘촘히 폴링(간섭 없는 조건에서 랩어라운드 놓치지 않게) */
        for (int i = 0; i < 20; i++) { enc_poll(); HAL_Delay(10); }

        int32_t deg10 = (enc_acc * 3600) / CPR;
        bsp_printf("%5lu  누적 %6ld.%ld deg   raw %4u\r\n",
                   (unsigned long)n++, (long)(deg10 / 10), (long)labs(deg10 % 10),
                   enc_prev);
        bsp_led_toggle();
    }
}
