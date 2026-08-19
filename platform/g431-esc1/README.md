# platform/g431-esc1 — 보드 공용 계층

| | |
|---|---|
| 보드 | B-G431B-ESC1 |
| MCU | STM32G431CB (Cortex-M4F, 170MHz) |
| Cube FW | STM32Cube FW_G4 V1.6.3 |
| 툴체인 | STM32CubeCLT (arm-none-eabi-gcc) |

보드는 항상 이거 하나다. 그래서 **실험마다 같은 것들은 전부 여기 한 벌만 존재한다.**

```
g431-esc1.ioc            CubeMX 프로젝트 — 이 파일로만 CubeMX를 돌린다
.mxproject               CubeMX 생성 상태 (커밋 대상)
Core/Inc, Core/Src       CubeMX 생성물. main() 은 초기화 후 app_main() 호출
Drivers/                 HAL + CMSIS (약 4.6MB — 복사본을 늘리지 않는 게 이 폴더의 존재 이유)
startup_stm32g431xx.s    스타트업 코드
STM32G431xx_FLASH.ld     링커 스크립트
cmake/                   툴체인 파일 (gcc / starm-clang)
bsp/                     손으로 쓴 보드 공용 코드 — 핀 래퍼, 센서 접근 등
platform.cmake           프로젝트가 include하는 진입점. add_lab_firmware() 제공
template/                새 실험 시작용 복사본
```

## 실험 코드와의 경계

```
main()  ← platform (CubeMX 생성)
  HAL_Init() → SystemClock_Config() → MX_*_Init()
  app_main()  ← projects/<실험>/src/<타깃>/app_main.c
```

`app_main()` 은 각 실험이 구현한다. 구현하지 않으면 `bsp.c` 의 weak 기본 구현이
링크되어 LED가 100ms로 빠르게 점멸한다 — "실험 코드가 안 붙었다"는 신호.

레지스터·핀을 실험 코드에서 직접 만지지 말고 `bsp/` 에 래퍼를 추가한다.
그게 다음 실험에서 재사용되는 지점이다.

## CubeMX를 다시 돌릴 때

주변장치(ADC/TIM/CORDIC/…)를 새로 켜야 하면 `g431-esc1.ioc` 를 열어 설정하고 생성한다.

- 생성물은 `Core/` 와 `Drivers/` 로만 들어간다. HAL 소스는 `platform.cmake` 가
  glob으로 잡으므로 파일 목록을 손으로 고칠 필요가 없다.
- `Core/Src/main.c` 의 `USER CODE` 블록(`#include "bsp.h"`, `app_main();`)은
  CubeMX가 보존한다. 생성 후 남아 있는지 한 번 확인할 것.
- **모든 실험이 이 변경을 함께 받는다.** 과거 실험의 재현성은 git 커밋/태그로 확보한다.
  ("`p1` 실험 당시 상태" = 그 커밋을 체크아웃)

## 이 계층을 만든 이유

전에는 실험마다 CubeMX 프로젝트를 통째로 새로 만들어서, 보드가 같은데도
`Drivers/`(90여 파일, 4.6MB)가 실험 수만큼 복제됐다. 실험 5개면 23MB, 450파일.
게다가 주변장치를 켤 때마다 사본들이 조금씩 갈라져서 어느 게 최신인지 알 수 없게 된다.
그래서 보드에 종속된 것과 실험에 종속된 것을 갈라놓았다.
