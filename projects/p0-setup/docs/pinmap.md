# B-G431B-ESC1 핀맵과 보드 정보

> **정본은 `platform/g431-esc1/g431-esc1.ioc`** 다. 이 문서는 그것을 읽기 쉽게 옮긴 것.
> `.ioc` 를 CubeMX 로 수정하면 이 문서도 같이 갱신한다.
>
> 아래 **1~3 은 `.ioc` / `main.c` / 링커스크립트에서 직접 확인한 값**이고,
> **4 는 아직 확인하지 못한 것**이다. 추측으로 채우지 않았다.

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

## 3. 현재 `.ioc` 에 설정된 핀 (전부)

`.ioc` 에 잡혀 있는 핀은 **3개뿐**이다. p0 단계는 LED 블링크까지라 그 이상 켜지 않았다.

| 핀 | 신호 | 라벨 | 용도 |
|---|---|---|---|
| **PC6** | `GPIO_Output` | `LD2` | 온보드 **빨간** LED. 코드로 제어하는 유일한 출력 |
| **PA13** | `SYS_JTMS-SWDIO` | — | SWD 데이터 |
| **PA14** | `SYS_JTCK-SWCLK` | — | SWD 클럭 |

활성 주변장치도 `NVIC`, `RCC`, `SYS` 3개뿐 (`Mcu.IP0~2`).

**PA13/PA14 는 건드리면 안 된다.** 다른 기능으로 재할당하면 디버깅 연결이 끊긴다.

BSP 래퍼는 `platform/g431-esc1/bsp/` 에 있다.

```c
#define BSP_LED_PORT  LD2_GPIO_Port   /* = GPIOC */
#define BSP_LED_PIN   LD2_Pin         /* = GPIO_PIN_6 */
void bsp_led_on/off/toggle(void);
```

> **헷갈리지 말 것**: ST-LINK 쪽의 초록/빨강 LED 는 디버거 통신 상태 표시등이고
> 사용자 코드와 무관하다. 코드로 제어하는 건 `LD2` 다. (08-05 로그 참고)

## 4. 아직 확인하지 못한 것 — **UM2516 을 봐야 한다**

B-G431B-ESC1 보드에는 아래가 실제로 달려 있지만, **어느 핀/페리페럴에 물려 있는지
확인된 정보가 없어서 비워뒀다.** 감으로 켜면 아무것도 동작하지 않고 원인 찾는 데
오래 걸린다 (08-05 로그에 적어둔 교훈).

| 확인할 것 | 왜 필요한가 | 확인 방법 |
|---|---|---|
| **VCP UART** | 시리얼 로깅. LPUART1 로 추정되나 미확인 | UM2516 에서 ST-LINK VCP 결선 확인 |
| **3션트 전류 센싱** | FOC 전류 루프의 입력. ADC 채널 + 옵앰프 게인 | UM2516 회로도 + ADC 채널 표 |
| **3상 PWM 출력** | TIM1 채널 + 상보 출력(CHxN) + 데드타임 | UM2516 + CubeMX 핀 할당 |
| **엔코더 (AS5600)** | 모터 내장. I2C 인터페이스 | 모터 데이터시트 + 보드 커넥터 핀아웃 |
| **BEMF / 온도 / VBUS** | 보호와 관측 | UM2516 |
| **모터·전원 커넥터 핀아웃** | 배선 사진과 함께 정리 | 실물 확인 |

확인한 것만 위 3번 표에 추가하고, **`.ioc` 를 정본으로 먼저 수정한 뒤 이 문서를 갱신**한다.

## 5. 아직 없는 것

- **배선 사진** — 실물 촬영이 필요하다. `log/img/` 에 넣고 여기서 상대경로로 링크
- **회로도 발췌** — UM2516 에서 전류 센싱·게이트 드라이버 부분만 잘라 정리하면
  FOC 디버깅할 때 계속 들여다볼 자료가 된다

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
