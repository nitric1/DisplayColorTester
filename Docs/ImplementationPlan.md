# DisplayColorTester 구현 계획

## 1. 문서 목적

이 문서는 DisplayColorTester를 단계적으로 구현하기 위한 계획을 정의한다.

첫 번째 구현 목표는 Win32 GDI의 `RGB(r, g, b)`와 `COLORREF`만 사용하는 sRGB 테스트 버전이다. Display-P3, Adobe RGB, BT.2020의 실제 색역 변환과 ICC/Advanced Color 처리는 첫 버전의 범위에서 제외하고 후속 단계에서 구현한다.

## 2. 최종 목표

최종 프로그램은 다음 동작을 제공한다.

- 시작 시 테스트 패턴과 색역 선택용 메인 창을 표시한다.
- 메인 창에는 `Color`, `Grayscale` 패턴 선택 항목과 `sRGB`, `Display-P3 (P3-D65)`, `Adobe RGB`, `BT.2020`, `Display Native RGB (Best effort)` 버튼을 표시한다.
- 색역 버튼을 누르면 활성화된 모든 모니터에 각각 하나의 borderless windowed-fullscreen 테스트 창을 표시한다.
- 모든 테스트 창은 같은 테스트 patch를 동시에 표시한다.
- Color 패턴의 순서는 `#F00`, `#0F0`, `#00F`, `#FF0`, `#F0F`, `#0FF`, `#FFF`, `#000`이다.
- Grayscale 패턴의 순서는 Gray 0%부터 Gray 100%까지 10% 간격의 11단계이다.
- 오른쪽 화살표 또는 마우스 왼쪽 버튼으로 다음 patch로 이동한다.
- 왼쪽 화살표 또는 마우스 오른쪽 버튼으로 이전 patch로 이동한다.
- patch가 바뀔 때 각 화면 중앙에 `sRGB - Red (#F00)` 또는 `sRGB - Gray 30%` 형식으로 현재 색역과 patch 이름을 1초간 표시한다.
- 테스트 창이 열린 직후와 마우스를 움직일 때만 커서를 표시하고, 마지막 움직임 1초 후에는 테스트 창 위의 커서를 숨긴다.
- `Esc`를 누르면 모든 테스트 창을 닫고 메인 창으로 돌아간다.

## 3. 단계 구분

### 3.1 1차 구현: sRGB GDI MVP

상태: 완료

1차 구현에서는 다음 범위만 완성한다.

- 네 색역 버튼을 모두 표시한다.
- `sRGB` 버튼만 활성화한다.
- 나머지 세 버튼은 비활성화하여 아직 실제 색역 출력이 지원되지 않는다는 점을 명확히 한다.
- GDI `RGB(r, g, b)` 매크로로 여덟 가지 sRGB 색상을 정의한다.
- 모니터별 ICC 프로필, WCS, Direct2D, Direct3D, DXGI 및 HDR 처리는 사용하지 않는다.
- 각 테스트 창의 `WM_PAINT`에서 단색 배경과 색상 이름을 GDI로 그린다.

이 단계의 목적은 창 수명, 다중 모니터 배치, 입력, 색상 순환 및 오버레이 동작을 먼저 안정화하는 것이다.

### 3.2 2차 구현: 색상 렌더링 계층 분리

상태: 완료

1차 구현의 동작을 유지하면서 GDI 세부 구현이 UI와 테스트 세션에 직접 노출되지 않도록 렌더링 인터페이스를 분리한다.

- 테스트 세션은 색역과 논리 색상 인덱스만 관리한다.
- 렌더러가 논리 색상을 실제 출력 색상으로 변환하도록 책임을 이동한다.
- 모니터별 렌더링 컨텍스트를 만들 수 있도록 자료 구조를 확장한다.
- 기존 sRGB GDI 렌더러를 기준 구현으로 유지하여 회귀 테스트에 사용한다.

### 3.3 3차 구현: Display-P3 지원

상태: 완료

- `Display-P3 (P3-D65)` 버튼을 활성화한다.
- Display-P3 원색 좌표, D65 백색점 및 필요한 전달 함수를 색역 정의에 추가한다.
- 일반 SDR과 Windows Advanced Color 환경에 맞는 렌더링 경로를 결정하고 구현한다.
- sRGB와 Display-P3 출력이 동일한 `RGB()` 호출로 처리되지 않도록 색 변환 계층을 사용한다.

구현된 출력 정책은 다음과 같다.

- Display-P3는 DCI-P3 원색, D65 백색점과 sRGB 전달 함수를 사용하는 색 공간으로 정의한다.
- Windows Advanced Color가 활성화된 모니터에는 Display-P3의 선형 RGB를 선형 scRGB로 변환하여 `DXGI_FORMAT_R16G16B16A16_FLOAT` flip-model swap chain으로 출력한다.
- HDR 모니터에서는 Windows의 현재 SDR white level을 적용하여 Display-P3 SDR 기준 흰색의 휘도를 보정한다.
- Advanced Color가 비활성화된 legacy SDR 모니터에는 Display-P3에서 현재 모니터 ICC 프로필로 WCS relative-colorimetric 변환한 device RGB 값을 `DXGI_FORMAT_B8G8R8A8_UNORM` swap chain으로 출력한다.
- 모니터 프로필을 얻거나 변환할 수 없는 legacy SDR 환경에서는 재현 가능한 sRGB gamut 경계로 clipping하는 fallback 값을 사용한다.
- Direct2D와 DirectWrite는 DirectX swap chain 위의 1초 오버레이만 담당하며, 테스트 배경색은 GDI `RGB()`를 통과하지 않는다.

### 3.4 4차 구현: Adobe RGB 및 BT.2020 지원

상태: 완료

- Adobe RGB와 BT.2020 색역 정의를 추가한다.
- 두 버튼을 활성화한다.
- 모니터별 ICC/WCS 변환 및 Advanced Color 경로를 완성한다.
- 출력 장치가 선택한 색역을 완전히 재현할 수 없는 경우의 clipping 또는 gamut mapping 정책을 문서화한다.
- SDR/HDR 혼합 모니터 구성에서 각 모니터가 올바른 출력 경로를 선택하는지 검증한다.

구현된 출력 정책은 다음과 같다.

- Adobe RGB는 `(0.6400, 0.3300)`, `(0.2100, 0.7100)`, `(0.1500, 0.0600)` 원색, D65 백색점과 `2.19921875` power 전달 함수를 사용한다.
- BT.2020은 `(0.7080, 0.2920)`, `(0.1700, 0.7970)`, `(0.1310, 0.0460)` 원색, D65 백색점과 BT.2020 전달 함수를 사용한다.
- 원색과 백색점에서 색역별 linear RGB→XYZ 행렬을 계산하고, 이를 linear XYZ→scRGB 행렬과 합성한다.
- Advanced Color 화면에는 합성 행렬 결과를 FP16 scRGB로 출력한다. 대상 모니터 gamut 밖의 값은 Windows display pipeline의 numeric clipping에 맡긴다.
- legacy SDR 화면에는 선택 색역에서 모니터 ICC 프로필로 WCS relative-colorimetric 변환한 device RGB 값을 출력한다.
- WCS `LOGCOLORSPACE`가 단일 power gamma만 표현하므로 BT.2020에는 `2.4` 근사를 사용한다. 현재 테스트 색상은 각 채널이 `0` 또는 `1`인 gamut 꼭짓점만 사용하므로 전달 함수 종류와 관계없이 선형 endpoint가 동일하다.
- 프로필을 사용할 수 없는 legacy SDR 화면에서는 해당 화면에서 재현 가능한 RGB gamut 경계로 clipping하는 fallback을 사용한다.

색역 정의 참고 자료:

- [Adobe RGB (1998) Color Image Encoding](https://www.adobe.com/digitalimag/pdfs/AdobeRGB1998.pdf)
- [Recommendation ITU-R BT.2020](https://www.itu.int/rec/R-REC-BT.2020)

### 3.5 5차 구현: Display Native RGB 한계 출력

상태: 완료

표준 색역 지원을 완료한 뒤 `Display Native RGB (Best effort)` 모드를 추가한다. 이 모드는 색 정확도 확인이 아니라 각 디스플레이가 보고한 native RGB gamut의 꼭짓점과 장치 RGB 최대 코드값을 확인하기 위한 진단 기능이다.

- 메인 창 마지막에 `Display Native RGB (Best effort)` 버튼을 추가한다.
- Red, Green, Blue에서는 대상 RGB 채널 하나를 최대로, 나머지 채널을 0으로 출력한다.
- Yellow, Magenta, Cyan, White에서는 해당 채널 조합을 최대로 출력하고 Black에서는 모든 채널을 0으로 출력한다.
- 논리 색상 순서와 입력 동작은 기존 테스트 모드와 동일하게 유지하되, 실제 출력값은 모니터별 native color 특성에 따라 다르게 계산할 수 있게 한다.
- Windows Advanced Color 활성 여부, 디스플레이의 보고된 원색 좌표와 프로필 신뢰도를 화면 또는 진단 정보에서 확인할 수 있게 한다.
- 디스플레이 정보가 누락되거나 모순되면 이 모드를 비활성화하거나 결과가 추정값임을 명확히 표시한다.
- 이 모드는 물리 subpixel의 직접 구동이나 측정 장비 수준의 최대 휘도를 보장하지 않는다고 UI와 문서에 명시한다.

구현된 출력 정책은 다음과 같다.

- Advanced Color 활성 모니터에서는 `IDXGIOutput6::GetDesc1`이 보고한 원색과 백색점으로 native linear RGB-to-scRGB 행렬을 계산한다.
- HDR 모니터의 전체화면 white 기준은 유효한 `MaxFullFrameLuminance / 80 nit`이며, 정보가 없으면 Windows의 SDR white level을 추정 기준으로 사용한다.
- Advanced Color SDR에서는 FP16 scRGB `1.0`을 디스플레이의 기준 white 최대값으로 사용한다.
- 보고된 원색 좌표가 유효하지 않으면 HDR은 BT.2020, SDR은 sRGB를 추정값으로 사용하고 오버레이에 이를 명시한다.
- legacy SDR에서는 ICC/WCS 변환을 적용하지 않고 8-bit full-range device RGB 끝값을 직접 출력한다.
- 각 모니터의 오버레이에 Advanced Color HDR/SDR 또는 legacy SDR 경로, bit depth, 보고된 원색/백색점 또는 추정 상태를 표시한다.

### 3.6 6차 구현: 테스트 패턴 모델과 sRGB Grayscale

상태: 구현 완료, 수동 출력 검증 대기

기존 Color 동작을 공통 test patch 모델로 일반화하고 sRGB에서 Grayscale 패턴을 먼저 완성한다.

- `TestPattern::Color`와 `TestPattern::Grayscale`을 추가한다.
- `TestColorId`에 직접 결합된 세션과 렌더러 인터페이스를 RGB 값과 표시 이름을 갖는 공통 test patch 모델로 일반화하고, 텍스트 명암을 patch 값에서 공통 계산한다.
- 기존 8개 Color patch와 신규 11개 Grayscale patch를 별도 고정 sequence로 관리한다.
- Grayscale 순서는 `Gray 0%`, `Gray 10%`, ..., `Gray 100%`로 고정하고 encoded RGB code level을 비율의 의미로 사용한다.
- `Gray 0%`는 `(0, 0, 0)`인 Black, `Gray 100%`는 `(1, 1, 1)`인 White로 정의한다.
- sRGB GDI의 8-bit code value는 반올림하여 `0`, `26`, `51`, `77`, `102`, `128`, `153`, `179`, `204`, `230`, `255`를 사용한다.
- 메인 창에 `Color`와 `Grayscale` 자동 radio button group을 추가하고 기본값은 `Color`로 한다.
- 패턴 선택 컨트롤을 Tab 탐색 순서에 포함하고 다섯 색역 버튼과 겹치지 않도록 기본 크기, 최소 크기와 DPI별 layout을 조정한다.
- Color 패턴에서는 기존 다섯 색역을 모두 사용할 수 있게 유지하고, 6차의 Grayscale 패턴에서는 sRGB만 활성화한다.
- `Application::StartTest`, `TestSession`과 렌더러 factory에 선택한 패턴을 전달하고 세션이 공통 patch index를 관리하게 한다.
- 테스트 창 제목에 선택 패턴을 포함하고 세션 종료 후 기존 색역 버튼과 패턴 선택 상태 및 포커스를 복구한다.
- 렌더러의 모니터별 사전 계산 배열과 index 처리에서 8개 고정 가정을 제거한다.
- sRGB Grayscale 오버레이는 `sRGB - Gray 30%` 형식으로 표시한다.
- 좌우 화살표, 좌우 마우스 버튼, 순환 이동, 오버레이, 커서 숨김과 `Esc` 종료 동작은 기존 Color 패턴과 동일하게 유지한다.

### 3.7 7차 구현: 표준 광색역 Grayscale 색상 관리

상태: 계획됨

Display-P3, Adobe RGB와 BT.2020에 중간 Grayscale 값을 정확히 처리하는 전달 함수 및 색상 관리 경로를 추가한다.

현재 Color 패턴은 모든 RGB 성분이 `0` 또는 `1`이므로 전달 함수의 양 끝점이 동일하지만, 중간 Grayscale 값에는 이 가정을 사용할 수 없다.

- Grayscale 선택 시 Display-P3, Adobe RGB와 BT.2020 버튼을 활성화한다.
- 이 단계까지 `Display Native RGB (Best effort)`의 Grayscale은 비활성화하여 정의되지 않은 native 중간값을 출력하지 않는다.
- encoded channel 값을 linear-light 값으로 변환하는 공통 함수를 추가한다.
- sRGB와 Display-P3에는 sRGB piecewise inverse transfer function을 적용한다.
- Adobe RGB에는 `2.19921875` power gamma를 적용한다.
- BT.2020에는 BT.2020 inverse OETF를 적용한다.
- Advanced Color에서는 선형화한 RGB를 기존 색역별 RGB-to-scRGB 행렬로 변환하고 SDR white scale을 적용한다.
- 모든 표준 색역이 D65 white를 사용하므로 equal-channel gray가 scRGB에서도 neutral gray가 되는지 수치 검증한다.
- legacy SDR에서는 encoded RGB를 먼저 선형화하고 WCS source `LOGCOLORSPACE`를 gamma `1.0`인 calibrated linear RGB로 구성한다.
- 선형화한 16-bit RGB를 현재 모니터 ICC 프로필로 변환하여 device RGB를 얻는다.
- 이 방식으로 단일 power gamma만 표현하는 `LOGCOLORSPACE`의 한계를 우회하고 sRGB와 BT.2020의 piecewise 전달 함수를 보존한다.
- 프로필을 읽거나 변환할 수 없으면 기존 fallback 경로를 사용하되 결과가 color-managed 출력이 아님을 기존 정책대로 취급한다.
- `0`과 `1` endpoint 및 기존 8개 Color patch의 출력 결과가 변경되지 않도록 회귀 검증한다.

### 3.8 8차 구현: Display Native RGB Grayscale와 전체 검증

상태: 계획됨

Display Native RGB에 Grayscale을 추가하고 다섯 색역의 두 패턴 조합을 최종 통합 검증한다.

- Grayscale 선택 시 `Display Native RGB (Best effort)` 버튼을 활성화한다.
- legacy SDR에서는 각 단계의 동일한 full-range device RGB code value를 세 채널에 직접 출력한다.
- Advanced Color에서는 DXGI가 native transfer curve를 제공하지 않으므로 보고된 원색과 백색점을 사용하는 equal-channel linear-light ramp로 출력한다.
- HDR에서는 기존 native full-frame white scale을 ramp의 100% 기준으로 사용한다.
- Native Advanced Color의 중간 단계는 물리 장치 code value를 직접 재현한다는 의미가 아니며 `Best effort` 진단 모드라는 한계를 유지한다.
- 원색 또는 휘도 정보가 유효하지 않으면 기존 BT.2020/sRGB 및 SDR white fallback 정책을 적용한다.
- Native 진단 오버레이와 `Gray N%`가 함께 읽기 쉽게 표시되도록 한다.
- 다섯 색역 × 두 패턴, Advanced Color/legacy SDR와 SDR/HDR 혼합 모니터 구성을 회귀 검증한다.
- README와 이 문서에 Grayscale 사용법, encoded code level의 의미와 Native linear-light 한계를 반영한다.

## 4. 1차 구현 상세 계획

### 4.1 프로젝트 기반 정리

1. `wWinMain`에서 애플리케이션 초기화와 표준 Win32 메시지 루프를 구현한다.
2. 메인 창과 테스트 창에 사용할 두 개의 window class를 등록한다.
3. Per-Monitor V2 DPI 인식을 애플리케이션 매니페스트에 선언한다.
4. Win32 API 실패를 사용자에게 알릴 공통 오류 처리 함수를 만든다.
5. 모든 생성 단계는 중간 실패 시 이미 생성된 HWND와 GDI 자원을 정리하도록 작성한다.

### 4.2 메인 창 구현

1. 크기 조절이 가능한 일반 overlapped 메인 창을 생성한다.
2. 다음 네 개의 자식 `BUTTON` 컨트롤을 만든다.
   - `sRGB`
   - `Display-P3 (P3-D65)`
   - `Adobe RGB`
   - `BT.2020`
3. 1차 구현에서는 `sRGB` 버튼만 enabled 상태로 둔다.
4. 창 크기와 DPI가 바뀌면 버튼 크기와 간격을 다시 계산한다.
5. `WM_COMMAND`에서 sRGB 버튼 클릭을 감지하여 테스트 세션을 시작한다.
6. 테스트 세션이 실행되는 동안 메인 창은 숨기고, 세션 종료 후 다시 표시하고 포커스를 복구한다.

### 4.3 sRGB 색상 모델 구현

색상 정보와 표시 이름을 하나의 고정 배열로 관리한다.

| 인덱스 | 표시 이름 | GDI 값 |
| ---: | --- | --- |
| 0 | `Red (#F00)` | `RGB(255, 0, 0)` |
| 1 | `Green (#0F0)` | `RGB(0, 255, 0)` |
| 2 | `Blue (#00F)` | `RGB(0, 0, 255)` |
| 3 | `Yellow (#FF0)` | `RGB(255, 255, 0)` |
| 4 | `Magenta (#F0F)` | `RGB(255, 0, 255)` |
| 5 | `Cyan (#0FF)` | `RGB(0, 255, 255)` |
| 6 | `White (#FFF)` | `RGB(255, 255, 255)` |
| 7 | `Black (#000)` | `RGB(0, 0, 0)` |

- 색상 인덱스는 테스트 세션에 하나만 존재한다.
- 다음 색상은 `(index + 1) % 8`로 계산한다.
- 이전 색상은 `(index + 7) % 8`로 계산하여 음수 나머지 문제를 피한다.
- 색상 테이블은 UI와 분리하여 순환 로직을 독립적으로 검증할 수 있게 한다.

### 4.4 모니터 열거와 테스트 창 생성

1. `EnumDisplayMonitors(nullptr, nullptr, ...)`로 현재 활성 모니터를 수집한다.
2. 각 `HMONITOR`에 대해 `GetMonitorInfoW`를 호출하여 `rcMonitor`와 장치 이름을 저장한다.
3. 가상 화면에서 음수 좌표를 가진 모니터도 좌표를 변경하지 않고 그대로 사용한다.
4. 모니터마다 독립적인 `WS_POPUP` 테스트 창을 하나씩 생성한다.
5. 창은 `rcMonitor` 전체를 덮도록 배치하고, taskbar를 포함해 화면 전체를 가리도록 topmost로 표시한다.
6. 독점 fullscreen 모드는 사용하지 않는다. 창 크기만 모니터 전체에 맞춘 windowed-fullscreen으로 구현한다.
7. 하나의 `TestSession`이 모든 테스트 HWND와 공통 색상 상태를 소유한다.
8. 창 생성 도중 하나라도 실패하면 이미 만든 테스트 창을 모두 닫고 메인 창으로 돌아간다.

### 4.5 GDI 렌더링

각 테스트 창의 `WM_PAINT`는 다음 순서로 처리한다.

1. `BeginPaint`로 paint DC를 얻는다.
2. 현재 `COLORREF`로 solid brush를 생성한다.
3. `FillRect`로 client area 전체를 채운다.
4. 오버레이가 활성화되어 있으면 현재 색상 이름을 중앙에 그린다.
5. 생성한 brush를 즉시 삭제하고 `EndPaint`를 호출한다.

추가 정책은 다음과 같다.

- `WM_ERASEBKGND`는 처리 완료로 반환하여 배경 지우기와 다시 칠하기 사이의 깜빡임을 줄인다.
- GDI brush나 font를 매 paint마다 누수하지 않도록 수명을 명확히 관리한다.
- 배경 밝기를 기준으로 `#FFF`와 밝은 색에는 검은 텍스트, `#000`과 어두운 색에는 흰 텍스트를 사용한다.
- 텍스트 가독성을 위해 반대색 그림자를 한두 픽셀 오프셋으로 먼저 그린다.

### 4.6 입력과 전체 창 동기화

각 테스트 창은 다음 메시지를 동일한 세션 명령으로 전달한다.

| 입력 | Win32 메시지/키 | 동작 |
| --- | --- | --- |
| 키보드 오른쪽 | `WM_KEYDOWN`, `VK_RIGHT` | 다음 색상 |
| 키보드 왼쪽 | `WM_KEYDOWN`, `VK_LEFT` | 이전 색상 |
| 마우스 왼쪽 | `WM_LBUTTONDOWN` | 다음 색상 |
| 마우스 오른쪽 | `WM_RBUTTONDOWN` | 이전 색상 |
| 키보드 Esc | `WM_KEYDOWN`, `VK_ESCAPE` | 테스트 종료 |

색상 변경 명령은 다음 순서로 수행한다.

1. 세션의 공통 색상 인덱스를 한 번 변경한다.
2. 오버레이 표시 상태와 만료 시간을 갱신한다.
3. 모든 테스트 창에 `InvalidateRect`를 호출한다.
4. 필요한 경우 `UpdateWindow`를 호출하여 같은 메시지 처리 구간에서 연속으로 다시 그린다.

여러 모니터의 주사율이 다를 수 있으므로 물리적인 동일 프레임 출력은 보장하지 않는다. 1차 구현에서는 하나의 UI 스레드에서 모든 창을 연속 갱신하는 수준을 동시 전환의 기준으로 삼는다.

### 4.7 1초 색상 이름/코드 오버레이

1. 테스트 세션 시작 시 첫 색상인 `sRGB - Red (#F00)` 표시 텍스트도 1초간 보여준다.
2. 색상이 변경될 때마다 오버레이를 다시 활성화한다.
3. 세션의 대표 테스트 창 하나에 고정된 timer ID로 1초 타이머를 설정한다.
4. 1초 안에 색상이 다시 바뀌면 기존 타이머를 취소하고 새로 시작한다.
5. `WM_TIMER`가 오면 타이머를 해제하고 오버레이 상태를 끈 뒤 모든 테스트 창을 다시 그린다.
6. 테스트 세션 종료 시 남은 타이머를 반드시 해제한다.

### 4.8 마우스 커서 자동 숨김

커서 숨김은 테스트 창에만 적용하며 메인 창과 다른 애플리케이션의 커서 상태에는 영향을 주지 않도록 구현한다.

1. 테스트 창을 처음 표시할 때 커서 표시 상태를 활성화하고 1초 커서 타이머를 시작한다.
2. 어느 테스트 창이든 `WM_MOUSEMOVE`를 받으면 커서를 즉시 표시하고 커서 타이머를 1초로 다시 시작한다.
3. 커서 타이머가 만료되면 커서 표시 상태를 비활성화한다.
4. 테스트 창의 `WM_SETCURSOR`에서 마우스가 client area 위에 있을 때 현재 상태에 따라 화살표 커서 또는 `nullptr`를 설정한다.
5. 타이머가 만료되는 순간 포인터가 테스트 창 위에 있으면 `SetCursor(nullptr)`를 호출하여 추가 마우스 이동을 기다리지 않고 즉시 숨긴다.
6. 색상 이름 오버레이 타이머와 커서 타이머는 서로 다른 timer ID와 상태를 사용한다.
7. `ShowCursor`는 프로세스/스레드 표시 카운터 불균형으로 커서가 복구되지 않을 수 있으므로 사용하지 않는다.
8. 테스트 세션을 종료할 때 커서 타이머를 해제하고 표준 화살표 커서를 복구한다.

키보드 입력과 색상 변경만으로는 커서 타이머를 다시 시작하지 않는다. 요구사항대로 테스트 창 생성과 실제 마우스 이동만 커서를 표시하는 기준으로 사용한다.

### 4.9 종료와 디스플레이 구성 변경

- 어느 테스트 창이 `Esc`를 받더라도 세션 전체를 종료한다.
- 세션 종료 시 오버레이/커서 타이머, font, 테스트 HWND 및 모니터 목록을 정리하고 표준 커서를 복구한다.
- 마지막 테스트 창이 닫힌 뒤 메인 창을 다시 표시한다.
- 메인 창이 종료되면 실행 중인 테스트 세션도 함께 종료한다.
- 테스트 도중 `WM_DISPLAYCHANGE`가 발생하면 1차 구현에서는 세션을 안전하게 종료하고 메인 창에서 다시 시작하도록 안내한다.
- 테스트 창 하나의 `WM_CLOSE`도 전체 세션 종료로 해석하여 일부 모니터에만 창이 남는 상태를 방지한다.

## 5. 예상 파일 구성

1차 구현에서는 과도한 분할을 피하면서 창과 상태의 책임을 분리한다.

| 파일 | 책임 |
| --- | --- |
| `Main.cpp` | `wWinMain`, 초기화, 메시지 루프 |
| `Application.h/.cpp` | 애플리케이션과 메인 창 수명 관리 |
| `TestSession.h/.cpp` | 모니터 열거, 테스트 창 집합, 색상 상태, 입력 명령 |
| `TestColors.h` | 색역·논리 색상 ID, 표시 순서와 색상 이름/코드 테이블 |
| `TestRenderer.h/.cpp` | 색역별 렌더러 인터페이스와 생성 |
| `GdiSrgbRenderer.h/.cpp` | 기존 GDI sRGB 출력 |
| `ColorManagedRenderer.h/.cpp` | Display-P3, Adobe RGB 및 BT.2020의 모니터별 Advanced Color 또는 ICC/WCS 출력 |
| `Resource.h` | 버튼, 아이콘 및 리소스 ID |
| `DisplayColorTester.rc` | 아이콘과 매니페스트 리소스 |
| `app.manifest` | Per-Monitor V2 DPI 및 Windows 호환성 선언 |

구현 과정에서 파일을 추가하면 `.vcxproj`와 `.vcxproj.filters`에도 동일하게 등록한다.

## 6. 1차 구현 검증 항목

### 6.1 빌드 검증

- x64 플랫폼의 Debug와 Release 구성을 모두 빌드한다.
- 경고 없이 빌드되는 것을 목표로 한다.
- GUI subsystem 실행 파일이 별도 콘솔 창을 만들지 않는지 확인한다.

### 6.2 기능 검증

- 프로그램 시작 시 메인 창과 네 버튼이 표시되는지 확인한다.
- sRGB만 클릭할 수 있고 나머지 버튼은 비활성화되어 있는지 확인한다.
- 활성 모니터마다 테스트 창이 정확히 하나씩 만들어지는지 확인한다.
- 화면 좌측에 배치되어 음수 좌표를 갖는 모니터도 완전히 덮는지 확인한다.
- 여덟 색상이 정의된 순서대로 표시되는지 확인한다.
- 이전/다음 이동이 배열 양 끝에서 정상적으로 순환하는지 확인한다.
- 키보드와 마우스 입력 방향이 요구사항과 일치하는지 확인한다.
- 모든 모니터가 같은 색상 인덱스를 유지하는지 확인한다.
- 오버레이가 중앙에 표시되고 마지막 전환 약 1초 후 사라지는지 확인한다.
- 테스트 창이 열린 직후 커서가 표시되고 약 1초 후 숨겨지는지 확인한다.
- 숨겨진 커서가 마우스 이동 즉시 다시 나타나며, 마지막 이동 약 1초 후 다시 숨겨지는지 확인한다.
- 테스트 종료 후 메인 창과 다른 애플리케이션에서 커서가 정상적으로 표시되는지 확인한다.
- `Esc` 후 모든 테스트 창이 사라지고 메인 창이 다시 나타나는지 확인한다.

### 6.3 안정성 검증

- 테스트 모드를 여러 번 반복 실행해 HWND, brush, font 및 timer 누수가 없는지 확인한다.
- 빠른 키 반복과 빠른 마우스 클릭에서 색상 인덱스가 손상되지 않는지 확인한다.
- 테스트 중 모니터 연결 상태가 바뀌어도 orphan fullscreen 창이 남지 않는지 확인한다.
- 메인 창 종료, 테스트 창 강제 닫기 및 부분 생성 실패 경로를 확인한다.

## 7. 1차 구현 완료 조건

다음 조건을 모두 만족하면 sRGB GDI MVP를 완료한 것으로 본다.

- 네 버튼이 있는 메인 창이 구현되어 있다.
- sRGB 버튼으로 모든 활성 모니터에 windowed-fullscreen 테스트 창을 생성할 수 있다.
- `RGB(r, g, b)`로 정의한 여덟 색상이 모든 화면에서 동일한 순서로 전환된다.
- 요구된 키보드와 마우스 입력이 모두 동작한다.
- `sRGB - Red (#F00)` 형식의 현재 색역, 색상 이름과 코드가 중앙에 1초간 표시된다.
- 테스트 창의 마우스 커서가 생성/이동 직후 표시되고 1초 후 자동으로 숨겨지며, 세션 종료 후 정상 복구된다.
- `Esc`가 전체 테스트 세션을 종료한다.
- x64 Debug와 x64 Release 구성이 모두 성공한다.
- Display-P3, Adobe RGB, BT.2020 버튼은 비활성 상태이며 색역 지원을 가장하지 않는다.

## 8. 후속 색역 구현 시 주의점

`RGB(r, g, b)`는 `COLORREF` 장치 값만 만들기 때문에 Display-P3, Adobe RGB 또는 BT.2020의 색도 좌표를 표현하는 수단으로 사용해서는 안 된다.

후속 구현에서는 다음 사항을 별도 설계하고 검증한다.

- 선택한 색역의 원색 좌표와 D65 백색점
- 색역별 전달 함수와 선형화
- 일반 SDR 모니터의 ICC/WCS 변환
- Windows Advanced Color 및 HDR 출력
- 모니터별 색상 프로필 차이
- 출력 색역을 벗어난 색상의 처리 정책
- 고정밀 swap chain과 색상 관리 중복 적용 방지

따라서 1차 구현의 GDI 렌더러는 최종 색상 관리 구현이 아니라 UI와 다중 모니터 동작을 검증하기 위한 명시적인 MVP로 유지한다.

## 9. Display Native RGB 구현 가능 범위와 한계

### 9.1 기능 정의

이 기능에서 `Native RGB`는 물리 subpixel에 직접 접근한다는 뜻이 아니라, 각 디스플레이의 장치 RGB 공간에서 다음 꼭짓점 값을 출력한다는 뜻으로 정의한다.

| 색상 | 장치 RGB 목표값 |
| --- | --- |
| Red | `(1, 0, 0)` |
| Green | `(0, 1, 0)` |
| Blue | `(0, 0, 1)` |
| Yellow | `(1, 1, 0)` |
| Magenta | `(1, 0, 1)` |
| Cyan | `(0, 1, 1)` |
| White | `(1, 1, 1)` |
| Black | `(0, 0, 0)` |

여기서 `1`은 해당 출력 경로에서 허용하는 최대 채널 코드값이다. 이는 패널의 실제 발광 소자 전류나 휘도 측정값과 동일한 개념이 아니다.

### 9.2 출력 경로

- Advanced Color가 활성화된 환경에서는 flip-model DXGI swap chain과 FP16 scRGB 출력을 사용한다.
- `IDXGIOutput6::GetDesc1`의 원색 좌표, 백색점, 색 공간 및 휘도 정보를 모니터별로 수집한다.
- 보고된 native primary를 scRGB 좌표로 변환하여 출력하고, Windows의 디스플레이 변환과 clipping을 거쳐 대상 디스플레이의 gamut 경계에 도달하도록 한다.
- HDR 전체화면 출력은 `MaxFullFrameLuminance`를 80 nit scRGB 기준으로 환산해 native RGB white와 각 채널 끝점의 기준 scale로 사용한다.
- Advanced Color SDR은 scRGB `(1, 1, 1)`이 디스플레이 최대 white로 해석되는 Windows 정책에 따라 scale `1.0`을 사용한다.
- Advanced Color가 비활성화된 legacy SDR 환경에서는 Windows가 앱 출력에 시스템 색 관리를 적용하지 않는 특성을 이용해 full-range device RGB 코드값을 출력한다.
- 서로 다른 모니터에는 같은 논리 색상을 표시하되, native gamut이 다르므로 실제 색도와 변환값은 모니터별로 달라질 수 있다.
- 원색 정보가 누락되거나 유효하지 않으면 HDR은 BT.2020, SDR은 sRGB 추정값으로 계속 출력하고 해당 모니터 오버레이에 추정임을 표시한다.

### 9.3 보장할 수 없는 사항

일반 Win32/DXGI 애플리케이션에는 패널의 물리 subpixel을 개별적으로 직접 구동하는 범용 API가 없다. 다음 요소는 애플리케이션 출력 이후에도 결과를 변경할 수 있다.

- GPU/Windows 보정 LUT와 색상 변환
- 모니터 내부의 gamut mode, white balance, gamma 및 동적 명암 처리
- HDR tone mapping, 자동 밝기 제한과 local dimming
- RGBW, PenTile, quantum-dot conversion 등 RGB stripe가 아닌 패널 구조
- EDID 또는 ICC/MHC 프로필의 누락이나 부정확한 원색 좌표

따라서 이 기능은 `Display Native RGB (Best effort)`로 명명한다. “각 채널에 가능한 최대 디지털 입력을 제공하고 native gamut 경계를 목표로 한다”는 것은 구현할 수 있지만, “각 물리 subpixel이 실제 최대 밝기로 발광한다”는 것은 소프트웨어만으로 검증하거나 보장할 수 없다. 정확한 확인에는 색도계 또는 분광측색계 측정이 필요하다.

### 9.4 구현 정책과 후속 계측 과제

1. `DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2`를 우선 사용하고 구형 API를 fallback으로 사용해 Advanced Color HDR/SDR 활성 상태를 구분한다.
2. Advanced Color에서는 현재 출력 경로의 `DXGI_OUTPUT_DESC1` 정보를 우선하며, legacy SDR에서는 프로필을 우회한 device RGB 끝값을 사용한다.
3. native primary의 xy 좌표와 white point로 RGB-to-XYZ 행렬을 계산한 뒤 linear sRGB/scRGB로 변환한다.
4. HDR은 `MaxFullFrameLuminance / 80 nit`, Advanced Color SDR은 `1.0`, legacy SDR은 8-bit full-range code value를 사용해 밝기 정책을 분리한다.
5. OS 색상 관리 활성/비활성 상태별 실제 색도와 휘도는 계측 장비로 후속 비교한다.

참고 자료:

- [Use DirectX with Advanced Color on high/standard dynamic range displays](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)
- [ICC profile behavior with Advanced Color](https://learn.microsoft.com/en-us/windows/win32/wcs/advanced-color-icc-profiles)
- [DXGI_OUTPUT_DESC1 structure](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/ns-dxgi1_6-dxgi_output_desc1)

## 10. 3차 구현 검증 항목

### 10.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- 새 DirectX/WCS 소스가 프로젝트와 필터 파일에 등록되었는지 확인한다.
- Display-P3 렌더러가 GDI `RGB()` 또는 `COLORREF`를 사용하지 않는지 확인한다.
- C/C++ 표준 헤더와 Win32 정수 타입 사용이 프로젝트 지침을 따르는지 검사한다.

### 10.2 수동 검증

- sRGB 테스트가 2차 구현과 동일하게 동작하는지 회귀 확인한다.
- Display-P3 버튼이 활성화되고 모든 활성 모니터에 테스트 창이 생성되는지 확인한다.
- 키보드, 마우스, 오버레이, 커서 자동 숨김 및 `Esc` 종료가 Display-P3에서도 동일하게 동작하는지 확인한다.
- Advanced Color가 활성화된 화면에서 FP16 scRGB 경로로 출력되는지 확인한다.
- Advanced Color가 비활성화된 화면에서 해당 모니터의 ICC/WCS 변환 결과가 사용되는지 확인한다.
- SDR/HDR 혼합 구성에서 모니터별 출력 경로가 독립적으로 선택되는지 확인한다.
- 가능하면 Display-P3 지원 모니터와 계측 장비를 사용해 Red, Green, Blue 및 White의 색도와 휘도를 확인한다.

## 11. 4차 구현 검증 항목

### 11.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- Adobe RGB와 BT.2020 버튼이 범용 색상 관리 렌더러를 생성하는지 확인한다.
- Display-P3 전용 변환 상수가 남아 있지 않고 세 광색역이 원색과 백색점에서 행렬을 계산하는지 확인한다.
- 각 색역에서 D65 white가 scRGB `(1, 1, 1)`로 변환되는지 수치 검증한다.
- 범용 색상 관리 렌더러가 GDI `RGB()` 또는 `COLORREF`를 사용하지 않는지 확인한다.

### 11.2 수동 검증

- 네 색역 버튼이 모두 활성화되어 각각 테스트 세션을 시작하는지 확인한다.
- Adobe RGB와 BT.2020에서 여덟 색상, 입력, 오버레이, DPI, 커서 자동 숨김 및 `Esc` 종료가 동일하게 동작하는지 확인한다.
- 오버레이에 `Adobe RGB - Red (#F00)` 또는 `BT.2020 - Red (#F00)` 형식의 현재 색역이 표시되는지 확인한다.
- Advanced Color 활성/비활성 화면과 SDR/HDR 혼합 구성에서 모니터별 출력 경로를 확인한다.
- 가능하면 각 색역을 지원하는 모니터와 계측 장비로 원색과 white의 색도·휘도를 확인한다.

## 12. 5차 구현 검증 항목

### 12.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- 다섯 번째 버튼이 `DisplayNative` 렌더러를 생성하는지 확인한다.
- 보고된 원색 좌표 검증, native RGB-to-scRGB 행렬 계산 및 모니터별 상태 저장을 확인한다.
- Advanced Color native 출력이 FP16 scRGB swap chain을 사용하고 legacy SDR 출력이 ICC/WCS 변환을 우회하는지 확인한다.
- 표준 헤더, Win32 callback 표기 및 정수 타입 사용이 프로젝트 지침을 따르는지 검사한다.

### 12.2 수동 검증

- 다섯 개 버튼이 모두 활성화되고 `Display Native RGB (Best effort)`가 모든 활성 모니터에서 시작되는지 확인한다.
- 여덟 색상, 좌우 입력, 오버레이, DPI, 커서 자동 숨김 및 `Esc` 종료가 기존 모드와 동일하게 동작하는지 확인한다.
- 각 모니터 오버레이에 Advanced Color HDR/SDR 또는 legacy SDR 출력 경로가 올바르게 표시되는지 확인한다.
- Advanced Color에서는 보고된 원색/백색점과 bit depth가 표시되고, 유효하지 않은 정보에는 BT.2020 또는 sRGB 추정 상태가 표시되는지 확인한다.
- legacy SDR에서는 full-range device RGB와 ICC bypass 상태가 표시되는지 확인한다.
- 가능하면 계측 장비로 각 primary와 white의 색도·전체화면 휘도를 측정하고, 모니터 내부 처리에 따른 차이를 기록한다.

## 13. 6차 구현 검증 항목

### 13.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- Color 8개와 Grayscale 11개 sequence의 크기, 순서, 표시 이름과 endpoint를 검사한다.
- sRGB GDI 10% 단계가 `0`, `26`, `51`, `77`, `102`, `128`, `153`, `179`, `204`, `230`, `255`로 변환되는지 검사한다.
- 공통 patch index가 선택 sequence의 양 끝에서 정상적으로 순환하는지 검사한다.
- 기존 Color patch의 출력값과 입력 동작이 변경되지 않았는지 회귀 검사한다.
- 표준 헤더, Win32 callback 표기와 정수 타입 사용이 프로젝트 지침을 따르는지 검사한다.

### 13.2 수동 검증

- 메인 창에서 `Color`와 `Grayscale`을 마우스 및 키보드로 선택할 수 있는지 확인한다.
- Color 선택 시 기존 다섯 색역 버튼이 모두 활성화되는지 확인한다.
- Grayscale 선택 시 sRGB만 활성화되고 나머지 색역 버튼은 비활성화되는지 확인한다.
- sRGB Grayscale이 Gray 0%부터 Gray 100%까지 표시되고 양 끝에서 정상 순환하는지 확인한다.
- 오버레이에 `sRGB - Gray N%`가 올바르게 표시되는지 확인한다.
- 키보드, 마우스, DPI, 커서 자동 숨김, 다중 모니터 동기화와 `Esc` 종료가 기존 Color 패턴과 동일하게 동작하는지 확인한다.

## 14. 7차 구현 검증 항목

### 14.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- sRGB, power gamma와 BT.2020 inverse transfer function의 endpoint, 대표 중간값과 단조 증가를 수치 검증한다.
- Display-P3, Adobe RGB와 BT.2020의 equal-channel gray가 D65 neutral scRGB로 변환되는지 허용 오차 내에서 검사한다.
- legacy SDR WCS source가 gamma `1.0`인 linear RGB이며 입력값이 사전에 선형화되는지 확인한다.
- `0`과 `1` endpoint 및 기존 Color patch의 Advanced Color와 legacy SDR 출력값이 변경되지 않았는지 회귀 검사한다.

### 14.2 수동 검증

- Grayscale 선택 시 sRGB, Display-P3, Adobe RGB와 BT.2020 버튼이 활성화되는지 확인한다.
- 네 표준 색역 각각에서 11개 Grayscale 단계, 입력, 오버레이와 종료 동작을 확인한다.
- Advanced Color 활성/비활성 화면과 SDR/HDR 혼합 구성에서 모니터별 출력 경로를 확인한다.
- Gray 단계가 육안으로 단조 증가하고 의도하지 않은 색조가 나타나지 않는지 확인한다.
- 기존 Color 패턴의 네 표준 색역 출력이 이전 버전과 동일한지 회귀 확인한다.
- 가능하면 계측 장비로 grayscale luminance가 단조 증가하는지, 예상 전달 함수와 일치하는지 비교한다.

## 15. 8차 구현 검증 항목

### 15.1 자동 검증

- x64 Debug와 x64 Release 구성을 모두 빌드한다.
- Native legacy SDR의 11개 단계가 equal-channel full-range device RGB code로 계산되는지 검사한다.
- Native Advanced Color가 reported primaries, white point와 full-frame white scale을 사용하는 linear-light ramp인지 검사한다.
- 원색 및 휘도 정보가 유효하지 않은 경우 기존 fallback이 적용되는지 확인한다.
- 다섯 색역 × 두 패턴의 renderer factory 경로가 모두 생성되는지 확인한다.

### 15.2 수동 검증

- Grayscale 선택 시 다섯 색역 버튼이 모두 활성화되는지 확인한다.
- Display Native RGB에서 11개 Gray 단계와 native 진단 오버레이가 함께 올바르게 표시되는지 확인한다.
- Native Advanced Color Grayscale이 best-effort linear-light ramp임을 UI와 문서에서 확인한다.
- 다섯 색역 × 두 패턴에서 키보드, 마우스, DPI, 커서 자동 숨김, 다중 모니터 동기화와 `Esc` 종료를 회귀 확인한다.
- Advanced Color/legacy SDR 및 SDR/HDR 혼합 구성에서 모니터별 출력 경로를 확인한다.
- 가능하면 계측 장비로 Native grayscale luminance의 단조 증가와 전체화면 white 휘도를 비교한다.
