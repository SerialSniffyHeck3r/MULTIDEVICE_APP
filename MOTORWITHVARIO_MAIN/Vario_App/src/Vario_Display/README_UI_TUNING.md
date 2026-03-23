# VARIO flight display UI tuning bundle

이 폴더는 `VARIO_APPLICATED/Vario_App/Vario_Display` 교체용 묶음이다.

## 포함된 핵심 변경

- 좌측 14px = VARIO gauge, 우측 14px = GS gauge 로 고정
- MODE 1 / 2 / 3 모두 동일한 좌우 bar shell 공유
- VARIO 숫자에서 +/- 부호 제거, 대신 up/down icon 표시
- VARIO instant / average gauge 분리
  - 좌측 8px instant fill
  - 좌측 10~14px 영역 average fill
- GS instant / average gauge 분리
  - 우측 8px instant fill
  - 우측 10~14px 영역 average arrow indicator
- 하단 숫자 block 을 고정 박스로 만들어
  - GS `99.9`
  - VARIO `19.9`
  가 다른 영역으로 침범하지 않게 정렬
- 좌상단 FLT TIME / GLD row 재배치
- 상단 중앙 clock 확대
- 우상단 ALT1 top-right 정렬 + ALT2/ALT3 inline row 재배치
- Screen 1: 원형 compass + `to START / to WP` 거리 문자열
- Screen 2: compass 제거, breadcrumb GPS trail 배경 적용
- Screen 3: 동일 shell 유지한 stub

## waypoint / start target 처리

현재 공개된 런타임 snapshot 만으로는 display 계층에서 즉시 사용할 수 있는
전용 waypoint 저장소가 없다고 가정하고 다음처럼 구성했다.

- `START`
  - trail ring buffer 의 가장 오래된 점을 target 으로 사용
- `WP`
  - `Vario_Display_SetWaypointManual(lat_e7, lon_e7, valid)` 로 주입

추후 별도 waypoint 저장소가 생기면
`vario_display_compute_nav_solution()` 안의 target source 만 갈아끼우면 된다.

## 수정하기 쉬운 포인트

레이아웃 상수는 대부분 `Vario_Display_Common.c` 상단에 몰아 두었다.

특히 다음 항목만 조정해도 화면이 크게 바뀐다.

- `VARIO_UI_TOP_*`
- `VARIO_UI_NAV_*`
- `VARIO_UI_COMPASS_*`
- `VARIO_UI_BOTTOM_*`
- `VARIO_UI_SIDE_BAR_W`
- `VARIO_UI_GAUGE_*`

폰트는 각 draw helper 내부 주석과 함께 직접 명시했다.

## 교체 권장 파일

- `Vario_Display.c`
- `Vario_Display.h`
- `Vario_Display_Common.c`
- `Vario_Display_Common.h`
- `Vario_Screen1.c`
- `Vario_Screen1.h`
- `Vario_Screen2.c`
- `Vario_Screen2.h`
- `Vario_Screen3.c`
- `Vario_Screen3.h`
