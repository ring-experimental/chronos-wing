#!/bin/zsh
# macOS 앱 묶음. 먼저 native에서 make를 돌린다. 결과: build/mac/ChronosWing.app (arm64, 임시 서명)
# 묶음에는 원본에서 번역한 코드와 게임 데이터가 들어간다. 원본을 가진 사람이 쓰는 용도이며 저장소와 공개 배포에 올리지 않는다.
# 실행 파일은 SDL_GetBasePath(묶음이면 Contents/Resources) 아래 game, ui, game.ev, 1mgm.sf2를 읽는다.
# Homebrew 동적 라이브러리는 Contents/Frameworks로 옮기고 @rpath로 바꾼다.
set -eu
HERE=${0:A:h}; N=$HERE:h; B=$N/build
APP=$B/mac/ChronosWing.app; C=$APP/Contents
rm -rf $APP; mkdir -p $C/MacOS $C/Resources $C/Frameworks
cp $B/chronos $C/MacOS/chronos
cp $HERE/Info.plist $C/Info.plist

# 게임 데이터는 Windows 묶음과 같이 원본 백업 파일과 서명 폴더를 뺀다.
(cd $B/game && find . -type f ! -name client.bin474328 ! -path './META-INF/*' | cpio -pdm --quiet $C/Resources/game)
cp -R $B/ui $C/Resources/ui
cp $B/game.ev $B/1mgm.sf2 $C/Resources/

# 아이콘. Android 빌드가 만든 타이틀 프레임 아이콘(512px)이 있으면 쓴다.
ICON=$B/android/res/mipmap-xxxhdpi/ic_launcher.png
if [[ -f $ICON ]]; then
  S=$B/mac/icon.iconset; rm -rf $S; mkdir -p $S
  for z in 16 32 128 256 512; do
    sips -z $z $z $ICON --out $S/icon_${z}x${z}.png >/dev/null
    sips -z $((z*2)) $((z*2)) $ICON --out $S/icon_${z}x${z}@2x.png >/dev/null
  done
  iconutil -c icns $S -o $C/Resources/chronos.icns
fi

# Homebrew 접두사(다른 곳이면 BREW_PREFIX) 아래의 의존성을 따라가며 복사한다.
PFX=${BREW_PREFIX:-$(brew --prefix)}
deps() { otool -L $1 | tail -n +2 | awk '{print $1}' | grep "^$PFX/" || true; }
# @rpath, @loader_path로 적힌 것(brotlidec → brotlicommon)은 그 라이브러리가 있던 폴더에서 찾는다.
rdeps() {
  local d=${2:h}
  otool -L $1 | tail -n +2 | awk '{print $1}' | grep -E '^@(rpath|loader_path)/' | while read r; do
    [[ -f $d/${r:t} ]] && echo $d/${r:t}
  done
}
typeset -A seen
queue=($(deps $C/MacOS/chronos))
while (( $#queue )); do
  l=$queue[1]; shift queue
  n=${l:t}
  [[ -n ${seen[$n]-} ]] && continue
  seen[$n]=$l
  cp -L $l $C/Frameworks/$n; chmod u+w $C/Frameworks/$n
  queue+=($(deps $l) $(rdeps $l ${l:A}))
done
fix() {
  for l in $(deps $1); do install_name_tool -change $l @rpath/${l:t} $1 2>/dev/null; done
}
fix $C/MacOS/chronos
install_name_tool -add_rpath @executable_path/../Frameworks $C/MacOS/chronos
for f in $C/Frameworks/*.dylib; do
  install_name_tool -id @rpath/${f:t} $f 2>/dev/null
  fix $f
done
# brotli처럼 @loader_path로 서로를 찾는 라이브러리도 Frameworks 안에서 해결되는지 본다.
for f in $C/MacOS/chronos $C/Frameworks/*.dylib; do
  if otool -L $f | tail -n +2 | awk '{print $1}' | grep -q "^$PFX/"; then echo "남은 Homebrew 경로: $f" >&2; exit 1; fi
done
codesign --force --deep --sign - $APP
codesign --verify --deep --strict $APP
# 다른 Mac으로 옮길 때는 zip으로 넘긴다(ditto가 서명과 속성을 보존한다).
rm -f $B/mac/ChronosWing-mac.zip
ditto -c -k --keepParent $APP $B/mac/ChronosWing-mac.zip
du -sh $APP $B/mac/ChronosWing-mac.zip
