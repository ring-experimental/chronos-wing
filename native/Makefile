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

# 사운드폰트 1MGM(고지는 third_party/1mgm.txt)
$(B)/1mgm.sf2:
	cp third_party/1mgm.sf2 $@

clean:
	rm -rf $(B)

.PHONY: all clean
