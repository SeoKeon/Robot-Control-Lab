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

/* ===== I2C 버스 진단 — 하드웨어 I2C 를 잠시 끄고 핀을 직접 본다 ===== */

extern I2C_HandleTypeDef hi2c1;

#define P_SDA GPIO_PIN_7
#define P_SCL GPIO_PIN_8

typedef struct { uint16_t sda, scl; } bb_pins_t;

static void bb_dly(void) { for (volatile int i = 0; i < 300; i++) { } }   /* 약 수 us */
static int  pin_rd(uint16_t p) { return HAL_GPIO_ReadPin(GPIOB, p) == GPIO_PIN_SET; }
static void pin_hi(uint16_t p) { HAL_GPIO_WritePin(GPIOB, p, GPIO_PIN_SET);   bb_dly(); }  /* 놓기 */
static void pin_lo(uint16_t p) { HAL_GPIO_WritePin(GPIOB, p, GPIO_PIN_RESET); bb_dly(); }  /* 끌어내림 */

static void pins_mode(uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = P_SDA | P_SCL;
    g.Mode  = mode;
    g.Pull  = pull;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOB, P_SDA | P_SCL, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &g);
    HAL_Delay(2);
}

static void bb_start(bb_pins_t b) { pin_hi(b.sda); pin_hi(b.scl); pin_lo(b.sda); pin_lo(b.scl); }
static void bb_stop(bb_pins_t b)  { pin_lo(b.sda); pin_hi(b.scl); pin_hi(b.sda); }

/* 걸려 있는 슬레이브를 풀어준다: SDA 를 놓고 SCL 9발 + STOP */
static void bb_recover(bb_pins_t b)
{
    pin_hi(b.sda);
    for (int i = 0; i < 9; i++) { pin_lo(b.scl); pin_hi(b.scl); }
    bb_stop(b);
}

static int bb_write(bb_pins_t b, uint8_t v)   /* ACK 받으면 1 */
{
    for (int i = 7; i >= 0; i--) {
        if (v & (1u << i)) { pin_hi(b.sda); } else { pin_lo(b.sda); }
        pin_hi(b.scl);
        pin_lo(b.scl);
    }
    pin_hi(b.sda);
    pin_hi(b.scl);
    int ack = !pin_rd(b.sda);
    pin_lo(b.scl);
    return ack;
}

static int bb_scan(bb_pins_t b, const char *name)
{
    int n = 0;
    bsp_printf("  %s :", name);
    for (uint8_t a = 0x08; a < 0x78; a++) {
        bb_start(b);
        int ack = bb_write(b, (uint8_t)(a << 1));
        bb_stop(b);
        if (ack) { bsp_printf(" 0x%02X", a); n++; }
    }
    bsp_printf("  (%d개)\r\n", n);
    return n;
}

static void i2c_bus_diag(void)
{
    bsp_uart_puts("\r\n--- I2C 버스 진단 ---\r\n");
    bsp_printf("I2C1 BUSY 플래그 = %lu  (1 이면 누가 SDA 를 잡고 있음)\r\n",
               (unsigned long)((hi2c1.Instance->ISR >> 15) & 1u));

    HAL_I2C_DeInit(&hi2c1);

    pins_mode(GPIO_MODE_INPUT, GPIO_NOPULL);
    int sda0 = pin_rd(P_SDA), scl0 = pin_rd(P_SCL);
    bsp_printf("[1] 대기 레벨 (내부 풀 없음)   SDA=%d SCL=%d   <- 기대 1 1\r\n", sda0, scl0);

    pins_mode(GPIO_MODE_INPUT, GPIO_PULLDOWN);
    bsp_printf("[2] 내부 풀다운 걸었을 때      SDA=%d SCL=%d   <- 1 이면 보드 10K 풀업 정상\r\n",
               pin_rd(P_SDA), pin_rd(P_SCL));

    pins_mode(GPIO_MODE_OUTPUT_OD, GPIO_PULLUP);
    bb_pins_t ok  = { P_SDA, P_SCL };
    bb_pins_t swp = { P_SCL, P_SDA };
    bsp_uart_puts("[3] 소프트웨어 I2C 스캔 (버스 복구 후)\r\n");
    bb_recover(ok);
    int n_ok = bb_scan(ok, "정상   SDA=PB7 SCL=PB8");
    bb_recover(swp);
    int n_sw = bb_scan(swp, "뒤바뀜 SDA=PB8 SCL=PB7");

    bsp_uart_puts("판정: ");
    if (!sda0 || !scl0) {
        bsp_uart_puts("선이 Low 로 눌려 있다 -> AS5600 VCC 가 안 들어옴(보호다이오드가 끌어내림) 또는 GND 단락\r\n");
    } else if (n_ok) {
        bsp_uart_puts("AS5600 응답함 -> 배선 OK. 아래 하드웨어 재스캔도 0x36 이면 정상\r\n");
    } else if (n_sw) {
        bsp_uart_puts("SDA 와 SCL 이 뒤바뀌어 연결돼 있다 -> 두 선을 서로 바꿔 꽂을 것\r\n");
    } else {
        bsp_uart_puts("선은 High 인데 아무도 응답 없음 -> SDA/SCL 이 AS5600 까지 안 감(단선) 또는 칩 불량\r\n");
    }

    HAL_I2C_Init(&hi2c1);   /* MspInit 이 PB7/PB8 을 다시 I2C AF 로 되돌린다 */
    bsp_uart_puts("[4] 하드웨어 I2C 재스캔 -> ");
    bsp_i2c_scan();
    bsp_uart_puts("--------------------\r\n");
}

void app_main(void)
{
    bsp_uart_puts("\r\n=== encoder hand-turn test (PWM OFF) ===\r\n");
    i2c_bus_diag();   /* 0x36 이 없으면 아래 값은 전부 0 — 판정 줄을 볼 것 */
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
        uint8_t st = 0, agc = 0;
        bsp_as5600_magnet_ok(&st);
        bsp_as5600_read_agc(&agc);
        bsp_printf("%5lu  누적 %6ld.%ld deg   raw %4u   MD=%d AGC=%3u\r\n",
                   (unsigned long)n++, (long)(deg10 / 10), (long)labs(deg10 % 10),
                   enc_prev, (st >> 5) & 1, agc);
        bsp_led_toggle();
    }
}
