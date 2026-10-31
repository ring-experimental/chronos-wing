#!/bin/zsh
# WIE CLI는 BGM을 MIDI 포트로만 보낸다. macOS에는 기본 포트가 없어 FluidSynth로 만들어 준다.
# 사용법: work/play.sh  (배경음 크기: BGM_GAIN=0.8 work/play.sh)
set -u

ROOT="${0:A:h:h}"
WIE_DIR="$HOME/private/reverse/tools/wie"
GAME="${GAME:-$ROOT/크로노스윙.zip}"

# FluidSynth는 stdin이 닫히면 종료하므로, 이 스크립트가 살아 있는 동안 열어 둔다.
fluidsynth -a coreaudio -m coremidi -g "${BGM_GAIN:-0.5}" -o midi.portname=WIE \
  "$WIE_DIR/assets/1mgm.sf2" < <(while kill -0 $$ 2>/dev/null; do sleep 5; done) >/dev/null 2>&1 &
SYNTH=$!
trap 'kill $SYNTH 2>/dev/null' EXIT

# CoreMIDI 포트가 생길 시간을 준다. 게임은 시작할 때 한 번만 포트를 찾는다.
sleep 1
"$WIE_DIR/target/release/wie" "$GAME"
