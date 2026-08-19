# 2026-08-19 ST 도구 지형도와 디버그 스택 — 누가 무엇을 하는가

<!-- 이 노트의 경로·버전은 전부 이 PC 에서 확인한 실제 값 -->

## 왜 찾아봤나

이름이 다 "STM32Cube~"로 시작해서 뭐가 뭔지 구분이 안 됐다.
CubeMX / CubeIDE / CubeCLT / CubeProgrammer / VS Code 확장 — 겹치는 것도 있고
없으면 안 되는 것도 있는데 그 경계를 몰라서, 뭐가 안 되면 어디를 깔아야 하는지 몰랐다.

실제로 이 때문에 08-05 에 두 번 막혔다 (`arm-none-eabi-gdb.exe not found`,
`spawn openocd.exe ENOENT`). 둘 다 "확장은 깔았지만 실물 바이너리가 없었다"가 원인.

## 핵심

### 1. 도구 지형도 — 확장은 껍데기, 바이너리는 따로 있다

가장 중요한 구분이다. **VS Code 확장은 UI 일 뿐이고 실제 프로그램은 별도 설치 패키지에 있다.**

| 이름 | 정체 | 없으면 |
|---|---|---|
| **STM32CubeMX** | 코드 생성기 (GUI). `.ioc` 를 읽어 `Core/`·`Drivers/` 생성 | 초기화 코드를 손으로 써야 함 |
| **STM32CubeIDE** | Eclipse 기반 **올인원 IDE**. 아래 것들을 다 품고 있다 | (우리는 VS Code 를 쓰니 불필요) |
| **STM32CubeCLT** | **CommandLine Tools** — GUI 없는 실물 툴체인 묶음 | 컴파일·디버깅 자체가 불가 |
| **STM32CubeProgrammer** | 플래시 라이터. CLT 안에 포함돼 있다 | 굽기 불가 |
| **VS Code 확장군** | 위 도구들을 VS Code 에서 호출하는 껍데기 | 손으로 명령을 쳐야 함 (가능하긴 함) |

이 PC 의 CubeCLT(`C:\ST\STM32CubeCLT_1.22.0\`) 안에 실제로 들어있는 것:

```
GNU-tools-for-STM32\     arm-none-eabi-gcc / gdb  ← 컴파일러와 디버거
CMake\  Ninja\  Make\    빌드 도구
STLink-gdb-server\       ST-LINK GDB 서버        ← 디버깅의 중간 다리
STM32CubeProgrammer\     STM32_Programmer_CLI    ← 플래싱
st-arm-clang\            ST 의 clang 툴체인 (대안)
STMicroelectronics_CMSIS_SVD\  레지스터 정의 파일 (아래 6번)
```

> **08-05 문제 2 의 정답**: 확장만 깔고 CLT 를 안 깔았으니 `arm-none-eabi-gdb.exe` 가
> 있을 곳이 없었다. CLT 설치 후 **VS Code 재시작**이 필요한 이유는 PATH 갱신 때문.

### 2. 이 PC 에는 툴체인이 두 벌 있다 (지금 실제 상태)

| | 무엇 | 어디서 되나 |
|---|---|---|
| **A. CubeCLT** | 시스템 PATH 의 `cmake`, `ninja`, `arm-none-eabi-gcc` | 아무 터미널에서나 |
| **B. VS Code 확장** | `cube-cmake` (CubeIDE 번들 툴체인 래퍼) | **VS Code 안에서만** |

`.vscode/settings.json` 의 `cmake.cmakePath: "cube-cmake"` 때문에 VS Code 빌드는 B 를 쓴다.
`cube-cmake` 는 시스템 PATH 에 없고, 확장이 자기 환경에만 `cube` 런처를 주입한다.
밖에서 실행하면 `'cube' command is not available in current context` 로 거부한다.

**교훈: 같은 층에 구현이 둘 있을 수 있다.** 08-05 의 층별 진단 틀에 이 항목을 추가해야 한다.
어느 쪽을 쓰는지가 설정 한 줄에 숨어 있어서, 증상만 보면 "어제 되던 게 안 되는" 것처럼 보인다.

### 3. 디버그 스택 — 5층, 각 층이 별개 프로그램

```
[에디터]      VS Code + cortex-debug 확장 (1.12.1)
                 ↓ 확장이 gdb 를 띄우고 명령을 보낸다
[디버거]      arm-none-eabi-gdb   (CLT / GNU Tools 14.3.rel1)
                 ↓ TCP 로 "이 주소 읽어줘" 같은 원격 프로토콜(RSP)
[GDB 서버]    ST-LINK_gdbserver   (CLT)
                 ↓ USB
[프로브]      온보드 ST-LINK/V2-1  (보드에 붙어 있음, USB 1개로 해결)
                 ↓ SWD 2선 (PA13=SWDIO, PA14=SWCLK)
[타깃]        STM32G431CBU6
```

핵심은 **GDB 는 STM32 를 모른다**는 것. GDB 는 "메모리 0x20000000 읽어" 같은 추상 명령만
보내고, 그걸 SWD 전기 신호로 바꾸는 게 GDB 서버 + 프로브다.
그래서 서버가 교체 가능하다 (ST-LINK GDB server / OpenOCD / J-Link GDB server).

> **08-05 문제 3 의 정답**: `servertype: "openocd"` 로 써놨는데 OpenOCD 는 별개 프로그램이고
> CLT 에 없다. CLT 는 ST 자체 서버(`stlink`)를 쓴다. **층은 맞았고 그 층의 구현이 틀렸던 것.**

`.ioc` 에서 PA13/PA14 가 `Serial_Wire` 로 잡혀 있는 것을 확인했다. 이 두 핀을
다른 용도로 쓰면 **디버깅 자체가 끊긴다** (그래서 CubeMX가 기본으로 잡아둔다).

### 4. tasks.json vs launch.json — 역할이 다르다

| | tasks.json | launch.json |
|---|---|---|
| 하는 일 | **명령 실행** (빌드, 플래싱, 스크립트) | **프로세스 띄우기** (+디버거 붙이기) |
| 단축키 | `Ctrl+Shift+B` | `F5` |
| 결과 | 종료 코드 | 실행 중인 디버그 세션 |
| 담당 확장 | VS Code 기본 / CMake Tools | cortex-debug |

연결은 `preLaunchTask` 한 줄이다.

```
F5 → launch.json 의 preLaunchTask: "build"
   → tasks.json 의 label: "build" 태스크 실행
   → 성공(종료코드 0)하면 디버거 시작
```

**VS Code + CMake 는 IDE 와 달리 F5 가 자동 빌드를 하지 않는다.**
이게 08-05 문제 4(디버거는 붙었는데 LED가 안 깜빡임 = 이전 바이너리)의 원인이었고,
`preLaunchTask` 가 그 해결책이다. 오늘 실제로 연결했다.

### 5. launch.json 의 필드가 각각 어느 층인가

지금 쓰는 설정을 층별로 갈라 보면 뭘 고쳐야 할지 바로 나온다.

| 필드 | 층 | 뜻 |
|---|---|---|
| `type: "cortex-debug"` | 에디터 | 이 확장이 세션을 관리 |
| `executable` | 디버거 | gdb 가 읽을 `.elf` (심볼·디버그 정보) |
| `armToolchainPath` | 디버거 | `arm-none-eabi-gdb` 가 있는 폴더 |
| `servertype: "stlink"` | GDB 서버 | 어느 서버 구현을 쓸지 |
| `serverpath` | GDB 서버 | 그 서버 실행파일 경로 |
| `interface: "swd"` | 프로브↔타깃 | 전송 방식 (SWD / JTAG) |
| `device: "STM32G431CB"` | 타깃 | 칩 종류 (플래시 크기·알고리즘 판단용) |
| `runToEntryPoint: "main"` | 디버거 | 리셋 후 `main` 까지 자동 진행 |

`executable` 이 `.elf` 인 이유: `.hex`/`.bin` 에는 심볼과 줄 번호가 없어서
소스 레벨 디버깅이 불가능하다. **굽는 건 hex/bin, 디버깅은 elf.**

### 6. 알아두면 좋은 것 — SVD 로 레지스터 보기

CLT 에 레지스터 정의 파일이 들어있다:

```
C:\ST\STM32CubeCLT_1.22.0\STMicroelectronics_CMSIS_SVD\STM32G431.svd
```

`launch.json` 에 `"svdFile"` 로 지정하면 cortex-debug 가 **주변장치 레지스터를
이름으로** 보여준다 (`TIM1->CCR1`, `ADC1->DR` 등을 비트 단위로).
FOC 로 가면 타이머·ADC 레지스터를 직접 확인할 일이 많아지므로 그때 붙이면 좋다.

### 7. 디버거 없이 굽기만 할 때

```powershell
STM32_Programmer_CLI -c port=SWD -w build\Debug\blink_test.elf -rst
```

오늘 `tasks.json` 의 `flash` 태스크로 등록했다 (`build` 에 `dependsOn`).
`-rst` 는 쓰고 나서 리셋 = 바로 실행.

## 실습 연결

- 증상이 나오면 **먼저 층을 특정**한다. 08-05 의 표가 그대로 쓰인다:
  `gdb not found`→툴체인 / `spawn ... ENOENT`→GDB 서버 / 연결만 실패→프로브↔타깃 /
  붙었는데 동작 안 함→바이너리(빌드 누락)
- 여기에 오늘 배운 것을 추가: **같은 층에 구현이 둘일 수 있다** (cmake vs cube-cmake)
- UART 를 켤 때 (다음 단계): 온보드 ST-LINK 가 VCP 를 제공하므로 USB 케이블 하나로
  디버깅과 시리얼이 동시에 된다. 단 **어느 페리페럴에 물려 있는지는 UM2516 확인 필요** —
  감으로 켜면 아무것도 안 나온다
- p1 을 만들 때 `launch.json` 은 커밋 대상이 아니므로 직접 복사해야 한다.
  `platform/g431-esc1/template/README.md` 에 절차를 적어뒀다

## 출처

- 이 PC 에서 직접 확인: `C:\ST\STM32CubeCLT_1.22.0\` 구성,
  `arm-none-eabi-gdb --version` (GNU Tools 14.3.rel1 / gdb 15.2), cortex-debug 1.12.1,
  `cube-cmake` 의 PATH 거부 메시지, `platform/g431-esc1/g431-esc1.ioc` 의 PA13/PA14
- 관련 기록: [2026-08-05 개발환경 세팅](../log/2026-08-05-dev-environment-setup.md)
  (층별 진단 틀의 원본) · [2026-08-19 빌드 경로 정리](../log/2026-08-19-platform-layer-and-build-setup.md)
- 함께 볼 노트: [ARM 크로스 툴체인과 빌드 파이프라인](2026-08-19-arm-toolchain-and-build-pipeline.md)
