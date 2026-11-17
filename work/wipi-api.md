# 크로노스 윙 WIPI API 목록

> 이 저장소에는 게임 원본과 그 파생물이 없다. 이 문서에 나오는 원본 파일, `native/build`, `work/bin`·`work/decomp`·`work/ghidra`·`work/shots`·`work/saves`는 원본을 가진 사람의 작업 폴더에만 있다. 적힌 주소와 수치는 분석 기록이다. → [docs/provenance.md](../docs/provenance.md)

재현: `work/tools/apitrace.sh`(실행 기록) → `apiscan.py`(정적 호출 지점) → `apijoin.py`(표 작성). 이름은 WIE 소스(MIT) 기준이며, `?`는 WIE에 이름이 없는 슬롯이다.

- 정적: 표 호출 1,211곳 중 1,101곳의 슬롯을 해석했다. 110곳은 해석하지 못했다(가상 함수와 콜백 포인터가 섞여 있음).
- 실행: 타이틀, 서버 끊김 메시지, 스토리 대화까지 약 100초 기록했다. 호출지 209곳 중 205곳이 정적 슬롯과 일치했고, 전역-표 대응이 어긋난 곳은 없다. 나머지 4곳은 위 110곳에 들어간다.
- 전투, 저장, 불러오기는 실행 기록에 없다. 해당 구간 API는 정적 결과에만 의존한다.
- `0x110900`, `0x110940` 전역(197곳)은 `0x85000` 이후 Java 런타임 층만 쓰는 JB 인터페이스 쪽 슬롯이다(이름 미대조). 네이티브 호스트에서 Java 층을 대체할지는 5번에서 정한다.

| 표 | 번호 | 이름 | 정적 호출지 | 실행 호출 수 | WIE 상태 |
|---|---:|---|---:|---:|---|
| Kernel | 0 | Printk | 138 | 15 |  |
| Kernel | 1 | Sprintk | 311 | 349 |  |
| Kernel | 7 | Exit | 3 | 0 |  |
| Kernel | 9 | GetCurProgramId | 1 | 0 |  |
| Kernel | 20 | Alloc | 3 | 443 |  |
| Kernel | 21 | Calloc | 132 | 3441 |  |
| Kernel | 22 | Free | 133 | 2041 |  |
| Kernel | 23 | GetTotalMemory | 1 | 1 |  |
| Kernel | 24 | GetFreeMemory | 13 | 253 |  |
| Kernel | 25 | DefTimer | 1 | 1 |  |
| Kernel | 26 | SetTimer | 2 | 1430 |  |
| Kernel | 27 | UnsetTimer | 1 | 0 |  |
| Kernel | 28 | CurrentTime | 19 | 2867 |  |
| Kernel | 29 | GetSystemProperty | 2 | 1 |  |
| Kernel | 31 | GetResourceId | 2 | 388 |  |
| Kernel | 32 | GetResource | 0 | 304 |  |
| Kernel | 33 | Reserved1 | 0 | 1 |  |
| Util | 1 | htons | 1 | 0 |  |
| Misc | 0 | back_light | 1 | 1 |  |
| Graphics | 2 | GetScreenFramebuffer | 1 | 1 |  |
| Graphics | 3 | DestroyOffscreenFramebuffer | 3 | 4 |  |
| Graphics | 4 | CreateOffscreenFramebuffer | 4 | 7 |  |
| Graphics | 5 | InitContext | 1 | 1 |  |
| Graphics | 6 | SetContext | 12 | 30295 |  |
| Graphics | 9 | DrawLine | 3 | 0 |  |
| Graphics | 10 | DrawRect | 2 | 10976 |  |
| Graphics | 11 | FillRect | 2 | 13618 |  |
| Graphics | 12 | CopyFrameBuffer | 6 | 670 |  |
| Graphics | 14 | CopyArea | 8 | 14 |  |
| Graphics | 15 | DrawArc | 2 | 0 |  |
| Graphics | 16 | FillArc | 3 | 695 |  |
| Graphics | 17 | DrawString | 2 | 0 |  |
| Graphics | 21 | FlushLcd | 1 | 1429 |  |
| Graphics | 22 | GetPixelFromRgb | 25 | 32781 |  |
| Graphics | 23 | GetRgbFromPixel | 8 | 0 |  |
| Graphics | 24 | GetDisplayInfo | 1 | 0 |  |
| Graphics | 26 | GetFont | 1 | 1 |  |
| Graphics | 30 | GetStringWidth | 2 | 0 |  |
| Im | 0 | handle_input | 6 | 0 |  |
| Im | 1 | set_current_mode | 1 | 0 |  |
| Im | 2 | get_current_mode | 1 | 0 |  |
| If5 | 18 | ? | 1 | 0 | stub |
| If5 | 19 | ? | 1 | 0 | stub |
| Fs | 0 | Open | 2 | 13 |  |
| Fs | 1 | Read | 1 | 0 |  |
| Fs | 2 | Write | 1 | 2 |  |
| Fs | 3 | Close | 4 | 13 |  |
| Fs | 6 | Remove | 7 | 0 |  |
| Uic | 0 | create_application_context | 1 | 1 |  |
| Uic | 1 | get_class | 1 | 0 |  |
| Uic | 2 | create | 1 | 0 |  |
| Uic | 3 | destroy | 1 | 0 |  |
| Uic | 9 | configure | 1 | 0 |  |
| Uic | 10 | MC_uicGetGeometry | 2 | 0 | stub |
| Uic | 11 | set_enable | 1 | 0 |  |
| Media | 0 | clip_create | 2 | 2 |  |
| Media | 3 | clip_free | 1 | 1 |  |
| Media | 4 | clip_put_data | 2 | 2 |  |
| Media | 7 | unk7 | 1 | 1 |  |
| Media | 8 | play | 2 | 2 |  |
| Media | 11 | stop | 1 | 1 |  |
| Media | 16 | vibrator | 2 | 0 |  |
| Media | 17 | unk17 | 1 | 1 |  |
| Media | 26 | clip_set_volume | 1 | 4 |  |
| Net | 0 | connect | 1 | 7 |  |
| Net | 1 | close | 1 | 7 |  |
| Net | 2 | socket | 1 | 0 |  |
| Net | 13 | set_read_callback | 1 | 0 |  |
| Net | 14 | set_write_callback | 1 | 0 |  |
| Net | 30 | ? | 1 | 0 |  |
| Net | 31 | ? | 1 | 0 |  |
