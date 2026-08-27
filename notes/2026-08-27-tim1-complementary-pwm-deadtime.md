# 2026-08-27 상보 PWM 과 데드타임 — 3상 인버터를 타이머 하나로

<!-- 오늘 실제 설정한 값(ARR=4249, DTG=149)을 예로 쓴다. 검증: 보드에서 완주 -->

## 왜 찾아봤나

모터를 돌리려면 MOSFET 6개를 켜고 꺼야 하는데, 왜 "상보(complementary)" 출력이
필요한지, 데드타임이 뭘 막는지, TIM1 설정 항목들이 각각 뭘 하는지 모르는 채로
CubeMX 를 클릭할 수는 없었다. 값 하나 틀리면 MOSFET 이 물리적으로 죽는 영역이다.

## 핵심

### 1. 하프브리지 — 상 하나당 스위치 둘

한 상(U)의 출력 전압을 만드는 구조:

```
 12V ──┬── [상단 MOSFET]──┬──→ 모터 U상
       │      (High)      │
       │                  │
 GND ──┴── [하단 MOSFET]──┘
              (Low)
```

- 상단 ON → U상에 12V
- 하단 ON → U상에 0V
- **둘 다 ON → 12V가 GND 로 직결. 이게 션트-스루(shoot-through)** —
  전류 경로에 모터 권선(21Ω)이 없다. MOSFET 저항(수 mΩ)뿐이라 수십 A 가 흐른다

그래서 상단과 하단은 **항상 반대**여야 한다 = 상보 신호.
3상이면 하프브리지 3개 = 스위치 6개 = PWM 신호 6개.

### 2. 데드타임 — "반대"만으로는 부족하다

논리적으로 반대여도 물리적으로는 겹친다. MOSFET 은 끄라고 해도 **바로 꺼지지
않는다** (게이트 전하 방전 + 드라이버 전파지연). 상단이 꺼지는 중에 하단이 켜지기
시작하면 그 겹침 구간 동안 션트-스루다.

**데드타임 = 하나를 끄고 나서 반대쪽을 켜기까지 일부러 두는 공백.**

우리 보드 값 유도 (L6387E 데이터시트 Table 5):

```
DT_min ≳ (toff전파지연 + MOSFET 게이트 방전) − ton전파지연
       ≈ (약 200ns + 약 250ns) − 약 70ns  ≈  380ns
```

첫 브링업은 ×2 여유로 **1µs**. 20kHz 주기 50µs 의 2%라 성능 손실이 사실상 없고,
이 보드는 BKIN(하드웨어 차단)이 없어 보수적으로 가는 게 맞다.

> L6387E 에는 인터록(양쪽 입력 동시 High 방지)이 내장돼 있지만, 그건 **입력 논리**
> 보호다. MOSFET 이 꺼지는 데 걸리는 **물리적 시간**은 못 막는다. 데드타임은 여전히 필요.

### 3. TIM1 이 6개 신호를 만드는 방법

TIM1 은 "advanced timer" 라 채널마다 **CHx 와 CHxN(상보) 한 쌍**을 하드웨어로 낸다.
데드타임 삽입도 하드웨어다 — 소프트웨어가 개입할 틈이 없어서 안전하다.

```
카운터: 0 → 4249 → 0 → 4249 …  (center-aligned: 올라갔다 내려온다)

CH1  ────┐________┌────      CNT < CCR1 일 때 High (PWM mode 1)
CH1N ____│~~~~~~~~│____      CH1 의 반전 + 양쪽 에지에 데드타임 삽입
         ↑        ↑
      deadtime  deadtime
```

오늘 설정의 의미:

| 설정 | 값 | 왜 |
|---|---|---|
| Counter Mode | Center-aligned 1 | 카운터가 산 모양. **전류 샘플링 때문** (아래 6) |
| ARR | 4249 | 170MHz ÷ (2×4250) = 20kHz. ×2 는 산을 오르내리는 시간 |
| CKD (ClockDivision) | DIV1 | 데드타임 시계 t_DTS = 1/170MHz. **DTG 계산의 기준** |
| Pulse (CCR) | 0 에서 시작 | 듀티는 코드에서. 켜지자마자 토크가 나가면 안 되니까 |
| OCPolarity / OCN | High/High | L6387 입력이 액티브 하이 |

### 4. DTG — 데드타임을 8비트에 담는 인코딩

BDTR 레지스터의 DTG[7:0] 은 단순한 배수가 아니라 **구간별 인코딩**이다
(RM0440). 상위 비트가 스케일을 고른다:

| DTG[7:5] | 식 | 범위 (t_DTS=5.88ns) |
|---|---|---|
| `0xx` | DTG × t_DTS | 0 ~ 747ns |
| `100` | (64 + DTG[5:0]) × 2 × t_DTS | 753 ~ 1118ns |
| `110` | (32 + DTG[4:0]) × 8 × t_DTS | 1.5 ~ 3.0µs |
| `111` | (32 + DTG[4:0]) × 16 × t_DTS | 3.0 ~ 5.9µs |

1µs 는 첫 구간을 넘으므로 `100` 인코딩:
**(64+21) × 2 × 5.882ns = 1000.0ns → DTG = 0b100_10101 = 149**

CubeMX 의 "Dead Time" 칸에 넣는 숫자가 ns 가 아니라 **이 DTG 값**이라는 게 함정.

### 5. MOE — 6채널의 마스터 스위치

BDTR 의 MOE(Main Output Enable) 비트가 0이면 CCR 이 뭐든 6핀 전부
**Idle State** 로 간다. 우리 설정은 Idle = Reset(Low) → 드라이버 입력 전부 Low
→ **MOSFET 6개 전부 off**.

이 보드는 게이트 드라이버 EN 핀이 없으므로 **MOE 가 출력을 끊는 유일한 수단**이다.
`bsp_pwm_stop()` 이 채널 정지 후 MOE 까지 내리는 이유.

- Automatic Output = Disable 로 둔 이유: MOE 를 하드웨어가 멋대로 다시 켜지 않고
  소프트웨어가 명시적으로 제어하게

### 6. 왜 center-aligned 인가 — 다음 단계의 복선

edge-aligned 로도 모터는 돈다. center-aligned 를 고른 건 **전류 센싱** 때문:

```
카운터 꼭대기(ARR 부근)에서는 CNT > CCR (듀티 100% 미만이면 항상)
→ 상단 3개 전부 OFF, 하단 3개 전부 ON
→ 3개 션트 저항에 상전류가 그대로 흐르는 유일한 순간
→ 여기서 ADC 를 트리거하면 스위칭 노이즈 없이 깨끗하게 읽힌다
```

edge-aligned 면 이 "조용한 창"이 주기 끝에 몰리고 좁다. FOC 용 타이머가
전부 center-aligned 인 이유.

### 7. 오늘 밟은 함정 — Output Compare ≠ PWM Generation

CubeMX 채널 목록에 `Output Compare CHx CHxN` 과 `PWM Generation CHx CHxN` 이
나란히 있다. 전자를 고르면 `TIM_OCMODE_TIMING`(Frozen) — 비교가 일어나도 핀이
안 움직인다. **생성된 코드에서 `OCMode = TIM_OCMODE_PWM1` 인지 확인하는 것**이
가장 빠른 검증이다.

## 실습 연결

- 데드타임 검증을 스코프 없이 했다: 전원 리밋 0.8A 걸고 완주(리셋 없음) +
  전류 0.2A 미만 = 션트-스루 없음. 정석은 스코프로 게이트 파형 실측 후 줄이는 것
- 전류 센싱 단계에서: TIM1 TRGO 를 Update 로 잡고 ADC injected 트리거 →
  카운터 꼭대기 샘플링. RM0440 의 TIM1 "master mode" 와 ADC "injected conversion" 장 참고
- 듀티를 올리면 조용한 창이 좁아진다 — 고듀티에서 샘플링이 왜 어려운지,
  min pulse 제약이 왜 생기는지가 여기서 나온다

## 출처

- L6387E 데이터시트 Table 5 (ton 110 / toff 105 / tr 50 / tf 30 ns, Typ.)
- RM0440 — TIM1 BDTR.DTG 인코딩, MOE/OSSI/OSSR
- 실측: `projects/p1-foc-bringup` 오늘 세션 ([로그](../log/2026-08-27-first-motor-rotation.md))
- 코드: [`platform/g431-esc1/bsp/Src/bsp.c`](../platform/g431-esc1/bsp/Src/bsp.c) 의 PWM 계층
