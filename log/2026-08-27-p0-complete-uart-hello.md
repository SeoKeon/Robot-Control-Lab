# 2026-08-27 p0 완료 — 시리얼 "hello" 뚫기

## 뭘 하려고 했나

p0-setup 의 완료 기준은 "Blink + 시리얼 hello" 였다. Blink 는 08-05 에 끝냈으니
남은 절반인 UART 를 뚫는 것.

그런데 진짜 목적은 다음 단계였다. **모터 코드는 로그 없이 디버깅이 거의 불가능하다.**
전기각·듀티·전류를 눈으로 못 보면 "안 도는데 왜인지 모름" 상태가 된다.
모터로 넘어가기 전에 관측 수단을 먼저 확보하려고 순서를 이렇게 잡았다.

## 뭘 했나

### 1. 핀맵 확정 — 문서를 읽지 않고 읽는 방법

가장 막혔던 건 코드가 아니라 **어느 핀이 뭔지 모르는 것**이었다. UM2516(29쪽)과
회로도(벡터 도면)를 열어봤는데 처음 봐서는 어디를 봐야 할지도 몰랐다.

해결: **PDF 에서 텍스트만 뽑아냈다.** Git Bash 에 `pdftotext` 가 들어있다.

```bash
pdftotext -layout um2516.pdf um.txt
grep -nE "TIM1|OPAMP|USART|PA[0-9]" um.txt
```

UM2516 **Table 4 "Main board STM32G431CB pinout for motor control"** 에
48핀 전부가 표로 정리돼 있었다. 회로도 벡터 도면을 눈으로 따라갈 필요가 없었다.

> 표가 한 번 어긋나게 뽑혀서(`-layout` 모드에서 셀이 밀림) PA3/PA4 가 뒤바뀌어
> 보였다. `-raw` 로 다시 뽑아 교차 확인했다. **핀 하나 틀리면 MOSFET 이 가는
> 상황이라 두 번 뽑아 비교하는 게 맞다.**

회로도는 넷 이름 확인에 썼다. `UH`/`UL`/`VH`/`VL`/`WH`/`WL` 이 있고 그중
`PC13-UL` 은 넷 이름에 상 이름이 직접 박혀 있어서 TIM1_CH1 = U상이 확정됐다.

### 2. CubeMX 로 USART2 활성화

`platform/g431-esc1/g431-esc1.ioc` 하나만 건드렸다 (실험 폴더에서 CubeMX 를
돌리지 않는다는 규칙).

**함정**: CubeMX 는 USART2 를 기본으로 `PA2`/`PA3` 에 배치한다. 이 보드는
`PB3`/`PB4` 다. 그대로 뒀으면 아무것도 안 나왔을 것이다.
게다가 PA2/PA3 는 나중에 전류 센싱 OPAMP1 이 쓸 핀이라 비워둬야 한다.

### 3. BSP 에 송신 래퍼

`platform/g431-esc1/bsp/` 에 넣었다 — 다음 실험에서 그대로 재사용된다.

```c
void bsp_uart_write(const uint8_t *data, uint16_t len);
void bsp_uart_puts(const char *s);
void bsp_printf(const char *fmt, ...);
```

`huart2` 는 CubeMX 가 `main.c` 에 만든 핸들을 `extern` 으로 가져다 쓴다.

### 4. 결과

```
=== uart_hello (B-G431B-ESC1) ===
SYSCLK : 170000000 Hz
USART2 : PB3(TX) / PB4(RX) @ 115200 8N1
hello 0
hello 1
```

플래싱에 쓰는 USB 케이블 그대로 시리얼이 나온다 (COM3, ST-LINK VCP).

## 막힌 것과 해결 과정

### "빌드가 되는데 왜 보드 정보가 없다는 거지?"

이게 이번에 가장 크게 배운 것이다. 빌드가 되니까 보드 정보도 들어있는 줄 알았다.
아니었다. `.ioc` 안에 **`board=custom`** 이라고 적혀 있었다 — 이 프로젝트는
보드가 아니라 **칩을 골라서** 만들어진 것이다.

| | 레포에 있음 | 레포에 없음 |
|---|---|---|
| 뭐냐 | STM32G431CB **칩** 정보 | B-G431B-ESC1 **기판** 정보 |
| 예 | Cortex-M4F, FLASH 128K, TIM1 레지스터 주소, HAL | PA8 이 게이트드라이버 어디에 붙었는지 |

**빌드에 필요한 건 전부 칩 정보다.** 컴파일러는 "Cortex-M4F 명령어로 만들어라",
링커는 "FLASH 128K @0x08000000 에 배치해라"만 알면 된다.
`PA8` 에 무엇이 붙어 있는지는 빌드에 전혀 필요 없다.

HAL 은 *"TIM1 으로 PWM 내는 법"* 은 알지만 *"이 보드에서 U상이 몇 번 채널인지"* 는
모른다. 그건 납땜으로 정해진 것이라 칩 입장에선 알 수가 없다.

`LD2 = PC6` 은 그 한 핀만 누군가 손으로 CubeMX 에 넣은 것이었다.
보드 지식은 한 핀씩 수동으로 들어온다.

### 추정이 틀렸던 것 — VCP 는 LPUART1 이 아니다

08-05 로그에 "LPUART1로 추정"이라고 써뒀는데 **USART2 (PB3/PB4)** 였다.
UM2516 Table 4 에 명확히 있었다.

UM2516 에는 "VCP" 라는 단어가 아예 없어서 한때 *외부 USB-시리얼 어댑터가
필요한가* 싶었다. 회로도 도터보드 시트에서 `USART2_TX_ST_LINK` 넷을 찾아
ST-LINK 로 직결되는 것을 확인했다. SWDIO/SWCLK 과 같은 경로다.

### 한글이 깨졌다 → 펌웨어 문제가 아니었다

배너의 한글만 `???` 로 나왔다. ASCII 줄은 완벽했다.
→ **터미널 인코딩 문제.** UTF-8 로 읽으니 정상. 펌웨어는 유효한 UTF-8 을 보내고 있었다.

증상이 "글자가 깨진다" 라고 같아 보여도, **ASCII 는 되고 한글만 깨지면
보드레이트가 아니라 인코딩**이다. 보드레이트가 틀리면 ASCII 부터 깨진다.

## 배운 것 / 다음 할 것

### platform 계층이 값을 증명한 순간

CubeMX 가 `Drivers/` 에 `stm32g4xx_hal_uart.c` 와 `hal_uart_ex.c` 를 새로 만들었다.
빌드하니:

```
[0/2] Re-checking globbed directories...
-- GLOB mismatch!
The following files were added:
  +.../stm32g4xx_hal_uart.c
  +.../stm32g4xx_hal_uart_ex.c
[1/2] Re-running CMake...
```

**CMake 를 한 줄도 안 고쳤다.** 22 → 26 타깃. `platform.cmake` 의
`file(GLOB ... CONFIGURE_DEPENDS)` 가 알아서 잡았다.
08-19 에 계층을 분리한 값이 여기서 나왔다.

`blink_test` 도 5864 → 11388 바이트로 커졌다. `.ioc` 를 공유하니
**모든 실험이 USART2 초기화를 함께 받는다** — 의도한 트레이드오프다.
128KB 중 11% 이니 문제없다.

### 첫 줄에 SYSCLK 을 찍은 이유

보드레이트는 페리페럴 클럭을 나눠서 만든다. 클럭이 틀리면 보드레이트도 틀려서
글자가 깨진다. 즉 **`SYSCLK : 170000000 Hz` 가 안 깨지고 읽히면
UART 와 클럭 설정이 한 번에 검증**된다. 자기검증이 되는 첫 메시지를 고른 것.

### 모르는 문서를 만났을 때

이번 진짜 교훈. 29쪽 매뉴얼과 벡터 회로도를 눈으로 훑는 건 비효율이었다.

1. **텍스트로 뽑아서 grep** — 표는 대개 깔끔하게 나온다
2. **찾을 키워드를 먼저 정한다** — `TIM1`, `USART`, `OPAMP`
3. **두 출처로 교차 확인** — 표(UM) + 넷 이름(회로도)
4. 안전에 걸리는 값은 **추출 모드를 바꿔 두 번 뽑아 비교**

### 다음 — p1 모터

핀맵이 확정됐으니 바로 들어갈 수 있다.

- CubeMX 에서 TIM1 상보 PWM 6채널 (`PA8`/`PC13`, `PA9`/`PA12`, `PA10`/`PB15`)
- **데드타임 설정** — L6387 데이터시트의 전파지연 스펙에서 값을 계산해야 한다.
  UM2516 과 회로도에는 데드타임 언급이 없다
- 첫 회전은 오픈루프: 정지 벡터 → 벡터 천천히 회전 → 주파수 올려 탈조 확인
- 전원 전류 리밋 0.8~1A (상저항 21Ω, 12V → 최대 0.57A)

주의할 것 두 개가 핀맵에서 나왔다:

- **게이트 드라이버 EN 핀이 없다.** 6개 PWM 만으로 제어된다 → 소프트웨어로 출력을
  끊는 수단이 없으니 리셋 직후 MOE off / 듀티 0 으로 시작해야 한다
- **TIM1_BKIN 이 MCU 에 연결돼 있지 않다.** 하드웨어 과전류 차단 경로가 없다
  → 전원공급장치 전류 리밋이 유일한 실질 보호장치다

---
관련 코드: [projects/p0-setup/src/uart_hello/](../projects/p0-setup/src/uart_hello/) · [platform/g431-esc1/bsp/](../platform/g431-esc1/bsp/) · [docs/pinmap.md](../projects/p0-setup/docs/pinmap.md)
