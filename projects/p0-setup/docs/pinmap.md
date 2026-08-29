# B-G431B-ESC1 핀맵과 보드 정보

> **보드 핀맵의 정본은 UM2516 Table 4** (+ MB1419 회로도 교차 확인).
> **우리 프로젝트에 실제로 반영된 것의 정본은 `platform/g431-esc1/g431-esc1.ioc`** 다.
> 둘은 아직 다르다 — 3장이 보드 전체, 4장이 현재 `.ioc` 에 들어간 것.
> `.ioc` 를 CubeMX 로 수정하면 4장도 같이 갱신한다.

## 1. MCU

| | |
|---|---|
| 부품번호 | **STM32G431CBU6** (`.ioc` 의 `Mcu.CPN`) |
| 코어 | Cortex-M4F (단정밀도 FPU) |
| FLASH | 128 KB @ `0x08000000` |
| RAM | 32 KB @ `0x20000000` |
| Cube FW | STM32Cube FW_G4 V1.6.3 |

부품번호 해독 (ST 명명 규칙): `G431` 시리즈 / `C` = 48핀 / `B` = 128KB 플래시 /
`U` = UFQFPN 패키지 / `6` = 동작온도 -40~85°C.

메모리 값은 `platform/g431-esc1/STM32G431xx_FLASH.ld` 의 `MEMORY` 블록에서 확인.

## 2. 클럭 (170 MHz)

`platform/g431-esc1/Core/Src/main.c` 의 `SystemClock_Config()` 에서:

```
HSI(내부 16MHz) → PLLM=/4 → 4MHz → PLLN=×85 → 340MHz → PLLR=/2 → SYSCLK 170MHz
AHB=/1, APB1=/1, APB2=/1  (전부 170MHz)
FLASH_LATENCY_4, 전압 스케일링 SCALE1_BOOST
```

외부 크리스탈(HSE)을 쓰지 않고 **내부 발진기(HSI)** 로 최대 클럭을 낸 구성이다.
FOC 로 가면 타이머 주기 계산의 기준이 이 170MHz 다.

## 3. 보드 전체 핀맵 — **확인 완료**

출처: **UM2516 Rev 4, Table 4 "Main board STM32G431CB pinout for motor control"**
(넷 이름은 MB1419 회로도 4페이지 MCU 시트에서 교차 확인)

### 3.1 모터 구동 — TIM1 3상 상보 PWM

| 상 | 상단(High) | 핀 | 하단(Low) | 핀 |
|---|---|---|---|---|
| **U** | `TIM1_CH1` | **PA8** | `TIM1_CH1N` | **PC13** |
| **V** | `TIM1_CH2` | **PA9** | `TIM1_CH2N` | **PA12** |
| **W** | `TIM1_CH3` | **PA10** | `TIM1_CH3N` | **PB15** |

회로도 넷 이름이 `UH`/`UL`/`VH`/`VL`/`WH`/`WL` 이고, `PC13-UL` 은 넷 이름에 직접
박혀 있어 CH1 = U상이 확정된다.

> ⚠️ **PC13 이 CH1N 이라는 게 함정.** PC13 은 보통 RTC/TAMP 로 쓰는 핀이라
> 무심코 다른 용도로 잡기 쉽다. 여기서는 U상 하단 게이트다.

### 3.2 전류 센싱 — MCU 내장 OPAMP 3개 (3션트)

| 션트 | OPAMP+ | OPAMP− | 출력 |
|---|---|---|---|
| 1 | PA1 | PA3 | **PA2** (`OP1_OUT`) |
| 2 | PA7 | PA5 | **PA6** (`OP2_OUT`) |
| 3 | PB0 | PB2 | **PB1** (`OP3_OUT`, TP3) |

STM32G431 **내장 OPAMP** 를 쓴다 (외부 앰프가 아니다). CubeMX 에서 OPAMP1/2/3 을
켜면 출력이 ADC 로 내부 연결된다. ADC 채널 번호는 CubeMX 화면에서 확인할 것.

### 3.3 그 외

| 신호 | 핀 | 비고 |
|---|---|---|
| **USART2_TX / RX** | **PB3 / PB4** | ⭐ **VCP.** LPUART1 추정은 틀렸다 |
| STATUS LED | PC6 | 우리가 `LD2` 로 쓰는 그 핀 |
| SWDIO / SWCLK | PA13 / PA14 | 건드리면 디버깅 끊김 |
| 엔코더/홀 커넥터 (J8) | PB6 (A+/H1), PB7 (B+/H2), PB8 (Z+/H3) | J8 에 5V·GND 전원도 나옴 (UM2516 §5.4) |
| BEMF 1/2/3 | PA4 / PC4 / PB11 | 센서리스용 |
| GPIO_BEMF | PB5 | BEMF 분압 스위치로 추정 |
| VBUS 전압 감지 | PA0 | |
| 온도 감지 | PB14 | |
| 포텐셔미터 | PB12 | |
| PWM 입력 (ESC 신호) | PA15 | 외부에서 서보 PWM 받는 용도 |
| BUTTON | PC10 | |
| CAN TX/RX/SHDN/TERM | PB9 / PA11 / PC11 / PC14 | |
| **8MHz 크리스탈 (HSE)** | PF0 / PF1 | **보드에 실장돼 있다** (Y2) |
| 미사용 | PB10, PB13, PC15 | N.C. |

### 3.4 이 핀맵에서 바로 나오는 결론

- **게이트 드라이버 EN 핀이 없다.** L6387 은 6개 PWM(HIN/LIN)만으로 제어된다.
  켤 것을 깜빡할 핀이 없는 대신, **소프트웨어로 끄는 수단도 없다** —
  리셋 직후 TIM1 을 안전한 상태(MOE off, 듀티 0)로 두고 시작해야 한다.
- **TIM1_BKIN 이 MCU 에 연결돼 있지 않다.** 하드웨어 과전류 차단 경로가 없다.
  → **전원공급장치의 전류 리밋이 유일한 실질 보호장치다.**
- **HSE 8MHz 크리스탈이 있다.** 지금 `.ioc` 는 HSI(내부 16MHz)를 쓰는데,
  FOC 타이밍 정확도가 필요해지면 HSE 로 바꾸는 선택지가 있다.
- **J8 두 핀이 I2C1 이 된다 — 확인 완료** (CubeMX 칩 DB 의 AF 목록):
  **PB7 = `I2C1_SDA`** (B+/H2), **PB8 = `I2C1_SCL`** (Z+/H3). PB6 은 I2C 기능 없음.
  처음 추정했던 "PB6/PB7" 은 틀렸다. J8 센서 라인에는 +3.3V 10K 풀업이
  보드에 이미 있어(홀 입력 회로) I2C 외부 풀업이 필요 없다.
  PB6 는 TIM4_CH1, PB7 는 TIM4_CH2 이기도 하다 — ABI 방식 엔코더(AS5047 등)로
  바꾸면 TIM4 엔코더 모드를 같은 커넥터로 쓸 수 있다.

## 4. 지금 `.ioc` 에 반영된 것

`g431-esc1.ioc` 는 아직 `board=custom` 이고 아래 3핀만 설정돼 있다.
위 3장의 나머지는 **아직 반영 전**이다.

| 핀 | 신호 | 라벨 |
|---|---|---|
| PC6 | `GPIO_Output` | `LD2` |
| PA13 | `SYS_JTMS-SWDIO` | — |
| PA14 | `SYS_JTCK-SWCLK` | — |

## 5. 아직 없는 것

- **배선 사진** — 실물 촬영이 필요하다. `log/img/` 에 넣고 여기서 상대경로로 링크
- **AS5600 실제 배선** — J8: 5V, GND, SDA(B+/H2→PB7), SCL(Z+/H3→PB8)

## 참고

- 정본: [`platform/g431-esc1/g431-esc1.ioc`](../../../platform/g431-esc1/g431-esc1.ioc)
- 보드 공용 계층 설명: [`platform/g431-esc1/README.md`](../../../platform/g431-esc1/README.md)
- [2026-08-05 개발환경 세팅](../../../log/2026-08-05-dev-environment-setup.md)

### ST 문서 링크

핀 확인은 **회로도(MB1419)가 정본**이다. UM2516 은 커넥터·사용법 중심.

| 문서 | 용도 | 링크 |
|---|---|---|
| **MB1419 회로도** | **MCU 핀 ↔ 게이트드라이버·션트 결선. 핀맵의 정본** | [schematic_pack/mb1419-g431cbu6-c01_schematic.pdf](https://www.st.com/resource/en/schematic_pack/mb1419-g431cbu6-c01_schematic.pdf) |
| UM2516 | 보드 사용자 매뉴얼 — 커넥터, VCP, 점퍼 | [user_manual/um2516-...pdf](https://www.st.com/resource/en/user_manual/um2516-electronic-speed-controller-discovery-kit-for-drones-with-stm32g431cb-stmicroelectronics.pdf) |
| 보드 제품 페이지 | 최신 리비전·CAD 자료 | [b-g431b-esc1.html](https://www.st.com/en/evaluation-tools/b-g431b-esc1.html) |
| DS12589 | STM32G431CB 데이터시트 — 핀 대체기능 표 | [datasheet/stm32g431cb.pdf](https://www.st.com/resource/en/datasheet/stm32g431cb.pdf) |
| RM0440 | STM32G4 레퍼런스 매뉴얼 — TIM1 상보출력·데드타임·ADC 인젝티드 | [reference_manual/rm0440-...pdf](https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) |
| G431 에라타 | 실리콘 버그 | [errata_sheet/dm00502298-...pdf](https://www.st.com/resource/en/errata_sheet/dm00502298-stm32g431xx441xx-device-errata-stmicroelectronics.pdf) |

### 파워단 부품 (UM2516 기준)

| 부품 | 역할 | 링크 |
|---|---|---|
| **L6387** | 하프브리지 게이트 드라이버 ×3. **데드타임 값의 근거가 여기 있다** (전파지연·상승/하강 시간) | [datasheet/l6387e.pdf](https://www.st.com/resource/en/datasheet/l6387e.pdf) |
| **STL180N6F7** | 파워 MOSFET ×6 | [datasheet/stl180n6f7.pdf](https://www.st.com/resource/en/datasheet/stl180n6f7.pdf) |

> L6387E 는 상·하단 입력이 동시에 High 가 되지 않도록 하는 **인터록**이 내장돼 있다.
> 논리 실수에 대한 안전망은 되지만, MOSFET 이 꺼지는 데 걸리는 시간 때문에
> **데드타임은 여전히 필요하다.** 인터록은 입력단 보호이지 스위칭 과도구간 보호가 아니다.
