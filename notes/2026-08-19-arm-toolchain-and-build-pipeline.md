# 2026-08-19 ARM 크로스 툴체인과 빌드 파이프라인 — .c 가 .elf 가 되기까지

<!-- 이 노트의 숫자는 전부 p0-setup/blink_test 의 실제 빌드 결과에서 뽑은 것 -->

## 왜 찾아봤나

빌드가 "된다/안 된다"만 알고 그 사이에서 뭐가 일어나는지 몰랐다.
`arm-none-eabi-gcc`, 링커스크립트, `.elf`/`.hex`/`.bin`, `--specs=nano.specs` 같은 게
CubeMX가 만들어준 대로 그냥 돌아가고 있었을 뿐이라, 뭔가 깨지면 어디를 봐야 할지 몰랐다.

## 핵심

### 1. 크로스 컴파일 — 이름에 답이 있다

`arm-none-eabi-gcc` 는 `<타깃 CPU>-<OS>-<ABI>-<도구>` 규칙이다.

| 조각 | 뜻 |
|---|---|
| `arm` | 만들어낼 코드가 도는 CPU. 내 PC(x86_64)가 아니다 |
| `none` | **OS 가 없다** (베어메탈). 리눅스용이면 `arm-linux-gnueabihf-` |
| `eabi` | Embedded ABI — 함수 호출 규약, 구조체 배치 등의 약속 |

`none` 이 중요하다. OS가 없으니 `main()` 을 불러주는 것도, 메모리를 나눠주는 것도
없다. 그 일을 **startup 코드와 링커스크립트가 대신**한다.

같은 접두어로 도구가 한 벌씩 있다. 우리 빌드가 실제로 쓰는 것:

| 도구 | 하는 일 |
|---|---|
| `arm-none-eabi-gcc` | 컴파일 (`.c` → `.o`), 어셈블 (`.s` → `.o`), 링크 호출까지 |
| `arm-none-eabi-objcopy` | `.elf` → `.hex` / `.bin` 형식 변환 |
| `arm-none-eabi-size` | 섹션 크기 출력 (빌드 끝에 나오는 그 표) |
| `arm-none-eabi-gdb` | 디버거 (→ 디버그 스택 노트 참고) |
| `arm-none-eabi-objdump` | 디스어셈블·섹션 덤프. "정말 그 코드가 들어갔나" 확인용 |

### 2. 파이프라인 4단계

```
.c / .s  --[컴파일·어셈블]-->  .o  --[링크]-->  .elf  --[objcopy]-->  .hex / .bin
                                    ↑
                              링커스크립트(.ld)
```

blink_test 는 22개 `.o` 를 만들어 하나로 링크한다
(HAL 13 + Core 6 + `bsp.c` + startup + `app_main.c`).

- **`.elf`** — 디버그 정보·심볼·섹션 주소가 다 들어있다. 884KB. **디버거가 쓰는 것**
- **`.hex`** — 주소 + 데이터를 텍스트로 적은 것. 플래시 라이터가 쓴다
- **`.bin`** — 순수 바이트열(5864B). 주소 정보가 없어서 "어디에 쓸지"를 따로 알려줘야 한다

`.elf` 가 884KB인데 실제로 칩에 들어가는 건 5864B다. 차이는 전부 디버그 정보.

### 3. 링커스크립트 — 주소를 정하는 곳

`platform/g431-esc1/STM32G431xx_FLASH.ld` 의 핵심 4줄:

```
RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 32K
FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
```

STM32G431CB 의 물리적 사실이다. `rx`(읽기·실행) / `xrw`(실행·읽기·쓰기) 권한도 여기서.
툴체인 파일이 이 스크립트를 `-T` 로 넘긴다:

```cmake
set(CMAKE_EXE_LINKER_FLAGS "... -T \"${CMAKE_CURRENT_LIST_DIR}/../STM32G431xx_FLASH.ld\"")
```

> 마이그레이션에서 이 경로를 `${CMAKE_SOURCE_DIR}` → `${CMAKE_CURRENT_LIST_DIR}/..` 로
> 바꾼 이유가 이것. 전에는 "빌드하는 프로젝트 폴더 기준"이라 platform 으로 옮기면 깨졌다.

### 4. 섹션 — 어디에 무엇이 들어가나

실제 blink_test 빌드 (`arm-none-eabi-size -A`):

| 섹션 | 크기 | 어디에 | 무엇 |
|---|---|---|---|
| `.isr_vector` | 472 B | FLASH 0x08000000 | **벡터 테이블** (아래 참고) |
| `.text` | 5356 B | FLASH | 코드 |
| `.rodata` | 16 B | FLASH | `const` 상수, 문자열 리터럴 |
| `.data` | 12 B | **FLASH + RAM** | 초기값 있는 전역변수 |
| `.bss` | 32 B | RAM | 초기값 없는(=0) 전역변수 |
| `._user_heap_stack` | 1540 B | RAM | 힙·스택 예약 |

빌드 끝의 숫자가 여기서 나온다.

```
FLASH: 5864 B = 472 + 5356 + 16 + 12(.data 초기값) + 8(init/fini_array)
RAM  : 1584 B = 12(.data) + 32(.bss) + 1540(heap/stack)
```

**`.data` 가 양쪽에 다 있는 이유**: `int x = 5;` 는 실행 중 값이 바뀔 수 있으니 RAM 에
있어야 하지만, 전원을 꺼도 초기값 `5` 는 어딘가 남아야 하니 FLASH 에도 사본이 있다.
부팅 때 startup 코드가 FLASH → RAM 으로 복사한다. `.bss` 는 0 으로 미는 것뿐이라
FLASH 를 안 쓴다. **큰 배열을 `= {0}` 으로 초기화하면 `.data` 로 가서 플래시를
그만큼 먹는다** — 초기화를 생략하면 `.bss` 로 간다.

### 5. 부팅 순서 — `main()` 은 시작점이 아니다

벡터 테이블 앞 2워드를 직접 덤프해보면:

```
8000000: 00800020 1d040008 ...
         ^^^^^^^^ ^^^^^^^^
         워드0     워드1
```

리틀엔디언이라 뒤집어 읽는다.

- **워드0 = `0x20008000`** — 초기 스택 포인터. `0x20000000 + 32K` = **RAM 최상단**.
  스택은 위에서 아래로 자란다
- **워드1 = `0x0800041d`** — `Reset_Handler` 주소. ELF 헤더의 Entry point 와 같은 값

전원이 들어오면 **CPU 가 하드웨어적으로** 이 두 워드를 읽어 SP 를 세우고 워드1로 점프한다.
그 다음이 소프트웨어다:

```
Reset_Handler (startup_stm32g431xx.s)
  ├─ .data 를 FLASH → RAM 복사
  ├─ .bss 를 0 으로 clear
  ├─ SystemInit()
  ├─ __libc_init_array()   (C 런타임 초기화)
  └─ main()                ← 여기서야 C 코드 시작
       HAL_Init() → SystemClock_Config() → MX_GPIO_Init() → app_main()
```

**진입점 주소가 홀수(`0x800041d`)인 이유**: Cortex-M 은 Thumb 명령어만 실행한다.
점프 주소의 **최하위 비트 1 = Thumb 모드** 라는 표시다. 실제 명령어는 짝수 주소
(`0x800041c`) 에 있고, 홀수는 "Thumb 으로 실행하라"는 플래그가 붙은 것.
이 비트를 빼먹으면 HardFault 가 난다.

### 6. 컴파일 플래그 — 왜 이 값인가

`platform/g431-esc1/cmake/gcc-arm-none-eabi.cmake` 에서:

| 플래그 | 뜻 | 틀리면 |
|---|---|---|
| `-mcpu=cortex-m4` | 명령어 집합 | 없는 명령어를 써서 HardFault |
| `-mfpu=fpv4-sp-d16` | **단정밀도** FPU (G431 은 `double` 하드웨어 없음) | `double` 연산이 조용히 소프트웨어로 |
| `-mfloat-abi=hard` | FPU 레지스터로 float 전달 | **라이브러리와 ABI 불일치 → 링크 에러** |
| `-ffunction-sections -fdata-sections` | 함수·변수를 각각 별도 섹션으로 | 아래 `--gc-sections` 가 무의미해짐 |
| `-Wl,--gc-sections` | **안 쓰는 섹션 버리기** | 코드 크기가 확 늘어난다 |
| `--specs=nano.specs` | newlib-nano (작은 표준 C 라이브러리) | `printf` 하나로 수십 KB |

`--gc-sections` 가 실제로 동작하는 걸 이번에 확인했다: `bsp.c` 의 weak `app_main()` 이
`.map` 에서 주소 `0x00000000` / 크기 0 으로 남아 있다 = 링커가 버린 것.

**`-O0` 과 디버깅**: Debug 는 `-O0 -g3`. 최적화를 켜면(`-Os`) 변수가 레지스터에만
살거나 코드 순서가 바뀌어서 브레이크포인트가 엉뚱한 줄에 걸린다. Release 에서만 켠다.

### 7. CMake — 두 단계인 이유

CMake 는 빌드 도구가 아니라 **빌드 파일을 만드는 도구**다.

| 단계 | 명령 | 결과 |
|---|---|---|
| configure | `cmake --preset Debug` | `build/Debug/build.ninja` + `CMakeCache.txt` |
| build | `cmake --build --preset Debug` | ninja 가 실제 컴파일·링크 |

- **툴체인 파일**(`CMAKE_TOOLCHAIN_FILE`)은 "컴파일러가 누구냐"를 configure **맨 처음**에
  정한다. 그래서 나중에 바꾸려면 빌드 폴더를 지워야 한다 (캐시에 절대경로가 박힌다)
- **`CMakeCache.txt`** 가 컴파일러 경로를 기억한다. 같은 빌드 폴더를 서로 다른 cmake 로
  번갈아 쓰면 여기서 충돌한다 (VS Code 태스크를 `type:"cmake"` 로 위임한 이유)
- **`CMakePresets.json`** 은 위 인자들(제너레이터·빌드폴더·빌드타입·툴체인)을 이름으로
  묶어둔 것. `--preset Debug` 한 마디로 대체된다
- **`file(GLOB ... CONFIGURE_DEPENDS)`** — 소스 목록을 패턴으로 잡고, 폴더가 바뀌면
  ninja 가 configure 를 자동 재실행한다. 빌드 첫 줄의
  `Re-checking globbed directories...` 가 그 동작

## 실습 연결

- CubeMX 에서 ADC/TIM 을 켜면 `Drivers/.../Src` 에 `.c` 가 늘고, `CONFIGURE_DEPENDS`
  글롭이 자동으로 잡는다. `platform.cmake` 를 손댈 일이 없다
- FOC 로 가면 `float` 연산이 많아진다. `-mfpu=fpv4-sp-d16` 은 **단정밀도만** 하드웨어다.
  `sinf()` 를 써야 하고 `sin()`(double) 을 쓰면 소프트웨어 에뮬레이션으로 느려진다
- 전류 루프를 수십 kHz 로 돌릴 때 크기·속도가 문제되면 Release(`-Os`) 로 재확인
- 큰 로그 버퍼를 잡을 때 `.bss` / `.data` 구분을 기억할 것. RAM 은 32KB 뿐이다

## 출처

- 이 노트의 모든 숫자: `projects/p0-setup/src/blink_test/build/Debug/` 의 실제 산출물
  (`arm-none-eabi-size -A`, `readelf -h/-S`, `objdump -s -j .isr_vector`, `blink_test.map`)
- `platform/g431-esc1/STM32G431xx_FLASH.ld`, `startup_stm32g431xx.s`,
  `cmake/gcc-arm-none-eabi.cmake` — CubeMX 생성물
- 관련 기록: [2026-08-19 보드 공용 계층 분리와 빌드 경로 정리](../log/2026-08-19-platform-layer-and-build-setup.md)
