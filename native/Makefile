# 크로노스 윙 네이티브 빌드.
# 전제: 저장소 루트의 크로노스윙.zip, work/tools의 파이썬 도구(capstone).
ROOT := $(abspath ..)
PY := $(HOME)/private/reverse/tools/venv/bin/python
ZIP := $(ROOT)/크로노스윙.zip
B := build
GAME := $(B)/game
HOSTSRC := $(wildcard host/*.c)
HOSTOBJ := $(HOSTSRC:host/%.c=$(B)/%.o)
RESTSRC := $(wildcard restored/*.c)
RESTOBJ := $(RESTSRC:restored/%.c=$(B)/r_%.o)
TRFLAGS ?= -DTRACE_PC
CFLAGS := -O2 -g -w -std=gnu11 -Ihost -I/opt/homebrew/include
LDFLAGS := -L/opt/homebrew/lib -lSDL3

.PHONY: all windows android
all: $(B)/chronos $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2

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

$(B)/r_%.o: restored/%.c restored/restored.h host/host.h host/thumbrt.h
	cc $(CFLAGS) -c $< -o $@

$(B)/%.o: host/%.c host/host.h host/thumbrt.h
	mkdir -p $(B)
	cc $(CFLAGS) -c $< -o $@

$(B)/chronos: $(B)/tr.o $(HOSTOBJ) $(RESTOBJ)
	cc $^ $(LDFLAGS) -lm -o $@

# MMF를 이벤트로 바꾸는 도구는 빌드 때만 쓴다(smaf-player, MIT). 결과만 실행 파일 옆에 둔다.
$(B)/game.ev: $(GAME)/client.bin
	PATH=/opt/homebrew/opt/rustup/bin:$$PATH CARGO_TARGET_DIR=$(abspath $(B))/smaf2ev cargo run --release --manifest-path $(ROOT)/work/tools/smaf2ev/Cargo.toml -- $(abspath $(GAME))/snd $(abspath $@) > $(B)/game.ev.txt

# Windows 교차 빌드(mingw-w64). SDL3 개발 패키지는 build/deps/mingw에 받아 둔다. 결과는 build/win 폴더 통째로 옮긴다.
WIN := $(B)/win
WCC := x86_64-w64-mingw32-gcc
WSDL := $(B)/deps/mingw/x86_64-w64-mingw32
WOBJ := $(HOSTSRC:host/%.c=$(WIN)/obj/%.o) $(RESTSRC:restored/%.c=$(WIN)/obj/r_%.o)

windows: $(WIN)/chronos.exe $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2
	cp $(WSDL)/bin/SDL3.dll $(B)/game.ev $(B)/1mgm.sf2 $(WIN)/
	rm -rf $(WIN)/game && mkdir -p $(WIN)/game
	cd $(GAME) && find . -type f ! -name client.bin474328 ! -path './META-INF/*' -exec sh -c 'mkdir -p "$$0/$$(dirname "$$1")" && cp "$$1" "$$0/$$1"' "$(abspath $(WIN))/game" {} \;

$(WIN)/chronos.exe: $(WIN)/obj/tr.o $(WOBJ) $(WIN)/obj/manifest.o
	$(WCC) $^ -L$(WSDL)/lib -lSDL3 -lm -static-libgcc -mwindows -o $@

$(WIN)/obj/tr.o: $(B)/tr.c host/thumbrt.h
	mkdir -p $(WIN)/obj
	$(WCC) -O1 -w -std=gnu11 -Ihost $(TRFLAGS) -c $< -o $@

$(WIN)/obj/r_%.o: restored/%.c restored/restored.h host/host.h host/thumbrt.h
	mkdir -p $(WIN)/obj
	$(WCC) -O2 -w -std=gnu11 -D__USE_MINGW_ANSI_STDIO=1 -Ihost -I$(WSDL)/include -c $< -o $@

$(WIN)/obj/%.o: host/%.c host/host.h host/thumbrt.h
	mkdir -p $(WIN)/obj
	$(WCC) -O2 -w -std=gnu11 -D__USE_MINGW_ANSI_STDIO=1 -Ihost -I$(WSDL)/include -c $< -o $@

$(WIN)/obj/manifest.o: windows/chronos.rc windows/chronos.manifest
	mkdir -p $(WIN)/obj
	x86_64-w64-mingw32-windres $< -o $@

android: $(B)/tr.c $(GAME)/client.bin $(B)/game.ev $(B)/1mgm.sf2
	android/build.sh

# 사운드폰트 1MGM(고지는 third_party/1mgm.txt)
$(B)/1mgm.sf2:
	cp third_party/1mgm.sf2 $@

clean:
	rm -rf $(B)

.PHONY: all clean
