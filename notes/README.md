# notes/

이론 학습 노트. 실습에서 막힌 걸 이론에서 찾아오는 순서 — 이론 완주 후 실습 시작 금지.

파일명은 log와 동일하게 `YYYY-MM-DD-주제.md`.

## 목록

<!-- 최신이 위. 노트를 추가할 때마다 한 줄 -->

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
