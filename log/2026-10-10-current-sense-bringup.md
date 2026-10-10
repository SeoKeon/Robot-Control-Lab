# 2026-10-10 전류 센싱 브링업 — 3션트, 정지 벡터로 세 상 전류 확인

## 뭘 하려고 했나

[엔코더 기반 정류](2026-10-10-first-closed-loop-commutation.md)는 됐지만 전압 모드라
**잡고 있으면 전류가 V/R 까지 커지고 막을 장치가 없다.** 진짜 FOC(전류 루프)로
가려면 세 상 전류를 재야 한다. 보드에 션트 3개와 MCU 내장 OPAMP 3개가 이미 있다.

## 뭘 했나

### 설정값을 먼저 확정했다

회로도 텍스트만으로는 OPAMP 피드백 구성(내부 게인 vs 외부 저항)을 가를 수 없었다.
이 보드를 지원하는 공개 코드 두 곳으로 확정했다:

- SimpleFOC `b_g431_mcu.cpp`: OPAMP `PGA_MODE`, `INVERTINGINPUT_IO0_BIAS`, 게인 16,
  high speed, 공장 트림
- NuttX B-G431B-ESC1 문서: 션트 3 mΩ, PGA 16, **유효 게인 −9.14** (보드 바이어스·
  감쇠 회로 포함, 부호 반전)

→ **1 count = 3.3/4096 ÷ (0.003 × 9.14) ≈ 29.4 mA.** 이 보드는 40 A 급이라
0.2 A 급 모터에선 신호가 몇 count 밖에 안 된다 — 평균이 전제다.

### CubeMX (3곳)

| 주변장치 | 설정 |
|---|---|
| OPAMP1/2/3 | `PGA Internally connected_IO0_BIAS`, 게인 16, High Speed. 핀 PA1/PA3, PA7/PA5, PB0/PB2 |
| ADC1 / ADC2 | VOPAMP1 / VOPAMP2·VOPAMP3 (내부 연결), injected, 트리거 TIM1 TRGO rising, 6.5 cycles, 클럭 SYNC ÷4 = 42.5 MHz |
| TIM1 | Repetition Counter 1, TRGO = Update |

ADC 에서 VOPAMP 채널을 고르니 OPAMP 출력이 자동으로 내부 연결됐다(`InternalOutput = ENABLE`).
TIM1 에는 TRGO 와 TRGO2 두 칸이 있는데, ADC 트리거로 고른 "Trigger Out event" 가
TRGO 이므로 그쪽만 Update 로 바꿨다.

### 핵심 함정 — 꼭대기에서 재야 한다

center-aligned 카운터는 꼭대기와 바닥에서 두 번 갱신되고, RCR=1 로 주기당 한 번만
남긴다. 그런데 **남는 게 꼭대기인지 바닥인지는 타이머를 시작한 방식에 달려 있다.**

- 꼭대기: 하단 MOSFET 3개 ON → 션트에 상전류 → 이때 재야 한다
- 바닥: 상단 ON → 션트 전류 0 → 여기서 재면 항상 영점만 나온다

짐작하지 않고 측정했다. ADC 변환이 끝난 순간의 카운터 값과 방향을 읽어서,
**하강 중이고 ARR 근처면 꼭대기**. 시작 방식 두 가지를 시험해 꼭대기인 쪽을 쓴다.
결과: 변환 직후 CNT = 4138 (ARR 4249), 하강 → 꼭대기 OK. PWM 중에도 4112, 하강.

또 하나: HAL 의 `PWM_Stop` 은 채널을 다 끄면 **카운터까지 멈춘다.** 그러면 ADC
트리거가 끊기고 정렬도 흐트러진다. `bsp_pwm_stop()` 이 카운터는 계속 돌리게 고쳤다
(MOE=0 이라 출력은 전부 off).

### 결과

**영점** (PWM off, 2000 샘플): U / V / W = 2534 / 2546 / 2532 count (약 2.04 V),
노이즈 σ 1.0~1.2 count. 영점이 1.65 V 가 아니라 2.04 V 인 것은 게인이 음수라
양의 전류가 값을 *내리기* 때문에 그쪽에 여유를 둔 바이어스 설계다.

**정지 벡터** (m=0.45, 600 ms 정착 후 1000 샘플 평균, mA):

| 전기각 | U | V | W | 합 |
|---|---|---|---|---|
| 0 | **+233.6** | −102.9 | −132.3 | −1.6 |
| 120 | −124.8 | **+260.6** | −145.8 | −10.1 |
| 240 | −123.1 | −106.1 | **+219.0** | −10.1 |

예측: 선간 4.05 V ÷ (10.5 + 5.25 Ω) ≈ **260 mA** 가 벡터 방향 상으로 들어가고
나머지 둘로 반씩 나온다. 원본: [`data/…current-sense-static-vector.txt`](../projects/p1-foc-bringup/data/2026-10-10-current-sense-static-vector.txt)

## 막힌 것과 해결 과정

### CubeMX 의 "저장"과 "생성"

중간 확인은 `Ctrl+S` 로 충분했다 — `.ioc` 는 텍스트라 저장만 하면 설정을 파일에서
읽어 검증할 수 있다. 다만 CubeMX 는 **기본값과 같은 항목은 파일에 안 적어서**
(ADC 클럭 분주, 트리거 엣지) 생성된 코드로 최종 확인했다.

## 배운 것 / 다음 할 것

- **세 상 합 = 0 이 공짜 검증이다.** 키르히호프 법칙상 반드시 0 이어야 하므로, 합이
  0 이면 세 채널의 게인·영점이 서로 맞는다는 뜻이다. 오늘 −1.6 ~ −10 mA,
  1 count(29 mA) 미만
- **큰 값이 U→V→W 로 옮겨가는 것**으로 상 배치와 ADC 채널 매핑을 한 번에 확인
- **양자화를 알고 보면 오차가 읽힌다.** V·W 의 −103 대 −132 는 3.5 대 4.5 count —
  딱 1 count 차이. "이상한 값"이 아니라 분해능 한계다
- 평균을 내면 8~9 count 신호도 쓸 만하다. 단 **전류 루프는 매 주기 값을 쓴다** —
  평균 없이 σ ≈ 1 count(≈35 mA) 노이즈가 그대로 들어온다. 루프 게인을 정할 때
  이 노이즈가 한계가 될 것

### 다음

1. **Clarke / Park 변환** — (iU, iV, iW) → (iα, iβ) → 엔코더 각도로 회전 → (id, iq)
2. 전압 모드 정류를 돌리면서 id, iq 를 찍어 확인: **id ≈ 0, iq 는 부하(손가락)에 따라 증가**
3. 그다음 iq·id 에 PI 제어기 → 전류 루프 = FOC

---
관련 코드: [`platform/g431-esc1/bsp/`](../platform/g431-esc1/bsp/) (전류 센싱 계층) ·
[`projects/p1-foc-bringup/`](../projects/p1-foc-bringup/)
관련 노트: [PWM 과 데드타임 §6 (center-aligned 를 고른 이유)](../notes/2026-08-27-tim1-complementary-pwm-deadtime.md) ·
[전압 모드 FOC](../notes/2026-10-10-voltage-mode-foc-and-offset.md)
