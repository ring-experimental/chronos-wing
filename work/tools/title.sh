#!/bin/zsh
# title.sh <zip> <outdir> [keys...]: 게임을 띄워 타이틀까지 키를 보내고 프레임을 덤프한다.
ZIP=$1; OUT=$2; shift 2; mkdir -p $OUT; CTL=$OUT/ctrl.txt; rm -f $CTL
WIE_CTRL=$CTL ~/private/reverse/tools/wie/target/release/wie $ZIP >/dev/null 2>$OUT/err.log &
P=$!; sleep 8
n=0
for k in "$@"; do
  if [[ $k == wait* ]]; then sleep ${k#wait}; else print "key $k" > $CTL; sleep 1.5; fi
  if [[ $k != wait* ]]; then n=$((n+1)); print "shot $OUT/s$n.ppm" > $CTL; sleep 0.3; fi
done
print quit > $CTL; sleep 1; kill $P 2>/dev/null; true
