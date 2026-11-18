#!/bin/zsh
# framecmp.sh <출력폴더> <스크립트|-> <초>: (WIE_BIN=<wie 실행 파일>, 원본이 저장소 루트에 없으면 ZIP=<경로>) 같은 프레임 스크립트로 WIE와 네이티브를 임시 저장 폴더에서 돌려 프레임 로그를 남긴다.
OUT=$1; SCRIPT=$2; SECS=$3; ROOT=${0:A:h:h:h}
rm -rf $OUT; mkdir -p $OUT/wie $OUT/nat $OUT/home $OUT/save
[[ $SCRIPT == - ]] && SCRIPT=/dev/null
[[ -n ${SAVE_WIE:-} ]] && cp -R $SAVE_WIE/. $OUT/home/
[[ -n ${SAVE_NAT:-} ]] && cp -R $SAVE_NAT/. $OUT/save/
BIN=${WIE_BIN:?WIE_BIN에 wie 실행 파일 경로를 준다}
HOME=$OUT/home WIE_CTRL=$OUT/ctrl.txt WIE_SCRIPT=$SCRIPT WIE_FRAMELOG=$OUT/wie $BIN --windowed ${ZIP:-$ROOT/크로노스윙.zip} >/dev/null 2>$OUT/wie.err &
W=$!
CHRONOS_SCRIPT=$SCRIPT CHRONOS_FRAMELOG=$OUT/nat $ROOT/native/build/chronos $ROOT/native/build/game --windowed --save $OUT/save 2>$OUT/nat.err &
N=$!
sleep $SECS
kill $W $N 2>/dev/null; wait 2>/dev/null
wc -l $OUT/wie/log.txt $OUT/nat/log.txt
