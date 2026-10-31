# 크로노스 윙 네이티브 빌드

WIE와 ARM 에뮬레이터 없이 도는 SDL3 빌드다. 게임 코드는 `client.bin`을 기계 번역한 C이고, 수정한 함수만 `restored/`의 읽는 C로 교체한다(목록은 `restored/list.txt`). WIPI API는 `host/`에서 직접 구현한다.

```bash
make -j8                                   # 저장소 루트의 크로노스윙.zip이 필요하다
build/chronos build/game                   # 전체화면(폭은 모니터 비율, 240~640). --windowed, --width <폭>, --save <폴더>, F11 전환
```

키: 방향키, Space/Enter=OK, Shift 좌/우=소프트키, Backspace=CLEAR, 123/QWE/ASD/ZXC=숫자 키패드.

- `build/`는 원본 파생물이라 저장소에 넣지 않는다. 원본과 빌드 묶음을 공개하지 않는다.
- `run_test.sh`, `stress.sh`: 제어 파일로 키를 넣는 자동 시험. 저장 폴더를 따로 쓴다.
- `CHRONOS_TRACE=<파일>`: API 호출을 WIE `wipic call` 로그와 같은 형식으로 남긴다. `work/tools/apidiff.py`로 비교한다.

## 출처

- WIPI API 동작: [WIE](https://github.com/dlunch/wie) (MIT). 소스를 참고해 C로 옮겼고 링크하지 않는다.
- SMAF 변환(빌드 때만): smaf-player (MIT).
- MIDI 합성: TinySoundFont (MIT), `third_party/tsf.h`.
- 사운드폰트: 1MGM (CC BY 3.0), `third_party/1mgm.txt`.
