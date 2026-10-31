#!/bin/zsh
# stress.sh <출력폴더> <초> [시드]: 무작위 키로 오래 돌려 충돌과 미구현 API를 찾는다. 저장 폴더는 유지한다.
OUT=$1; SECS=$2; RANDOM=${3:-1}; mkdir -p $OUT/save; CTL=$OUT/ctrl
[[ -n ${SAVE_FROM:-} ]] && cp -R $SAVE_FROM/. $OUT/save/
HERE=${0:A:h}
CHRONOS_TRACE=$OUT/api.txt $HERE/build/chronos $HERE/build/game --windowed --save $OUT/save --ctrl $CTL 2> $OUT/err.log &
P=$!; sleep 5
KEYS=(${=STRESS_KEYS:-ok ok up down left right up down left right ok lsk rsk star hash})
T0=$SECONDS; n=0
while (( SECONDS - T0 < SECS )) && kill -0 $P 2>/dev/null; do
  print "key ${KEYS[$((RANDOM % ${#KEYS} + 1))]}" > $CTL; sleep 0.3
  n=$((n+1)); (( n % 30 == 0 )) && { print "shot $OUT/f$((n/30)).ppm" > $CTL; sleep 0.1 }
done
kill -0 $P 2>/dev/null && { print quit > $CTL; sleep 1 }
kill $P 2>/dev/null; wait $P; print "exit $?" >> $OUT/err.log
