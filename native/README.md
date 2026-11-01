# 크로노스 윙 네이티브 빌드

WIE와 ARM 에뮬레이터 없이 도는 SDL3 빌드다. 게임 코드는 `client.bin`을 기계 번역한 C이고, 수정한 함수만 `restored/`의 읽는 C로 교체한다(목록은 `restored/list.txt`). WIPI API는 `host/`에서 직접 구현한다.

```bash
make -j8                                   # 저장소 루트의 크로노스윙.zip이 필요하다
build/chronos build/game                   # 전체화면(폭은 모니터 비율, 240~640). --windowed, --width <폭>, --save <폴더>, F11 전환
```

키: 방향키, Space/Enter=OK, Shift 좌/우=소프트키, Backspace=CLEAR, 123/QWE/ASD/ZXC=숫자 키패드.

## Windows, Android

SDL3 패키지를 `build/deps`에 받아 둔다(`SDL3-devel-<버전>-mingw.tar.gz`를 `mingw/`, `SDL3-<버전>.tar.gz`를 `src/`에 한 단계 벗겨 푼다).

```bash
make windows   # build/win 폴더 통째로 옮겨 chronos.exe 실행. 실행 파일 옆 game 폴더를 쓴다
make android   # build/android/chronos.apk(arm64, 디버그 키). adb install로 설치
```

Android는 첫 실행 때 APK 안의 데이터를 푼다. 뒤로 키는 CLEAR다.

## UI

게임 위에 얹는 UI는 RmlUi(`host/ui.cpp`, 문서 `ui/`)다. 터치 패드는 Android 기본, 데스크톱은 `--touch` 또는 설정에서 켠다. F1이나 패드의 `설정`으로 크기, 투명도, 위치를 바꾸며 `ui.ini`에 저장된다. 표시 조건은 데이터 모델 `ui`의 `dialog`, `edit`, `pad_on`에 묶여 있고 대사 중에는 OK만 남는다. 의존성은 처음 빌드 때 `deps.sh`가 만든다.

- `build/`는 원본 파생물이라 저장소에 넣지 않는다. 원본과 빌드 묶음을 공개하지 않는다.
- `run_test.sh`, `stress.sh`: 제어 파일로 키를 넣는 자동 시험. 저장 폴더를 따로 쓴다.
- `CHRONOS_TRACE=<파일>`: API 호출을 WIE `wipic call` 로그와 같은 형식으로 남긴다. `work/tools/apidiff.py`로 비교한다.

## 세이브 옮기기

타이틀의 `캐릭터 관리` → `캐릭터 복구`에서 세이브를 파일 하나(`.cwsave`)로 내보내거나 들여온다. 서비스가 끝난 서버 복구를 대신한다. 들여오면 게임의 해시로 파일마다 검증하고, 기존 세이브는 저장 폴더의 `backup/`으로 옮긴 뒤 게임을 종료한다. `CERTIDATA`는 옮기지 않는다. `CHRONOS_SAVEFILE_TEST=<파일>`이면 파일 창 없이 그 경로를 쓴다.

## 출처

- WIPI API 동작: [WIE](https://github.com/dlunch/wie) (MIT). 소스를 참고해 C로 옮겼고 링크하지 않는다.
- SMAF 변환(빌드 때만): smaf-player (MIT).
- 플랫폼 계층: SDL3 (zlib).
- UI: RmlUi (MIT), FreeType (FTL). 글꼴은 빌드 때 게임 폰트(`ft/`)에서 만든다(`work/tools/hnf2ttf.py`).
- MIDI 합성: TinySoundFont (MIT), `third_party/tsf.h`.
- 사운드폰트: 1MGM (CC BY 3.0), `third_party/1mgm.txt`.
