# projects/

코드·CAD·측정 데이터 보관소. 설명과 서사는 [log/](../log/)에 쓰고 여기서 링크.

폴더는 시작할 때 만든다. 규칙:

```
projects/p1-foc-bringup/
├── src/    # 실험 코드 (펌웨어·스크립트)
│   └── foc_bringup/
│       ├── CMakeLists.txt    # 짧다. platform/g431-esc1 을 참조만
│       ├── CMakePresets.json
│       └── app_main.c        # 여기가 본편
├── data/   # 측정 데이터 (선별해서 커밋)
└── docs/   # 회로도, 배선 사진 등
```

## 여기 두지 않는 것

HAL·CMSIS·startup·링커스크립트·툴체인 cmake·CubeMX `.ioc` 는 실험마다 같으므로
`platform/g431-esc1/` 한 벌만 쓴다. 실험 폴더에서 CubeMX를 새로 돌리지 않는다.
보드 공용 코드(핀 래퍼, 센서 읽기 등)도 `platform/g431-esc1/bsp/` 로 간다.

## 새 실험 시작

`platform/g431-esc1/template/` 를 복사한다.
자세한 절차는 [template/README.md](../platform/g431-esc1/template/README.md).

## 빌드

```powershell
cd projects\p0-setup\src\blink_test
cmake --preset Debug
cmake --build --preset Debug
```

`.elf`/`.hex`/`.bin` 과 크기 출력이 `build/Debug/` 에 나온다.
