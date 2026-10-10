"""
2804 짐벌모터 + AS5600(TENSTAR 22.5x22.9) + B-G431B-ESC1 테스트 거치대
CadQuery 파라메트릭 모델

사용법 (VS Code):
  1) OCP CAD Viewer 패널을 연 상태에서 이 파일을 실행 -> 조립 상태가 뷰어에 표시됨
  2) 실행하면 같은 폴더의 out/ 에 부품별 STEP / STL 이 생성됨
     STEP -> FreeCAD 편집용,  STL(or STEP) -> Bambu Studio 출력용
단위: mm
"""
import math
from pathlib import Path
import cadquery as cq

# =====================================================================
# 파라미터
# =====================================================================
# --- 나사 (보유 볼트: 플라스틱 M2 / M2.5, 길이 전부 6 mm -> 셀프태핑 불가, 너트 사용) ---
M25_CLEAR = 2.9         # M2.5 통과 구멍
NUT_AF = 4.8            # M2.5 플라스틱 너트 폭 (마주보는 면, 실측)
NUT_T = 2.0             # 너트 두께 (실측)
NUT_FIT = 0.3           # 너트 홈 여유 (폭, 두께 공통)
NUT_ROOF = 0.8          # 너트 홈 위쪽 벽 두께
M25_HEAD_D = 5.0        # M2.5 머리 자리 (머리 4.5)

# --- 모터 (2804, 실측) ---
MOTOR_D = 34.4
MOTOR_H = 14.0
SLOT_R_MIN = 6.0        # 방사형 슬롯: 4홀 피치 12mm ~ 25mm 대응 (실측 홀 원 지름 약 19.8 -> r 9.9)
SLOT_R_MAX = 12.5
SLOT_W = 2.8            # M2.5
SLOT_HEAD_RECESS = 1.5  # 판 아래 나사머리 홈 깊이 -> 6mm 볼트가 모터에 3.5mm만 들어감 (모터 구멍 깊이 4mm 이상 확인)
CENTER_HOLE_D = 12.0
ROTOR_SLOT_R = (4.0, 8.5)  # 회전자 윗면 M2 구멍용 십자 슬롯 범위 (실측 배치 원 지름 약 12.8, 구멍 하나만 맞아서 슬롯으로 변경)
ROTOR_SLOT_W = 2.6
SHAFT_PROTRUDE = 0.0    # 모터 바닥면 아래로 축이 튀어나온 길이 (실측: 평평)

# --- 중공축 / 자석 ---
SHAFT_IDS = [4.5, 4.7, 4.9]   # 실측 내경 4.7 -> 3종 출력 후 꽉 끼는 것 사용
MAGNETS = {"m4": (4.0, 1.9),  # 직경방향 착자, AS5600 동봉 (기본)
           "m6": (6.0, 2.5)}  # 별도 6x2.5
MAGNET_FIT = 0.6              # 자석 홈 지름 여유 (실측: 여유 0.2로는 4.2 -> 3.7, 6.2 -> 5.8로 작게 나옴)
PLUG_HEAD_H = 3.7             # 자석 종류와 상관없이 고정 -> 자석 아랫면 높이가 같아 플러그끼리 호환

# --- AS5600 브레이크아웃 (TENSTAR, 실측) ---
SENSOR_W = 22.7               # 가로 (핀 헤더 방향)
SENSOR_L = 23.1               # 세로
SENSOR_FIT = 0.25             # 포켓 한쪽 여유 (포켓은 정사각형이라 보드를 90도 돌려도 들어감)
SENSOR_HOLE_PITCH = 16.0      # 모서리 구멍 중심 간격 (실측: 구멍 3.6, 가장자리 간격 가로 12.4 / 대각 19.15)
HEADER_ROW_Y = 6.5            # 보드 중심 ~ 헤더 줄 거리 (4핀: DIR/SCL/SDA/GPO, 3핀: VCC/OUT/GND)
HEADER_SLOT = (10.6, 4.4)     # 헤더 플라스틱 통과 슬롯 (헤더는 칩 반대편으로 납땜, 너트 홈과 안 겹치게)
SENSOR_POCKET = 1.0
PCB_T = 1.6
CHIP_H = 1.5                  # AS5600 SOIC-8 높이
MAG_GAP = 2.0                 # 스페이서 없을 때 칩 윗면 ~ 자석 거리 (스페이서로 0.4~2.0 조절, 4mm 자석은 1mm 안팎)

# --- B-G431B-ESC1 (ST 공식: ST-LINK 포함 30 x 41 mm) ---
ESC_L = 41.0
ESC_W = 30.0
ESC_CLEAR = 1.0               # 트레이 여유

# --- 구조 ---
BASE_T = 4.0
TOWER = 46.0
WALL = 3.0
TOWER_H = 26.0
PLATE_T = 4.0
POST_X = 15.0
POST_D = 9.0                  # 너트 홈이 들어가는 굵기
CARRIER_T = 4.0
CARRIER_W = 28.0
CARRIER_SCREW_T = 1.2         # 캐리어 기둥 나사 자리 남는 두께 -> 6mm 볼트가 기둥 속 너트까지 닿음
SPACERS = [0.4, 0.6, 1.0]     # 각 2개, 기둥 위에 끼워 센서를 올림 (0.2 레이어 배수, 6mm 볼트라 기둥당 최대 1.6)
INDICATOR_T = 3.0             # 6mm M2 볼트가 회전자에 3mm만 들어감
TOL = 0.2

# --- 파생값 ---
HALF = TOWER / 2
INNER = TOWER - 2 * WALL
CORNER_XY = HALF - WALL - 2
MAGNET_Z = BASE_T + TOWER_H + PLATE_T + SHAFT_PROTRUDE - PLUG_HEAD_H   # 자석 아랫면
CHIP_TOP = MAGNET_Z - MAG_GAP
POST_H = CHIP_TOP - CHIP_H - PCB_T - (CARRIER_T - SENSOR_POCKET) - BASE_T
SENSOR_POCKET_SIDE = max(SENSOR_W, SENSOR_L) + 2 * SENSOR_FIT
TRAY_IL = ESC_L + 2 * ESC_CLEAR
TRAY_IW = ESC_W + 2 * ESC_CLEAR
TRAY_X = HALF + 6 + TRAY_IL / 2 + 2
BASE_X0 = -HALF - 6
BASE_X1 = TRAY_X + TRAY_IL / 2 + 2 + 7
BASE_Y = max(TOWER, TRAY_IW + 4) / 2 + 6
CARRIER_L = INNER - 1                 # 타워 안쪽에 0.5 mm씩 여유


def rbox(x, y, h, r=3.0):
    """모서리가 둥근 직육면체 (바닥이 z=0)"""
    return cq.Workplane("XY").rect(x, y).extrude(h).edges("|Z").fillet(r)


def nut_trap(x, y, z, ang, slot_len=0.0):
    """M2.5 육각 너트 홈. ang 방향으로 꼭짓점이 향하고, slot_len > 0 이면 그 방향으로 밀어 넣는 통로"""
    af = NUT_AF + NUT_FIT
    h = NUT_T + NUT_FIT
    trap = cq.Workplane("XY", origin=(0, 0, z)).polygon(6, af * 2 / 3 ** 0.5).extrude(h)
    if slot_len > 0:
        trap = trap.union(cq.Workplane("XY", origin=(0, 0, z))
                          .center(slot_len / 2, 0).rect(slot_len, af).extrude(h))
    return trap.rotate((0, 0, 0), (0, 0, 1), ang).translate((x, y, 0))


# =====================================================================
# 1) 베이스 + 타워 + ESC 트레이
# =====================================================================
def make_base():
    base = rbox(BASE_X1 - BASE_X0, 2 * BASE_Y, BASE_T, 5).translate(
        ((BASE_X0 + BASE_X1) / 2, 0, 0))
    base = base.edges(">Z").fillet(1.0)

    tower = (rbox(TOWER, TOWER, TOWER_H, 3)
             .cut(cq.Workplane("XY").rect(INNER, INNER).extrude(TOWER_H))
             .translate((0, 0, BASE_T)))
    corners = (cq.Workplane("XY", origin=(0, 0, BASE_T))
               .pushPoints([(sx * CORNER_XY, sy * CORNER_XY)
                            for sx in (-1, 1) for sy in (-1, 1)])
               .rect(6, 6).extrude(TOWER_H))
    posts = (cq.Workplane("XY", origin=(0, 0, BASE_T))
             .pushPoints([(-POST_X, 0), (POST_X, 0)])
             .circle(POST_D / 2).extrude(POST_H))
    tray_walls = (cq.Workplane("XY", origin=(0, 0, BASE_T))
                  .pushPoints([(TRAY_X, sy * (TRAY_IW / 2 + 1)) for sy in (-1, 1)])
                  .rect(TRAY_IL, 2).extrude(6))
    rails = (cq.Workplane("XY", origin=(0, 0, BASE_T))
             .pushPoints([(TRAY_X, sy * (TRAY_IW / 2 - 3)) for sy in (-1, 1)])
             .rect(TRAY_IL - 6, 2.5).extrude(2))

    body = base.union(tower).union(corners).union(posts).union(tray_walls).union(rails)

    # --- 구멍/창 ---
    cuts = (cq.Workplane("XY", origin=(0, 0, BASE_T + TOWER_H - 12))
            .pushPoints([(sx * CORNER_XY, sy * CORNER_XY)
                         for sx in (-1, 1) for sy in (-1, 1)])
            .circle(M25_CLEAR / 2).extrude(13))
    cuts = cuts.union(cq.Workplane("XY", origin=(0, 0, BASE_T + POST_H - 10))
                      .pushPoints([(-POST_X, 0), (POST_X, 0)])
                      .circle(M25_CLEAR / 2).extrude(11))
    # 너트 홈: 모서리는 타워 안쪽에서, 기둥은 가운데 쪽에서 옆으로 밀어 넣음
    nut_z = BASE_T + TOWER_H - NUT_ROOF - NUT_T - NUT_FIT
    for sx in (-1, 1):
        for sy in (-1, 1):
            to_center = math.degrees(math.atan2(-sy, -sx))
            cuts = cuts.union(nut_trap(sx * CORNER_XY, sy * CORNER_XY, nut_z, to_center, 9))
    post_nut_z = BASE_T + POST_H - NUT_ROOF - NUT_T - NUT_FIT
    for sx in (-1, 1):
        cuts = cuts.union(nut_trap(sx * POST_X, 0, post_nut_z, 180 if sx > 0 else 0, 7))
    win_h = TOWER_H - 8
    zc = BASE_T + TOWER_H / 2 + 1
    for pos, size in [((0, -HALF, zc), (INNER - 14, WALL * 3, win_h)),   # 앞 작업창
                      ((0, HALF, zc), (INNER - 14, WALL * 3, win_h)),    # 뒤 작업창
                      ((HALF, 0, zc), (WALL * 3, INNER - 16, win_h))]:   # ESC쪽 케이블창
        cuts = cuts.union(cq.Workplane("XY").box(*size).translate(pos))
    # 케이블타이 슬롯
    zip_pts = [(TRAY_X + dx, sy * (TRAY_IW / 2 + 5))
               for dx in (-TRAY_IL / 2 + 8, TRAY_IL / 2 - 8) for sy in (-1, 1)]
    cuts = cuts.union(cq.Workplane("XY").pushPoints(zip_pts).rect(4, 2.2).extrude(BASE_T))
    # 책상 고정 구멍
    desk = [(x, sy * (BASE_Y - 5)) for x in (BASE_X0 + 5, BASE_X1 - 5) for sy in (-1, 1)]
    cuts = cuts.union(cq.Workplane("XY").pushPoints(desk).circle(2).extrude(BASE_T))
    # 타워 바닥 배선 구멍
    cuts = cuts.union(cq.Workplane("XY").center(0, -8).circle(5).extrude(BASE_T))
    return body.cut(cuts)


# =====================================================================
# 2) 모터판
# =====================================================================
def make_motor_plate():
    p = rbox(TOWER, TOWER, PLATE_T, 3)
    p = p.faces(">Z").workplane().hole(CENTER_HOLE_D)
    slot_len = SLOT_R_MAX - SLOT_R_MIN + SLOT_W
    slot_c = (SLOT_R_MIN + SLOT_R_MAX) / 2
    pts = [(slot_c, 0), (0, slot_c), (-slot_c, 0), (0, -slot_c)]
    angs = [0, 90, 0, 90]
    for (x, y), a in zip(pts, angs):
        p = p.cut(cq.Workplane("XY").center(x, y).slot2D(slot_len, SLOT_W, a).extrude(PLATE_T))
        # 아래쪽 나사머리 자리 (모터를 판에 아래서 체결)
        p = p.cut(cq.Workplane("XY").center(x, y)
                  .slot2D(slot_len + 2.7, 6.0, a).extrude(SLOT_HEAD_RECESS))
    p = (p.faces(">Z").workplane()
         .pushPoints([(sx * CORNER_XY, sy * CORNER_XY) for sx in (-1, 1) for sy in (-1, 1)])
         .cboreHole(M25_CLEAR, M25_HEAD_D + 0.4, 1.8))
    # 아래쪽 AS5600 나사머리 자리 (판을 어느 방향으로 끼워도 맞게 4곳)
    h = SENSOR_HOLE_PITCH / 2
    p = p.cut(cq.Workplane("XY")
              .pushPoints([(sx * h, sy * h) for sx in (-1, 1) for sy in (-1, 1)])
              .circle(3.25).extrude(2.0))
    # 0° 기준 삼각 표시
    mark = (cq.Workplane("XY", origin=(0, 0, PLATE_T - 0.6))
            .polyline([(-2, HALF - 4), (2, HALF - 4), (0, HALF - 1)]).close().extrude(1))
    return p.cut(mark)


# =====================================================================
# 3) 센서 캐리어 (AS5600 칩 면이 위 = 자석 쪽, 헤더 핀은 아래로)
# =====================================================================
def make_sensor_carrier():
    c = rbox(CARRIER_L, CARRIER_W, CARRIER_T, 2)
    pocket = (cq.Workplane("XY", origin=(0, 0, CARRIER_T - SENSOR_POCKET))
              .rect(SENSOR_POCKET_SIDE, SENSOR_POCKET_SIDE).extrude(SENSOR_POCKET))
    headers = (cq.Workplane("XY")
               .pushPoints([(0, HEADER_ROW_Y), (0, -HEADER_ROW_Y)])
               .rect(*HEADER_SLOT).extrude(CARRIER_T))
    relief = cq.Workplane("XY").circle(4).extrude(CARRIER_T)
    post_pts = [(-POST_X, 0), (POST_X, 0)]
    post_holes = cq.Workplane("XY").pushPoints(post_pts).circle(M25_CLEAR / 2).extrude(CARRIER_T)
    post_heads = (cq.Workplane("XY", origin=(0, 0, CARRIER_SCREW_T)).pushPoints(post_pts)
                  .circle(M25_HEAD_D / 2).extrude(CARRIER_T))
    h = SENSOR_HOLE_PITCH / 2
    screw_pts = [(h, h), (-h, -h)]   # 대각 2개면 충분, M2.5 (보드 구멍 3.6)
    screw = cq.Workplane("XY").pushPoints(screw_pts).circle(M25_CLEAR / 2).extrude(CARRIER_T)
    for x, y in screw_pts:   # 밑면 너트 홈, 면이 헤더 슬롯을 향하게
        screw = screw.union(nut_trap(x, y, 0, 30))
    return c.cut(pocket).cut(headers).cut(relief).cut(post_holes).cut(post_heads).cut(screw)


# =====================================================================
# 4) 스페이서 세트 (기둥 2개 x 5종)
# =====================================================================
def make_spacers():
    out = cq.Workplane("XY")
    for i, t in enumerate(SPACERS):
        for j in (0, 1):
            ring = (cq.Workplane("XY").circle(3).circle(M25_CLEAR / 2).extrude(t)
                    .translate((i * 11, j * 11, 0)))
            out = out.union(ring)
    return out


# =====================================================================
# 5) 자석 플러그 (중공축 아래쪽에 압입, 자석은 아래 = 센서 쪽)
# =====================================================================
def make_magnet_plug(shaft_id, magnet):
    magnet_d, magnet_h = MAGNETS[magnet]
    head_h = PLUG_HEAD_H
    stem_l = 5.0
    head = cq.Workplane("XY").circle(5).extrude(head_h)
    stem = cq.Workplane("XY").add(
        cq.Solid.makeCone((shaft_id + 0.05) / 2, (shaft_id - 0.25) / 2, stem_l,
                          pnt=cq.Vector(0, 0, head_h)))
    plug = head.union(stem)
    r = (magnet_d + MAGNET_FIT) / 2
    plug = plug.cut(cq.Workplane("XY").circle(r).extrude(magnet_h))
    # 입구 모따기: 첫 층이 눌려 퍼져도 자석이 걸리지 않게
    plug = plug.cut(cq.Workplane("XY").add(cq.Solid.makeCone(r + 0.4, r, 0.4)))
    slit = (cq.Workplane("XY", origin=(0, 0, head_h + 1))
            .rect(0.8, shaft_id + 1).extrude(stem_l))
    plug = plug.cut(slit)
    # 구분용 노치 (5.4=1개, 6.0=2개, 6.5=3개)
    n = SHAFT_IDS.index(shaft_id) + 1 if shaft_id in SHAFT_IDS else 0
    for k in range(n):
        notch = (cq.Workplane("XY").center(5, 0).circle(0.7).extrude(head_h)
                 .rotate((0, 0, 0), (0, 0, 1), k * 30))
        plug = plug.cut(notch)
    return plug


# =====================================================================
# 6) 회전 인디케이터 (회전자 윗면 M2 4개, 또는 양면테이프)
# =====================================================================
def make_indicator():
    disk = cq.Workplane("XY").circle((MOTOR_D - 1) / 2).extrude(INDICATOR_T)
    pointer = (cq.Workplane("XY")
               .polyline([(-3, 0), (3, 0), (0.8, 32), (-0.8, 32)]).close().extrude(INDICATOR_T))
    ind = disk.union(pointer)
    r0, r1 = ROTOR_SLOT_R
    for a in (45, 135, 225, 315):   # 바늘(+Y)을 피해 대각선 방향
        slot = (cq.Workplane("XY").center((r0 + r1) / 2, 0)
                .slot2D(r1 - r0 + ROTOR_SLOT_W, ROTOR_SLOT_W, 0).extrude(INDICATOR_T)
                .rotate((0, 0, 0), (0, 0, 1), a))
        ind = ind.cut(slot)
    for a in range(0, 360, 30):
        tick = (cq.Workplane("XY", origin=(0, MOTOR_D / 2 - 3.5, INDICATOR_T - 0.6))
                .rect(0.8, 4).extrude(1).rotate((0, 0, 0), (0, 0, 1), a))
        ind = ind.cut(tick)
    return ind


# =====================================================================
# 빌드 & 내보내기
# =====================================================================
parts = {
    "base": make_base(),
    "motor_plate": make_motor_plate(),
    "sensor_carrier": make_sensor_carrier(),
    "spacers": make_spacers(),
    "indicator": make_indicator(),
}
for mag in MAGNETS:
    for sid in SHAFT_IDS:
        parts[f"magnet_plug_{mag}_{sid:.1f}"] = make_magnet_plug(sid, mag)

# 조립 상태
z_plate = BASE_T + TOWER_H
z_carrier = BASE_T + POST_H
asm = cq.Assembly(name="foc_test_stand")
asm.add(parts["base"], name="base", color=cq.Color("gray80"))
asm.add(parts["motor_plate"], name="motor_plate",
        loc=cq.Location((0, 0, z_plate)), color=cq.Color("steelblue"))
asm.add(parts["sensor_carrier"], name="sensor_carrier",
        loc=cq.Location((0, 0, z_carrier)), color=cq.Color("orange"))
asm.add(cq.Workplane("XY").rect(SENSOR_W, SENSOR_L).extrude(PCB_T), name="as5600_dummy",
        loc=cq.Location((0, 0, z_carrier + CARRIER_T - SENSOR_POCKET)), color=cq.Color("lightblue"))
asm.add(cq.Workplane("XY").circle(MOTOR_D / 2).extrude(MOTOR_H), name="motor_dummy",
        loc=cq.Location((0, 0, z_plate + PLATE_T)), color=cq.Color("gray30"))
asm.add(parts["indicator"], name="indicator",
        loc=cq.Location((0, 0, z_plate + PLATE_T + MOTOR_H)), color=cq.Color("gold"))
asm.add(parts[f"magnet_plug_m4_{SHAFT_IDS[1]:.1f}"], name="magnet_plug",
        loc=cq.Location((0, 0, MAGNET_Z)), color=cq.Color("green"))
asm.add(cq.Workplane("XY").rect(ESC_L, ESC_W).extrude(1.6), name="esc1_dummy",
        loc=cq.Location((TRAY_X, 0, BASE_T + 2)), color=cq.Color("purple"))

if __name__ == "__main__":
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    for name, wp in parts.items():
        cq.exporters.export(wp, str(out / f"{name}.step"))
        cq.exporters.export(wp, str(out / f"{name}.stl"), tolerance=0.02, angularTolerance=0.1)
    asm.export(str(out / "assembly.step"))

    print(f"모터판 윗면 높이: {z_plate + PLATE_T:.1f} mm")
    print(f"자석 아랫면: {MAGNET_Z:.1f} mm, AS5600 칩 윗면(스페이서 없음): {CHIP_TOP:.1f} mm "
          f"-> 간격 {MAG_GAP:.1f} mm")
    # 6mm 볼트가 너트를 끝까지 통과하는지 (너트가 홈 바닥에 있을 때 기준)
    nut_need = NUT_ROOF + NUT_FIT + NUT_T
    print(f"볼트 길이 필요: 모서리 {PLATE_T - 1.8 + nut_need:.1f}, AS5600 {PCB_T + CARRIER_T - SENSOR_POCKET - NUT_T - NUT_FIT + NUT_T:.1f}, "
          f"기둥 {CARRIER_SCREW_T + nut_need:.1f} + 스페이서 -> 스페이서 기둥당 최대 {6 - CARRIER_SCREW_T - nut_need:.1f} mm")
    print(f"기둥 높이: {POST_H:.1f} mm")
    print(f"베이스 크기: {BASE_X1 - BASE_X0:.0f} x {2 * BASE_Y:.0f} mm")

    try:   # VS Code + OCP CAD Viewer 에서 실행 시 표시
        from ocp_vscode import show
        show(asm)
    except Exception:
        pass
