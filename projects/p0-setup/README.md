# p0-setup — 환경 구축

**목표**: 실험대 세팅 — 배선, 마운트, 개발환경. Blink + 시리얼 "hello"가 완료 기준.

- `src/blink_test/` — LED 블링크 (빌드→플래싱→디버깅 완주 확인). 실험 코드는 `app_main.c` 하나
- `data/` — (이 단계에선 없음)
- `docs/` — [핀맵과 보드 정보](docs/pinmap.md). 배선 사진은 아직 없음

보드 공용 자산(HAL/CMSIS, startup, 링커스크립트, CubeMX `.ioc`)은
[`platform/g431-esc1/`](../../platform/g431-esc1/) 에 있다.

## 빌드

```powershell
cd src\blink_test
cmake --preset Debug
cmake --build --preset Debug
```

## 관련 기록

- [2026-07-22 프로젝트 시작](../../log/2026-07-22-kickoff.md)
- [2026-08-01 도착 부품 테스트](../../log/2026-08-01-parts-testing.md)
- [2026-08-05 개발환경 세팅](../../log/2026-08-05-dev-environment-setup.md)
