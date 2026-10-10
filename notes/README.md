# notes/

이론 학습 노트. 실습에서 막힌 걸 이론에서 찾아오는 순서 — 이론 완주 후 실습 시작 금지.

파일명은 log와 동일하게 `YYYY-MM-DD-주제.md`.

## 공부 로드맵 — 단계별로 무엇을 봐야 하나

<!-- 단계가 끝날 때마다 갱신. 실험(log)과 이론(notes)을 같은 줄에 묶는다 -->

실험은 대부분 같이 진행했으니, **로그로 "무슨 일이 있었나"를 보고 → 노트로 "왜 그런가"를
보고 → 질문에 스스로 답해보는** 순서를 권한다. 질문에 막히면 그 단계 노트로 돌아간다.

### ✅ 1. 개발 환경 — 빌드·플래싱·디버깅

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [툴체인과 빌드 파이프라인](2026-08-19-arm-toolchain-and-build-pipeline.md) · [ST 도구와 디버그 스택](2026-08-19-stm32-tooling-and-debug-stack.md) · 로그 [08-05](../log/2026-08-05-dev-environment-setup.md), [08-19](../log/2026-08-19-platform-layer-and-build-setup.md) | `.data` 는 왜 FLASH 와 RAM 양쪽에 있나? 진입점 주소는 왜 홀수인가? "gdb not found" 와 "openocd ENOENT" 는 각각 어느 층 문제인가? |

### ✅ 2. 모터를 처음 돌리기 — PWM, 데드타임, 오픈루프

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [상보 PWM 과 데드타임](2026-08-27-tim1-complementary-pwm-deadtime.md) · [오픈루프 벡터 구동](2026-08-27-openloop-vector-and-why-foc.md) · 로그 [08-27](../log/2026-08-27-first-motor-rotation.md) | 데드타임이 없으면 무슨 일이 생기나? DTG=149 는 어떻게 나왔나? duty 세 개가 어떻게 "방향이 있는 벡터"가 되나? 토크가 sin(δ) 에 비례한다는 걸로 오픈루프의 추종 부족을 설명할 수 있나? |

### ✅ 3. 각도 읽기 — 엔코더

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [자기식 엔코더 AS5600](2026-08-29-magnetic-encoder-as5600.md) · 로그 [08-29](../log/2026-08-29-encoder-bringup.md), [10-10 지그·검증](../log/2026-10-10-jig-and-encoder-validation.md) | AGC=128 은 무엇을 뜻하나? 언랩은 어떤 가정 위에 있나? "재현되지 않는 오차"는 왜 계통 오차가 아닌가? |

### ✅ 4. 각도로 모터 당기기 — 전압 모드 FOC

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [전압 모드 FOC 와 오프셋](2026-10-10-voltage-mode-foc-and-offset.md) · 로그 [10-10 첫 클로즈드루프](../log/2026-10-10-first-closed-loop-commutation.md) | 오프셋을 24점에서 재고 원형 평균하는 이유는? 정/역 속도가 같으면 왜 오프셋이 맞다는 증거인가? 돌 때 전류가 오히려 주는 이유를 V = R·I + Ke·ω 로 설명할 수 있나? |

### ✅ 5. 전류 재기와 전류 제어 — 진짜 FOC

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [Clarke·Park 와 전류 루프](2026-10-10-clarke-park-and-current-loop.md) · 로그 [전류 센싱](../log/2026-10-10-current-sense-bringup.md), [dq 관측·각도 지연](../log/2026-10-10-dq-observation-and-encoder-lag.md), [전류 루프](../log/2026-10-10-current-loop-foc.md) · [PWM 노트 §6](2026-08-27-tim1-complementary-pwm-deadtime.md) | 왜 카운터 꼭대기에서 재야 하나? 세 상 합이 0 이면 무엇이 확인되나? 회전 중 id ≠ 0 을 보고 오프셋을 의심하지 않은 근거는? 정렬을 볼 때 왜 속도가 아니라 id 를 보나? 전류 루프에서 피드포워드와 적분기는 각각 무슨 일을 하나? 전원 표시가 거의 안 변한 이유는? |

### ✅ 6. 속도 루프 — p1 완료

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [속도 루프 설계](2026-10-10-speed-loop-cascade-design.md) · [PID 캐스케이드](2026-07-22-pid-cascade-structure.md) · 로그 [속도 루프](../log/2026-10-10-speed-loop.md) | 속도 루프가 전류 루프를 "즉시 나오는 토크"로 봐도 되는 조건은? Kp = J·ωc/Kt 의 단위를 따져볼 수 있나? 대역폭을 올리니 속도 출렁임은 줄었는데 iq 출렁임은 그대로인 이유는? 극점이 임계감쇠인데 오버슈트가 생기는 이유는? 브레이크 테스트에서 anti-windup 이 없었다면 놓는 순간 무슨 일이? 루프 속도 1940 → 1810 Hz 는 무엇의 흔적이었나? |

외부 자료: Brian Douglas "Understanding PID Control" 시리즈 (캐스케이드, anti-windup, setpoint weighting 편)

### ✅ 7. 위치 루프

| 볼 것 | 스스로 답해볼 질문 |
|---|---|
| [위치 루프와 궤적](2026-10-10-position-loop-and-trajectory.md) · 로그 [위치 루프](../log/2026-10-10-position-loop.md) | 위치 루프에 적분기가 없는데 정지 오차가 0 이 되는 이유는? 30도 계단이 튀지 않았는데도 궤적을 쓰는 이유 세 가지는? 피드포워드(속도·가속)는 각각 무엇을 미리 하나? stick-slip 의 네 단계를 오늘 데이터(7.9도 → −4.2도)로 설명할 수 있나? 놓았을 때 넘어가는 양이 밀린 거리와 무관했던 이유는? |

남은 숙제: 한 바퀴에 한 번 걸리는 곳을 손으로 확인 · 마찰 보상 · 놓았을 때 넘어가는 양 줄이기 (노트 "직접 해볼 것")

### 🔄 다음 — 로봇팔과 보행 (README 로드맵)

| 공부할 개념 |
|---|
| 2축 순기구학·역기구학 · 동역학(중력 보상, 관절 토크) · 여러 모터의 동기 제어(보드 간 통신) · 시뮬레이션 도구 |

## 노트 목록

<!-- 최신이 위. 노트를 추가할 때마다 한 줄 -->

- [2026-10-10 위치 루프와 궤적 — 각도를 명령하기](2026-10-10-position-loop-and-trajectory.md)
  — 적분기 하나짜리 플랜트, 안쪽 적분기가 만드는 정지 오차 0, 사다리꼴 궤적과 피드포워드, stick-slip, 저장된 적분기
- [2026-10-10 속도 루프 설계 — 관성·토크 상수로 PI 게인 정하기](2026-10-10-speed-loop-cascade-design.md)
  — 대역폭 분리, Kp = J·ωc/Kt, 외란 억제와 대역폭, PI 영점이 만드는 13.5% 오버슈트, 조건부 적분 anti-windup
- [2026-10-10 Clarke·Park 변환과 전류 루프](2026-10-10-clarke-park-and-current-loop.md)
  — 왜 회전 좌표계인가, Clarke 진폭 불변형, Park 와 각도 오차, 측정한 플랜트로 설계한 피드포워드+적분기, 포화·anti-windup·d축 우선, 상전류 ≠ 전원 전류
- [2026-10-10 전압 모드 FOC 와 전기각 오프셋](2026-10-10-voltage-mode-foc-and-offset.md)
  — 90도 앞 당기기, 다점·원형평균 캘리브레이션, 정/역 대칭 검증, 역기전력 한 줄로 오늘 관측 전부 설명, 전류 루프 예고, 루프 지연
- [2026-08-29 자기식 절대각 엔코더 — AS5600 의 원리와 진단](2026-08-29-magnetic-encoder-as5600.md)
  — 홀 4소자와 지름방향 자석, MD/ML/MH/AGC 진단 레지스터, 언랩, 재현성·변인제거 진단 방법론, I2C 의 대가
- [2026-08-27 오픈루프 벡터 구동과 FOC 로 가는 길](2026-08-27-openloop-vector-and-why-foc.md)
  — duty 3개가 벡터가 되는 이유, 토크∝sin(δ)로 오늘 관측 3개 해석, FOC 가 필요한 이유, 다음 공부 키워드 5개
- [2026-08-27 상보 PWM 과 데드타임](2026-08-27-tim1-complementary-pwm-deadtime.md)
  — 하프브리지·션트-스루, DTG 인코딩, MOE, center-aligned 를 고른 이유, CubeMX OC vs PWM 함정
- [2026-08-19 ST 도구 지형도와 디버그 스택](2026-08-19-stm32-tooling-and-debug-stack.md)
  — CubeMX/CubeIDE/CubeCLT 구분, GDB↔GDB서버↔프로브 5층, tasks/launch.json 역할
- [2026-08-19 ARM 크로스 툴체인과 빌드 파이프라인](2026-08-19-arm-toolchain-and-build-pipeline.md)
  — `.c`→`.o`→`.elf`→`.hex`, 링커스크립트, 섹션과 메모리맵, 부팅 순서, CMake 2단계
- [2026-07-22 PID 캐스케이드 구조](2026-07-22-pid-cascade-structure.md)
  — 전류→속도→위치 루프 중첩과 튜닝 순서
