#!/bin/zsh
# run_test.sh <출력폴더> [키...]: 임시 저장 폴더로 창 모드 실행하고 키마다 프레임을 남긴다. waitN은 대기.
OUT=$1; shift; rm -rf $OUT; mkdir -p $OUT/save; CTL=$OUT/ctrl
[[ -n ${SAVE_FROM:-} ]] && cp -R $SAVE_FROM/. $OUT/save/
HERE=${0:A:h}
$HERE/build/chronos $HERE/build/game --windowed --save $OUT/save --ctrl $CTL 2> $OUT/err.log &
P=$!; sleep 6
n=0
for k in "$@"; do
  if [[ $k == wait* ]]; then sleep ${k#wait}; else print "key $k" > $CTL; sleep 1.2; n=$((n+1)); print "shot $OUT/s$n.ppm" > $CTL; sleep 0.3; fi
done
print "shot $OUT/last.ppm" > $CTL; sleep 0.3
print quit > $CTL; sleep 1; kill $P 2>/dev/null
wait $P; print "exit $?" >> $OUT/err.log
