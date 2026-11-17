#!/bin/zsh
# compare.sh <출력폴더> <가상 초> [시드]: 번역 빌드와 인터프리터 빌드를 같은 무작위 키 대본, 가상 시계로 돌려 대조한다.
# 견주는 것: 프레임 해시 기록, API 호출 기록(id, lr), 끝날 때의 게스트 메모리, 저장 폴더. SAVE_FROM=<폴더>면 그 세이브로 시작한다.
# BINS="a b"로 견줄 실행 파일을 바꾼다(같은 빌드 두 번이면 재현성 확인).
# DATA_ONLY=1이면 끝 메모리를 스택 앞(0x300000)까지만 견준다. 함수를 읽는 C로 옮기면 원본이 스택에 남기던 찌꺼기가 달라지기 때문이다.
OUT=$1; N=$2; RANDOM=${3:-1}; HERE=${0:A:h}; B=$HERE/../build
KEYS=(${=COMPARE_KEYS:-ok ok ok up down left right up down left right lsk rsk clear star hash 5})
rm -rf $OUT; mkdir -p $OUT
N=$((N * 1000)); f=3000
while (( f < N )); do
  k=${KEYS[$((RANDOM % ${#KEYS} + 1))]}
  print "$f key $k\n$((f + 100 + RANDOM % 200)) up $k" >> $OUT/script.txt
  f=$((f + 300 + RANDOM % 700))
done
print "$N dump end\n$N quit" >> $OUT/script.txt
for bin in ${=BINS:-chronos chronos-interp}; do
  D=$OUT/$bin; [[ -d $D ]] && D=$OUT/$bin.2
  mkdir -p $D/save $D/frames
  [[ -n ${SAVE_FROM:-} ]] && cp -R $SAVE_FROM/. $D/save/
  T0=$(date +%s)
  CHRONOS_FAKE_TIME=1 CHRONOS_VCLOCK=1 CHRONOS_SCRIPT=$OUT/script.txt CHRONOS_FRAMELOG=$D/frames CHRONOS_TRACE=$D/api.txt \
    $B/$bin $B/game --windowed --save $D/save 2> $D/err.log
  print "$bin: 종료 $?, $(tail -1 $D/err.log), $(( $(date +%s) - T0 ))초, 프레임 $(wc -l < $D/frames/log.txt), API 호출 $(wc -l < $D/api.txt)"
  DIRS+=($D)
done
A=$DIRS[1]; C=$DIRS[2]; ok=1
cmp -s $A/frames/log.txt $C/frames/log.txt && print "프레임 기록: 같음" || { print "프레임 기록: 다름"; diff $A/frames/log.txt $C/frames/log.txt | head -5; ok=0 }
cmp -s $A/api.txt $C/api.txt && print "API 기록: 같음" || { print "API 기록: 다름, 첫 차이 $(cmp $A/api.txt $C/api.txt | head -1)"; ok=0 }
[[ ! -e $A/frames/mem_end.bin && ! -e $C/frames/mem_end.bin ]] && print "끝 메모리: 둘 다 대본이 끝나기 전에 게임이 종료함" || cmp -s ${DATA_ONLY:+-n} ${DATA_ONLY:+1455916} $A/frames/mem_end.bin $C/frames/mem_end.bin && print "끝 메모리: 같음" || { print "끝 메모리: 다름 $(cmp -l $A/frames/mem_end.bin $C/frames/mem_end.bin | wc -l)바이트"; ok=0 }
diff -rq $A/save $C/save > /dev/null && print "저장 폴더: 같음" || { print "저장 폴더: 다름"; diff -rq $A/save $C/save | head -5; ok=0 }
(( ok )) && print "결과: 일치" || print "결과: 불일치"
