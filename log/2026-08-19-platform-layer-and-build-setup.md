# 2026-08-19 보드 공용 계층 분리와 빌드 경로 정리

## 뭘 하려고 했나

blink_test 하나 만들었을 뿐인데 레포가 벌써 무거웠다. 원인을 보니
`blink_test.ioc` 의 `ProjectManager.LibraryCopy=1` — "필요한 라이브러리만 **복사**" 설정이라
CubeMX가 실험마다 `Drivers/`(HAL + CMSIS, 90여 파일 / 4.6MB)를 통째로 새로 복사한다.

보드는 B-G431B-ESC1 하나로 고정인데 실험마다 사본이 늘어나는 구조다.
실험 5개면 23MB / 450파일. 크기보다 더 나쁜 건 FOC로 넘어가 ADC·TIM·CORDIC 을
켜기 시작하면 사본들이 서로 갈라져서 **어느 게 최신인지 추적이 안 되는 것.**

그래서 보드에 종속된 것과 실험에 종속된 것을 갈라놓기로 했다.

## 뭘 했나

`platform/g431-esc1/` 을 만들어 실험마다 같은 것을 전부 여기 한 벌만 두었다.

```
main()                     ← platform (CubeMX 생성, 손대지 않음)
  HAL_Init()
  SystemClock_Config()
  MX_GPIO_Init()
  app_main()               ← projects/<실험>/src/<타깃>/app_main.c  (여기가 본편)
```

- `Core/`, `Drivers/`, `startup_stm32g431xx.s`, `STM32G431xx_FLASH.ld`,
  툴체인 cmake, `.ioc`(→ `g431-esc1.ioc`, 이제 실험이 아니라 보드에 속한다) 이동
- `platform.cmake` 의 `add_lab_firmware()` 로 빌드. HAL 소스는 glob(`CONFIGURE_DEPENDS`)
  으로 잡으므로 CubeMX에서 주변장치를 켜도 CMake를 손댈 일이 없다
- `bsp/` 신설 — 보드 공용 코드(현재는 LED 래퍼만). `app_main()` 의 weak 기본 구현을 두어
  실험 코드가 안 붙으면 LED가 100ms로 빠르게 점멸한다. 보드만 보고 알 수 있게 한 것
- `template/` 신설 — 새 실험은 복사 + `@TARGET@` 치환. **CubeMX를 새로 돌리지 않는다**
- 실험 폴더에 남는 것은 `app_main.c` + 29줄 `CMakeLists.txt` + 프리셋뿐 (6개 파일)

빌드 경로도 정리했다.

- `.vscode/tasks.json` 신설 — `build` / `flash` 태스크
- `launch.json` 에 `preLaunchTask: "build"` 연결 (08-05 문제 4 의 해결책을 실제로 적용)
- `.gitattributes` 신설 — 인덱스 줄바꿈 LF 고정 + 이미지·바이너리 명시
- `.gitignore` 루트 통합. 프로젝트별 `.gitignore` 의 `!.settings` 예외가
  CubeCLT 캐시를 되살리고 있었다
- 로그 제목 줄을 규칙(`# YYYY-MM-DD 제목`)에 맞춤

## 막힌 것과 해결 과정

### 1. 마이그레이션 스크립트가 파싱조차 안 됨

한글이 든 `.ps1` 을 BOM 없는 UTF-8 로 저장했더니 PowerShell 5.1 이 CP949 로 읽었다.
단순 깨짐으로 끝나지 않고 **문자열 종료 따옴표까지 삼켜서** 파서 에러로 죽는다.

```
Write-Host "?쒓? ?뚯뒪??
The string is missing the terminator: "
```

콘솔 코드페이지는 65001(UTF-8)인데 `[System.Text.Encoding]::Default` 는 여전히
CP949 다. PowerShell 5.1 은 **BOM 없는 스크립트 파일을 이 ANSI 코드페이지로 읽는다.**
→ UTF-8 BOM 을 붙여 해결. 확인법: `head -c 3 file.ps1` 이 `efbbbf` 여야 한다.

반대로 스크립트가 *생성하는* 소스/문서는 BOM 없는 UTF-8 이 맞다. 방향이 반대다.

### 2. `git mv` 가 전부 실패 — blink_test 는 커밋된 적이 없었다

히스토리를 보존하려고 `git mv` 로 옮기려 했는데 실패했다.

```
fatal: source directory is empty, source=projects/p0-setup/src/blink_test/Core
```

레포에 커밋된 파일이 13개뿐이고 `blink_test/` 는 통째로 untracked 였다.
`git mv` 는 **추적 중인 경로만** 옮긴다. 추적 여부를 보고 `git mv` / 일반 이동으로
갈라지게 해서 해결. 히스토리가 없는 파일에는 보존할 rename 도 없으니 결과 트리는 같다.

여기서 더 위험했던 것: 되돌리기 안내가 `git reset --hard HEAD` 였는데,
원래 untracked 였던 파일이 `git add -A` 로 스테이지된 뒤에는 이게 **복구가 아니라 삭제**다.
실행 전에 25MB 전체를 따로 백업해두고 진행했다.

### 3. 툴체인이 두 벌이라 빌드가 헷갈렸다 — 이게 근본 원인이었다

| | 무엇 | 어디서 되나 |
|---|---|---|
| A. STM32CubeCLT | 시스템 PATH 의 `cmake`, `ninja`, `arm-none-eabi-gcc` | 아무 터미널에서나 |
| B. VS Code STM32Cube 확장 | `cube-cmake` (CubeIDE 번들 툴체인 래퍼) | **VS Code 안에서만** |

`.vscode/settings.json` 의 `cmake.cmakePath: "cube-cmake"` 때문에 VS Code 빌드는 B 를 쓴다.
그런데 `cube-cmake` 는 시스템 PATH 에 없다. 확장이 자기 환경에만 주입한다.
밖에서 직접 실행하면 거부한다:

```
'cube' command is not available in current context
```

즉 **터미널 = A, VS Code = B.** 둘 다 동작하지만 서로 다른 컴파일러다.
"어제 되던 게 오늘 안 되는" 느낌의 대부분이 여기서 온다.

그래서 `tasks.json` 의 build 태스크를 셸 명령(`cmake --build`)으로 쓰지 않고
`"type": "cmake"` 로 **CMake Tools 확장에 위임**했다. 상태바 빌드 버튼(F7)과
완전히 같은 cmake 를 쓰므로 `build/Debug` 의 CMakeCache 가 어긋나지 않는다.
셸 태스크로 짰다면 A 와 B 가 같은 빌드 폴더를 번갈아 건드려서 컴파일러 불일치가 났을 것.

### 4. 기록은 있었는데 적용이 안 돼 있었다

08-05 로그 §2 의 최종 `launch.json` 에는 `preLaunchTask: "build"` 가 적혀 있는데
디스크의 `launch.json` 에는 그 줄이 없고 `tasks.json` 자체가 없었다.
**문제 4(빌드 안 하고 F5)의 해결책이 문서에만 있고 실물에는 없던 상태.**
기록과 실물이 갈라지면 기록 쪽을 믿게 되니 더 위험하다.

## 배운 것 / 다음 할 것

**"복사냐 참조냐"는 처음에 정해야 한다.** CubeMX의 `LibraryCopy=1` 은 실험이 하나일 때
아무 문제가 없어 보인다. 사본이 갈라지기 시작하면 이미 늦는다. 지금 옮기는 값이
실험 5개일 때보다 훨씬 싸다.

**같은 도구가 두 벌 깔려 있을 수 있다.** 08-05 에 정리한 층별 진단 틀
(에디터 → 디버거 → GDB 서버 → 프로브 → 타겟) 에 **"같은 층에 구현이 둘일 수 있다"** 를
추가해야 한다. 이번 건은 툴체인 층이 두 벌이었고, 어느 쪽을 쓰는지가 설정 한 줄에 숨어 있었다.

**검증 안 된 계획은 계획이 아니다.** 이동 계획서에는 "65개 파일이 rename 으로 인식됨"이라고
적혀 있었지만 실제 레포에서는 `git mv` 가 한 건도 되지 않았다.
복제 레포에서 리허설한 뒤 실제 레포에 적용하는 순서로 막았다.

### 다음

1. **UART 뚫기** (08-05 §7 그대로) — UM2516 에서 VCP 페리페럴 확인이 먼저.
   `platform/g431-esc1/g431-esc1.ioc` 로만 CubeMX를 돌리고, 래퍼는 `bsp/` 에 추가한다
2. `bsp/` 는 아직 LED만 — 전류 센싱·PWM·엔코더 핀은 `.ioc` 를 정본으로 확인한 것만 채운다
3. `launch.json` 이식성 — `executable` 에 `blink_test.elf` 가 하드코딩돼 있어
   p1 을 시작하면 걸린다. `${command:cmake.launchTargetPath}` 로 바꾸면 타깃을 자동 추적
4. `projects/p0-setup/docs/` 가 비어 있다 — 배선 사진과 핀맵을 p1 넘어가기 전에

---
관련 코드: [platform/g431-esc1/](../platform/g431-esc1/) · [projects/p0-setup/src/blink_test/](../projects/p0-setup/src/blink_test/)
