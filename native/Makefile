# 크로노스 윙 네이티브 빌드.
# 이 저장소에는 게임 원본이 없다. 원본(크로노스윙.zip)은 가진 사람이 저장소 루트에 직접 두며 .gitignore가 막는다.
# - all, windows, android, mac: 그 zip에서 코드와 데이터를 꺼내 쓴다. build/의 결과물은 원본 파생물이라 올리지 않는다.
# - interp_gen, web: zip 없이 빌드된다. 웹 빌드는 실행할 때 사용자가 고른 원본 파일을 브라우저에서 읽는다.
# 이 기계에만 맞는 경로는 적지 않는다. 도구는 PATH에서 찾고, 다른 곳에 있으면 make 변수로 넘긴다(PY, CARGO, SDL_CFLAGS, SDL_LIBS).
# 의존성의 판은 두 곳에 적혀 있다: 파이썬 도구는 저장소 루트의 pyproject.toml과 uv.lock, C/C++ 라이브러리는 deps.sh.
ROOT := $(abspath ..)
# 파이썬 도구(capstone, fonttools)는 uv가 uv.lock대로 맞춘 환경에서 돌린다.
PY ?= uv run --project $(ROOT) python
CARGO ?= cargo
ZIP := $(ROOT)/크로노스윙.zip
B := build
GAME := $(B)/game
HOSTSRC := $(wildcard host/*.c)
HOSTOBJ := $(HOSTSRC:host/%.c=$(B)/%.o)
UISRC := $(wildcard host/*.cpp)
UIOBJ := $(UISRC:host/%.cpp=$(B)/%.o)
UIDEPS := host/ui.h host/ui_render.h
RML = $(B)/deps/$(1)/install
RMLLIBS := -lrmlui -lfreetype
RESTSRC := $(wildcard restored/*.c)
RESTOBJ := $(RESTSRC:restored/%.c=$(B)/r_%.o)
# 디버깅: make TRFLAGS=-DTRACE_PC 면 접근마다 범위를 검사하고 pc를 남긴다.
TRFLAGS ?=
# 맥 빌드의 SDL3는 설치된 것을 쓴다. pkg-config가 알면 그대로, 모르면 Homebrew 접두사에서 찾는다.
BREW := $(shell brew --prefix 2>/dev/null)
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null || echo $(if $(BREW),-I$(BREW)/include))
SDL_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo $(if $(BREW),-L$(BREW)/lib) -lSDL3)
SDL_CFLAGS := $(SDL_CFLAGS)
SDL_LIBS := $(SDL_LIBS)
CFLAGS := -O2 -g -w -std=gnu11 -Ihost $(SDL_CFLAGS)
LDFLAGS := $(SDL_LIBS)

.PHONY: all windows android mac interp web
all: $(B)/chronos $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2 $(B)/ui

$(GAME)/client.bin: $(ZIP)
	mkdir -p $(GAME)
	unzip -qo "$(ZIP)" 010100D2.jar -d $(B)
	unzip -qo $(B)/010100D2.jar -d $(GAME)
	cp $(GAME)/client.bin474328 $(GAME)/client.bin

$(B)/codemap.json: $(GAME)/client.bin $(ROOT)/work/tools/codemap.py
	$(PY) $(ROOT)/work/tools/codemap.py $(GAME)/client.bin $@ > $(B)/codemap.txt

$(B)/tr.c: $(B)/codemap.json $(ROOT)/work/tools/thumb2c.py restored/list.txt
	$(PY) $(ROOT)/work/tools/thumb2c.py $(GAME)/client.bin $< $@ restored/list.txt

$(B)/tr.o: $(B)/tr.c host/thumbrt.h
	cc -O1 -w -std=gnu11 -Ihost $(TRFLAGS) -c $< -o $@

$(B)/r_%.o: restored/%.c restored/restored.h restored/panels.h host/host.h host/thumbrt.h
	cc $(CFLAGS) -c $< -o $@

$(B)/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(B)
	cc $(CFLAGS) -c $< -o $@

$(B)/chronos: $(B)/tr.o $(HOSTOBJ) $(RESTOBJ) $(UIOBJ) $(call RML,mac)/lib/librmlui.a
	c++ $(filter %.o,$^) $(LDFLAGS) -L$(call RML,mac)/lib $(RMLLIBS) -lm -o $@

# 인터프리터 빌드: 번역 C(tr.o) 없이 client.bin의 코드를 직접 실행한다. 웹 빌드의 바탕이고 interp/compare.sh로 번역 빌드와 대조한다.
INTERPSRC := $(RESTSRC) $(HOSTSRC) $(UISRC) $(wildcard restored/*.h)
$(B)/interp_gen.c: restored/list.txt $(ROOT)/work/tools/interpgen.py $(INTERPSRC)
	mkdir -p $(B)
	python3 $(ROOT)/work/tools/interpgen.py restored/list.txt $@ $(INTERPSRC) > $(B)/interp_gen.txt

$(B)/interp_gen.o: $(B)/interp_gen.c host/host.h host/thumbrt.h
	cc $(CFLAGS) -c $< -o $@

$(B)/interp.o: interp/interp.c host/host.h host/thumbrt.h
	cc -O2 -g -Wall -std=gnu11 -Ihost -c $< -o $@

interp: $(B)/chronos-interp $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2 $(B)/ui
$(B)/chronos-interp: $(B)/interp.o $(B)/interp_gen.o $(HOSTOBJ) $(RESTOBJ) $(UIOBJ) $(call RML,mac)/lib/librmlui.a
	c++ $(filter %.o,$^) $(LDFLAGS) -L$(call RML,mac)/lib $(RMLLIBS) -lm -o $@

$(call RML,mac)/lib/librmlui.a:
	./deps.sh mac

$(B)/%.o: host/%.cpp $(UIDEPS) $(call RML,mac)/lib/librmlui.a
	c++ -O2 -g -std=c++17 -DRMLUI_STATIC_LIB -Ihost $(SDL_CFLAGS) -I$(call RML,mac)/include -c $< -o $@

# UI 폰트는 게임 비트맵 폰트에서 만든다.
$(B)/ui: $(wildcard ui/*) $(ROOT)/work/tools/hnf2ttf.py $(GAME)/client.bin
	rm -rf $@ && cp -R ui $@
	$(PY) $(ROOT)/work/tools/hnf2ttf.py $(GAME) $@/ChronosGame.ttf
	touch $@

# MMF를 이벤트로 바꾸는 도구는 빌드 때만 쓴다(smaf-player, MIT). 결과만 실행 파일 옆에 둔다.
$(B)/game.ev: $(GAME)/client.bin
	CARGO_TARGET_DIR=$(abspath $(B))/smaf2ev $(CARGO) run --release --manifest-path $(ROOT)/work/tools/smaf2ev/Cargo.toml -- $(abspath $(GAME))/snd $(abspath $@) > $(B)/game.ev.txt

# Windows 교차 빌드(mingw-w64). SDL3 개발 패키지는 deps.sh가 build/deps/mingw에 받는다. 결과는 build/win 폴더 통째로 옮긴다.
WIN := $(B)/win
WCC := x86_64-w64-mingw32-gcc
WSDL := $(B)/deps/mingw/x86_64-w64-mingw32
WOBJ := $(HOSTSRC:host/%.c=$(WIN)/obj/%.o) $(RESTSRC:restored/%.c=$(WIN)/obj/r_%.o) $(UISRC:host/%.cpp=$(WIN)/obj/%.o)
WRML := $(call RML,win)

windows: $(WIN)/chronos.exe $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2 $(B)/ui
	cp $(WSDL)/bin/SDL3.dll $(B)/game.ev $(B)/1mgm.sf2 $(WIN)/
	cp "$$(find $$(dirname $$(dirname $$(realpath $$(which $(WCC))))) -name libwinpthread-1.dll | grep x86_64 | head -1)" $(WIN)/
	rm -rf $(WIN)/ui && cp -R $(B)/ui $(WIN)/ui
	rm -rf $(WIN)/game && mkdir -p $(WIN)/game
	cd $(GAME) && find . -type f ! -name client.bin474328 ! -path './META-INF/*' -exec sh -c 'mkdir -p "$$0/$$(dirname "$$1")" && cp "$$1" "$$0/$$1"' "$(abspath $(WIN))/game" {} \;

$(WIN)/chronos.exe: $(WIN)/obj/tr.o $(WOBJ) $(WIN)/obj/manifest.o
	x86_64-w64-mingw32-g++ $^ -L$(WSDL)/lib -L$(WRML)/lib $(RMLLIBS) -lSDL3 -lm -static-libgcc -static-libstdc++ -mwindows -o $@

$(WRML)/lib/librmlui.a:
	./deps.sh win

$(WIN)/obj/%.o: host/%.cpp $(UIDEPS) $(WRML)/lib/librmlui.a
	mkdir -p $(WIN)/obj
	x86_64-w64-mingw32-g++ -O2 -std=c++17 -DRMLUI_STATIC_LIB -Ihost -I$(WSDL)/include -I$(WRML)/include -c $< -o $@

$(WIN)/obj/tr.o: $(B)/tr.c host/thumbrt.h
	mkdir -p $(WIN)/obj
	$(WCC) -O1 -w -std=gnu11 -Ihost $(TRFLAGS) -c $< -o $@

$(WIN)/obj/r_%.o: restored/%.c restored/restored.h restored/panels.h host/host.h host/thumbrt.h
	mkdir -p $(WIN)/obj
	$(WCC) -O2 -w -std=gnu11 -D__USE_MINGW_ANSI_STDIO=1 -Ihost -I$(WSDL)/include -c $< -o $@

$(WIN)/obj/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h
	mkdir -p $(WIN)/obj
	$(WCC) -O2 -w -std=gnu11 -D__USE_MINGW_ANSI_STDIO=1 -Ihost -I$(WSDL)/include -c $< -o $@

$(WIN)/obj/manifest.o: windows/chronos.rc windows/chronos.manifest
	mkdir -p $(WIN)/obj
	x86_64-w64-mingw32-windres $< -o $@

android: $(B)/tr.c $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2 $(B)/ui
	[ -f $(call RML,android)/lib/librmlui.a ] || ./deps.sh android
	android/build.sh

# 웹 빌드(Emscripten, 인터프리터). 원본에서 나온 것은 넣지 않는다: 게임 데이터와 글꼴, 소리 이벤트는 사용자가 고른 원본 파일에서 브라우저가 만든다(web/gamedata.js).
# 결과는 build/web 폴더 통째로 올린다. 서비스 워커는 https나 localhost에서만 돈다.
WEB := $(B)/web
WEBDEP := $(B)/deps/web/install
WEBOBJ := $(HOSTSRC:host/%.c=$(B)/web-obj/%.o) $(RESTSRC:restored/%.c=$(B)/web-obj/r_%.o) $(UISRC:host/%.cpp=$(B)/web-obj/%.o) $(B)/web-obj/interp.o $(B)/web-obj/interp_gen.o
WEBCFLAGS := -O2 -w -std=gnu11 -Ihost -I$(WEBDEP)/include
WEBSTATIC := index.html app.js gamedata.js manifest.webmanifest icon.svg icon-192.png icon-512.png
SMAFWASM := $(B)/smaf2ev/wasm32-unknown-unknown/release/smaf2ev.wasm

web: $(WEB)/chronos.js $(SMAFWASM) $(addprefix web/,$(WEBSTATIC)) web/sw.js
	cd web && cp $(WEBSTATIC) $(abspath $(WEB))/
	cp $(SMAFWASM) $(WEB)/
	sed "s/__BUILD__/$$(cat $(WEB)/chronos.js $(WEB)/chronos.wasm web/*.js web/index.html | shasum | cut -c1-12)/" web/sw.js > $(WEB)/sw.js

$(WEB)/chronos.js: $(WEBOBJ) $(wildcard ui/*) third_party/1mgm.sf2
	mkdir -p $(WEB)
	em++ $(WEBOBJ) -O2 -L$(WEBDEP)/lib -lrmlui -lfreetype -lSDL3 -lidbfs.js \
		-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576 \
		-sEXPORTED_FUNCTIONS=_main,_web_save_picked,_web_ime_set,_web_ime_current,_web_ime_ok -sEXPORTED_RUNTIME_METHODS=callMain,ccall,FS,IDBFS,UTF8ToString,HEAPU8 \
		--preload-file ui@/ui --preload-file third_party/1mgm.sf2@/1mgm.sf2 -o $@

$(SMAFWASM): $(ROOT)/work/tools/smaf2ev/src/lib.rs $(ROOT)/work/tools/smaf2ev/Cargo.toml
	CARGO_TARGET_DIR=$(abspath $(B))/smaf2ev $(CARGO) build --release --lib --target wasm32-unknown-unknown --manifest-path $(ROOT)/work/tools/smaf2ev/Cargo.toml

$(WEBDEP)/lib/librmlui.a:
	./deps.sh web

$(B)/web-obj/%.o: host/%.c host/host.h host/thumbrt.h host/ui.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/r_%.o: restored/%.c restored/restored.h restored/panels.h host/host.h host/thumbrt.h | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/%.o: host/%.cpp $(UIDEPS) | $(WEBDEP)/lib/librmlui.a
	mkdir -p $(B)/web-obj
	em++ -O2 -std=c++17 -DRMLUI_STATIC_LIB -Ihost -I$(WEBDEP)/include -c $< -o $@

$(B)/web-obj/interp.o: interp/interp.c host/host.h host/thumbrt.h
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

$(B)/web-obj/interp_gen.o: $(B)/interp_gen.c host/host.h host/thumbrt.h
	mkdir -p $(B)/web-obj
	emcc $(WEBCFLAGS) -c $< -o $@

# macOS 앱 묶음. 게임 데이터와 Homebrew 동적 라이브러리를 함께 넣어 단독으로 실행된다.
mac: all
	mac/bundle.sh

# 사운드폰트 1MGM(고지는 third_party/1mgm.txt)
$(B)/1mgm.sf2:
	cp third_party/1mgm.sf2 $@

clean:
	rm -rf $(B)

.PHONY: all clean
