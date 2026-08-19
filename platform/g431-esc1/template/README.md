# 새 실험 시작하기

CubeMX를 새로 돌리지 않는다. 이 폴더를 복사하는 것으로 끝난다.

```powershell
# 1) 실험 폴더 만들기
$name = "p1-foc-bringup"
$proj = "C:\SVN\Robot-Control-Lab\projects\$name"
New-Item -ItemType Directory -Force -Path "$proj\src\foc_bringup", "$proj\data", "$proj\docs" | Out-Null

# 2) 템플릿 복사
Copy-Item "C:\SVN\Robot-Control-Lab\platform\g431-esc1\template\*" "$proj\src\foc_bringup\" -Exclude README.md

# 3) CMakeLists.txt 의 @TARGET@ 을 타깃 이름으로 바꾸기 (예: foc_bringup)
```

`@TARGET@` 을 바꾸고 `app_main.c` 를 쓰면 끝. HAL/CMSIS/startup/링커스크립트는
`platform/g431-esc1` 한 벌을 그대로 쓴다.

주변장치를 새로 켜야 하면 `platform/g431-esc1/g431-esc1.ioc` 를 CubeMX로 열어
설정하고 생성한다. 생성물은 `platform/g431-esc1/Core` 와 `Drivers` 로만 들어가며,
모든 실험이 그 변경을 함께 받는다. 과거 실험의 재현은 git 커밋/태그로 확보한다.

`CMakePresets.json` 의 `toolchainFile` 상대경로(`../../../../`)는
`projects/<실험>/src/<타깃>/` 깊이를 기준으로 한 것이다. 깊이를 바꾸면 함께 고친다.
