# B-G431B-ESC1 개발환경 세팅 기록

- 대상 보드: B-G431B-ESC1 (STM32G431CB)
- 환경: Windows + VS Code + STM32Cube 확장 + CubeMX
- 목표: 빌드 → 플래싱 → 디버깅 → LED 블링크 확인

---

## 0. 최종 구성

| 층 | 사용한 것 |
|---|---|
| 코드 생성 | STM32CubeMX |
| 에디터 | VS Code + STM32Cube 확장군 |
| 툴체인 (GCC/GDB) | **STM32CubeCLT** |
| 디버그 서버 | ST-LINK GDB Server (CLT 포함) |
| 디버그 어댑터 | 온보드 ST-LINK/V2-1 (USB 1개로 해결) |
| 빌드 시스템 | CMake |

---

## 1. 겪은 문제와 해결

### 문제 1 — `cmakeDebugType`이 "스크립트"로 설정된... `scriptPath`를 정의해야 합니다

**원인**: `launch.json`에 유효한 구성이 없어서, CMake Tools 확장이 자기 기본값(CMake 스크립트 디버거)을 들이민 것. 펌웨어 디버깅과 무관.

**해결**: `launch.json`을 직접 작성. Run 패널(Ctrl+Shift+D) 드롭다운에서 CMake 항목이 아니라 내가 만든 구성을 선택해야 함.

---

### 문제 2 — `GDB executable "arm-none-eabi-gdb.exe" was not found`

**원인**: VS Code 확장은 껍데기일 뿐. 실제 바이너리(GCC, GDB, GDB서버)는 **STM32CubeCLT**라는 별도 설치 패키지에 들어 있음. 확장만 깔고 CLT를 안 깐 상태였음.

**해결**: STM32CubeCLT 설치 → **VS Code 재시작** (PATH 갱신 필요).

> 회사 PC의 NXP MCUXpresso 안에도 `arm-none-eabi-gdb.exe`가 있지만 쓰지 않음.
> - ST-LINK GDB Server / CubeProgrammer는 여전히 없어서 반쪽짜리
> - 회사 환경과 개인 프로젝트 경로가 얽히면 서로 깨질 수 있음
> - **회사 = NXP, 개인 = ST 로 분리 유지**

---

### 문제 3 — `Failed to launch OpenOCD GDB Server: spawn openocd.exe ENOENT`

**원인**: `launch.json`에 `"servertype": "openocd"`로 써놨는데, CLT는 ST 자체 GDB 서버를 주력으로 하고 OpenOCD가 PATH에 없었음.

**해결**: `servertype`을 `stlink`로 변경 + `configFiles`(OpenOCD 전용) 제거 + CLT 경로 명시.

---

### 문제 4 — 디버거는 붙었는데 LED가 안 깜빡임

**원인**: **빌드를 안 하고 F5를 누름.** 이전 바이너리가 그대로 돌고 있었음.

**해결**: 빌드 후 다시 플래싱. → 빨간 LED 점멸 확인.

> **VS Code + CMake 조합은 IDE와 달리 F5가 자동 빌드를 안 함.**
> `launch.json`에 `preLaunchTask`를 걸어두면 해결됨.

---

### 헷갈렸던 것 — ST-LINK의 초록/빨강 LED

디버거 통신 상태 표시등. **사용자 코드와 무관하며 원래 그렇게 깜빡임.**
코드로 제어하는 건 `LD2`(빨간 LED). 이 둘을 혼동하지 말 것.

---

## 2. 최종 launch.json

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "STM32 Debug",
      "type": "cortex-debug",
      "request": "launch",
      "servertype": "stlink",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/build/Debug/blink_test.elf",
      "device": "STM32G431CB",
      "interface": "swd",
      "runToEntryPoint": "main",
      "preLaunchTask": "build",
      "serverpath": "C:\\ST\\STM32CubeCLT_x.x.x\\STLink-gdb-server\\bin\\ST-LINK_gdbserver.exe",
      "armToolchainPath": "C:\\ST\\STM32CubeCLT_x.x.x\\GNU-tools-for-STM32\\bin",
      "stm32cubeprogrammer": "C:\\ST\\STM32CubeCLT_x.x.x\\STM32CubeProgrammer\\bin"
    }
  ]
}
```

> `x.x.x`는 실제 설치된 CLT 버전 폴더명으로 교체할 것.
> `preLaunchTask`의 값은 `tasks.json`의 `label`과 일치해야 함.

---

## 3. 디버거 없이 굽기만 할 때

```
C:\ST\STM32CubeCLT_x.x.x\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe -c port=SWD -w build\Debug\blink_test.elf -rst
```

`tasks.json`에 태스크로 등록해두면 Ctrl+Shift+B로 바로 실행 가능.

---

## 4. 블링크 코드

```c
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(500);
    /* USER CODE END WHILE */
```

**반드시 `USER CODE BEGIN/END` 사이에만 작성.**
밖에 쓰면 `.ioc` 수정 후 코드 재생성 시 날아감.

---

## 5. VS Code 실행 구조 (개념 정리)

| | tasks.json | launch.json |
|---|---|---|
| 하는 일 | 명령 실행 (빌드, bat, 스크립트) | 프로세스 띄우기 (+디버거) |
| 단축키 | Ctrl+Shift+B | F5 / Ctrl+F5 |
| 결과 | 종료 코드 | 실행 중인 세션 |
| 연결 | ← `preLaunchTask`로 호출됨 | |

**연결 방식**
```
F5 → launch.json의 preLaunchTask
   → tasks.json의 동일 label 태스크 실행
   → 성공 시 디버거 시작
```

### key를 외울 필요 없음
- `.vscode/*.json` 안에서 **Ctrl+Space** → 가능한 key 목록 + 설명 표시
- `launch.json` 우측 하단 **"구성 추가"** 버튼 → 확장이 제공하는 템플릿 삽입
- `Ctrl+Shift+P` → `Tasks: Run Task` → 자동 감지된 태스크 목록 (톱니바퀴로 tasks.json에 고정 가능)

### 알아두면 유용한 필드
| 필드 | 용도 |
|---|---|
| `preLaunchTask` | 디버깅 전 태스크 실행 |
| `problemMatcher: "$gcc"` | 컴파일 에러를 Problems 패널에 클릭 가능한 링크로 |
| `dependsOn` / `dependsOrder` | 태스크 실행 순서 지정 |
| `${workspaceFolder}` | 경로 하드코딩 방지 |

### 설정 우선순위
```
기본값 < 사용자 설정 < 워크스페이스 설정 < 폴더 설정(.vscode)
```
"내 PC에선 되는데 동료 PC에선 안 되는" 문제는 대부분 여기서 발생.
개인 설정에만 있고 `.vscode/`에 공유 안 된 경우.

---

## 6. 문제 발생 시 층별 진단

세부 절차보다 이 구조를 기억할 것. **벤더가 바뀌어도 동일하게 적용됨.**

```
[에디터/IDE]        VS Code, CubeIDE, MCUXpresso
      ↓
[디버거]            GDB
      ↓
[GDB 서버]          ST-LINK GDB Server, OpenOCD, P&E
      ↓
[디버그 프로브]      ST-LINK, J-Link  (하드웨어)
      ↓
[타겟 MCU]          STM32G431CB
```

오늘 겪은 것을 이 틀로 보면:

| 증상 | 막힌 층 |
|---|---|
| gdb.exe not found | 툴체인 (GDB) |
| spawn openocd ENOENT | GDB 서버 |
| 장치 검색은 되는데 연결 실패 | 프로브 ↔ 타겟 |
| 붙었는데 동작 안 함 | 바이너리 (빌드 누락) |

---

## 7. 다음 단계 (P1 나머지: UART)

1. **UM2516(보드 사용자 매뉴얼)에서 VCP가 어느 페리페럴에 물려 있는지 확인**
   - LPUART1로 추정되나 **반드시 문서 확인 필요**
   - 감으로 USART2 등을 켜면 아무것도 안 나오고 원인 찾는 데 오래 걸림
2. CubeMX에서 해당 UART 활성화, 115200 8N1, 코드 재생성
3. `HAL_UART_Transmit()`으로 문자열 송신
4. 시리얼 터미널로 수신 확인 (온보드 ST-LINK가 VCP 제공)

UART가 뚫리면 이후 AS5600 각도값, ADC 전류값 로깅이 전부 이 경로로 가능.

---

## 참고 — 현재 확인된 빌드 결과

```
ELF: ARM 32-bit, EABI5, with debug_info, not stripped
Entry point : 0x08000d5d   (Thumb 모드라 홀수)
Flash       : 0x08000000, 3,640 bytes  (128KB 중)
RAM         : 0x20000000
```
