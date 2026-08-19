# 새 실험 시작하기

CubeMX를 새로 돌리지 않는다. 이 폴더를 복사하는 것으로 끝난다.

## 들어 있는 것

| 파일 | 역할 | `@TARGET@` 치환 |
|---|---|---|
| `CMakeLists.txt` | platform.cmake 를 찾아 `add_lab_firmware()` 호출 | **필요** |
| `CMakePresets.json` | Debug/Release 프리셋 + 툴체인 파일 지정 | 불필요 |
| `app_main.c` | 실험 코드 진입점 — 여기가 본편 | 불필요 |
| `.clangd` | clangd 가 `build/Debug/compile_commands.json` 을 보게 함 | 불필요 |
| `.vscode/tasks.json` | `build` / `flash` 태스크. `launch.json` 의 `preLaunchTask` 가 부름 | **필요** |
| `.vscode/c_cpp_properties.json` | IntelliSense 용 compile_commands 경로 | 불필요 |
| `.vscode/settings.json` | CMake Tools 가 쓸 cmake(`cube-cmake`) 지정 | 불필요 |

## 복사 절차

```powershell
# 1) 이름 정하기
$repo   = "C:\SVN\Robot-Control-Lab"
$name   = "p1-foc-bringup"   # 실험 폴더 이름
$target = "foc_bringup"      # 빌드 타깃 = .elf 이름

# 2) 폴더 만들기
$dst = "$repo\projects\$name\src\$target"
New-Item -ItemType Directory -Force -Path $dst, "$repo\projects\$name\data", "$repo\projects\$name\docs" | Out-Null

# 3) 템플릿 복사 (-Recurse 없으면 .vscode\ 가 안 따라온다)
Copy-Item "$repo\platform\g431-esc1\template\*" $dst -Recurse -Exclude README.md

# 4) @TARGET@ 치환 (CMakeLists.txt, .vscode\tasks.json)
Get-ChildItem $dst -Recurse -File | ForEach-Object {
    $t = [System.IO.File]::ReadAllText($_.FullName)
    if ($t -match "@TARGET@") {
        [System.IO.File]::WriteAllText($_.FullName,
            ($t -replace "@TARGET@", $target),
            (New-Object System.Text.UTF8Encoding $false))
    }
}
```

`app_main.c` 를 쓰면 끝. HAL/CMSIS/startup/링커스크립트는 `platform/g431-esc1` 한 벌을 그대로 쓴다.

## 빌드

VS Code 는 **실험 폴더(`src/<타깃>/`)를 열어야 한다.** `.vscode/` 가 거기 있고
`launch.json` 이 `${workspaceFolder}` 를 쓰기 때문에, 레포 루트를 열면 경로가 어긋난다.

- VS Code: 상태바에서 프리셋 `Debug` 선택 → `F7` (또는 `Ctrl+Shift+B` = `build` 태스크)
- 터미널:

```powershell
cd $dst
cmake --preset Debug        # configure — 한 번만
cmake --build --preset Debug
```

## launch.json 은 직접 만들어야 한다

STM32CubeCLT 절대경로가 박혀 PC마다 다르므로 커밋 대상이 아니다(`.gitignore`).
`projects/p0-setup/src/blink_test/.vscode/launch.json` 을 복사한 뒤
`executable` 의 `.elf` 이름을 타깃 이름으로 바꾼다. 전체 내용은
`log/2026-08-05-dev-environment-setup.md` §2 에 기록돼 있다.

`preLaunchTask: "build"` 를 반드시 남겨둘 것 — VS Code + CMake 조합은
IDE 와 달리 `F5` 가 자동 빌드를 하지 않아서, 이게 없으면 이전 바이너리로 디버깅한다.

## 주변장치를 새로 켜야 하면

`platform/g431-esc1/g431-esc1.ioc` 를 CubeMX로 열어 설정하고 생성한다.
생성물은 `platform/g431-esc1/Core` 와 `Drivers` 로만 들어가며, 모든 실험이 그 변경을
함께 받는다. 과거 실험의 재현은 git 커밋/태그로 확보한다.

`CMakePresets.json` 의 `toolchainFile` 상대경로(`../../../../`)는
`projects/<실험>/src/<타깃>/` 깊이를 기준으로 한 것이다. 깊이를 바꾸면 함께 고친다.
