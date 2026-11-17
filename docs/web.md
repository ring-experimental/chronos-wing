# 웹 빌드

브라우저에서 도는 판이다. 페이지에는 실행 환경만 있고, 게임은 사용자가 가진 원본 파일을 브라우저가 직접 읽어 실행한다.

> 배포되는 파일 어디에도 원본과 그 파생물이 없다. → [provenance.md](provenance.md)

## 빌드

```bash
brew install emscripten ninja rustup    # rustup target add wasm32-unknown-unknown
cd native
make web                                # 처음에는 deps.sh web이 SDL3, FreeType, RmlUi를 받아 빌드한다
python3 -m http.server -d build/web 8765
```

`native/build/web`이 결과다. 이 폴더를 그대로 정적 호스팅에 올리면 된다.

| 파일 | 내용 |
|---|---|
| `index.html`, `app.js` | 페이지와 진행(저장소 연결, 가져오기, 시작) |
| `gamedata.js` | 원본 풀기, 글꼴과 소리 변환 |
| `chronos.js`, `chronos.wasm` | 호스트 + 인터프리터 |
| `chronos.data` | 화면 패드 UI와 사운드폰트 |
| `smaf2ev.wasm` | SMAF 변환기(`work/tools/smaf2ev`) |
| `sw.js`, `manifest.webmanifest`, `icon*` | PWA |

## 처음 열 때

```mermaid
sequenceDiagram
    actor U as 사용자
    participant P as 페이지 (app.js)
    participant G as gamedata.js
    participant S as smaf2ev.wasm
    participant DB as IndexedDB (/persist)
    participant W as chronos.wasm

    U->>P: 페이지 열기
    P->>DB: 저장소 불러오기
    DB-->>P: 게임 데이터 없음
    P-->>U: "원본 파일 고르기"
    U->>P: zip 또는 jar 선택
    P->>G: extractGame(bytes)
    G->>G: zip 풀기 → 안의 jar 풀기
    G->>G: client.bin SHA-256 확인
    G-->>P: 파일 2,497개
    P->>G: buildEvents()
    G->>S: SMAF마다 변환
    S-->>G: game.ev
    P->>G: buildFont()
    G-->>P: ChronosGame.ttf
    P->>DB: game/, game.ev, ChronosGame.ttf 저장
    P-->>U: "게임 시작"
    U->>P: 시작
    P->>W: callMain(/persist/game …)
    W->>DB: 2초마다, 탭을 떠날 때 세이브 내려 쓰기
```

두 번째부터는 저장된 데이터를 바로 쓰므로 `게임 시작`만 누르면 된다.

## 가져오기가 만드는 것

빌드 때 파이썬과 Rust 도구로 만들던 파생물을 브라우저에서 같은 규칙으로 만든다.

```mermaid
flowchart LR
    zip["원본 zip"] --> jar["010100D2.jar"]
    jar --> files["게임 데이터 폴더"]
    jar --> bin["client.bin<br/>SHA-256으로 판본 확인"]
    files --> ft["ft/ 비트맵 글꼴"]
    files --> snd["snd/ SMAF"]
    ft -->|"gamedata.js buildFont<br/>= hnf2ttf.py"| ttf["ChronosGame.ttf"]
    snd -->|"smaf2ev.wasm<br/>= smaf2ev 도구"| ev["game.ev"]
```

네이티브 빌드의 결과와 맞대어 확인했다.

| 항목 | 대조 결과 |
|---|---|
| 푼 파일 | 2,497개 모두 같음 |
| `game.ev` | 바이트 단위로 같음 |
| 글꼴 | 글리프 2,448자의 윤곽, 폭, 문자 대응이 같음 |

지원하는 원본은 KTF 판 하나다. 다른 통신사 판은 실행 이미지가 달라 `restored/`의 주소가 맞지 않으므로 거부한다.

## 저장소 배치

Emscripten의 가상 파일 시스템 위에서 돈다.

| 경로 | 저장 | 내용 |
|---|---|---|
| `/ui`, `/1mgm.sf2` | 배포 꾸러미(`chronos.data`) | 화면 패드, 사운드폰트 |
| `/persist/game/` | IndexedDB | 원본에서 푼 게임 데이터 |
| `/persist/game.ev`, `/persist/ChronosGame.ttf` | IndexedDB | 가져올 때 만든 파생물 |
| `/persist/save/` | IndexedDB | 세이브 |
| `/persist/ui.ini` | IndexedDB | 패드 배치, 키 설정 |
| `/tmp/` | 메모리 | 세이브 내보내기·들여오기의 임시 파일 |

브라우저 데이터를 지우면 `/persist`가 통째로 사라진다. 세이브는 타이틀의 `캐릭터 관리` → `캐릭터 복구`에서 파일(`.cwsave`)로 내보낼 수 있다. 웹에서는 내보내기가 내려받기로, 들여오기가 파일 고르기 화면으로 바뀐다(`host/savefile.c`).

## 네이티브와 다른 점

| 항목 | 네이티브 | 웹 |
|---|---|---|
| 게임 코드 | 번역 C | 인터프리터 |
| 메인 루프 | 다음 타이머까지 잔다 | 화면 주기마다 한 번(`web_tick`) |
| 메모리 보호 | 보호 페이지 | 접근마다 범위 검사 |
| 파일 창 | OS 파일 창 | 페이지의 고르기 화면, 내려받기 |
| 게임 화면 폭 | 모니터 비율(240~640) | 시작할 때의 페이지 비율(240~640). 터치 기기는 세로로 든 채 시작해도 가로 비율로 잡고, 세로인 동안 돌리라는 안내를 띄운다 |
| 종료 | 프로세스 종료 | 저장 후 "다시 시작" 화면 |

## 오프라인과 갱신

서비스 워커(`sw.js`)는 실행 환경 파일만 캐시한다. 게임 데이터와 세이브는 IndexedDB에 있어 서비스 워커가 다루지 않는다.

```mermaid
stateDiagram-v2
    state "첫 방문" as first
    state "캐시됨" as cached
    state "새 버전 대기" as waiting
    [*] --> first
    first --> cached: 파일 12개 캐시
    cached --> cached: 다시 열기 (캐시에서, 오프라인 가능)
    cached --> waiting: 새 빌드 배포 후 열기
    waiting --> cached: 한 번 더 열면 새 버전
```

캐시 이름에 빌드 해시가 들어가므로 새 빌드가 올라가면 다음에 열 때 새 캐시를 받고, 그다음 열 때부터 새 버전이 적용된다. 서비스 워커는 https나 `localhost`에서만 돈다.

## 배포

`main`에 푸시하면 GitHub Actions가 빌드해 Pages에 올린다(`.github/workflows/pages.yml`).

```mermaid
flowchart LR
    push["main에 푸시"] --> co["저장소만 받기"]
    co --> deps["deps.sh web<br/>SDL3, FreeType, RmlUi"]
    deps --> build["make web"]
    build --> art["build/web"]
    art --> pages["GitHub Pages"]
```

러너에는 원본이 없다. 원본이 있어야 빌드되는 것이 섞이면 이 단계가 실패한다.

## 시험용 주소 인자

| 인자 | 동작 |
|---|---|
| `?touch=1` | 터치 기기가 아니어도 화면 패드를 켠다 |
| `?width=<폭>` | 게임 화면 폭을 정한다(240~640) |

## 확인한 것과 못 한 것

맥의 크롬 계열 브라우저에서 가져오기, 타이틀, 필드, 화면 패드, 저장 유지, 서비스 워커 캐시를 확인했다. 폰 실기기와 iOS Safari, 한글 입력, 세이브 내보내기·들여오기 화면, 종료 흐름은 아직 확인하지 않았다.
