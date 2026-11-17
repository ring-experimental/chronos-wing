#!/bin/zsh
# APK 빌드. Gradle 없이 NDK, build-tools로 만든다. 먼저 native에서 make를 돌려 build/tr.c와 게임 데이터를 준비한다.
# 결과: build/android/chronos.apk (android/chronos.keystore 서명, arm64-v8a)
# APK에는 원본에서 번역한 코드와 게임 데이터가 들어간다. 원본을 가진 사람이 제 기기에 쓰는 용도이며 저장소와 공개 배포에 올리지 않는다.
set -eu
HERE=${0:A:h}; N=$HERE:h; B=$N/build; O=$B/android
SDK=${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}
NDK=$SDK/ndk/27.2.12479018; BT=$SDK/build-tools/35.0.0; JAR=$SDK/platforms/android-35/android.jar
JDK=/opt/homebrew/opt/openjdk@21/bin; export PATH=$JDK:$PATH
SRC=$B/deps/src; ABI=arm64-v8a; API=24
mkdir -p $O/lib/lib/$ABI $O/java $O/assets

# SDL3. 16KB 페이지 기기에서도 올라가도록 세그먼트를 16KB로 정렬한다.
if [[ ! -f $O/sdl16k/libSDL3.so ]]; then
  cmake -S $SRC -B $O/sdl16k -G Ninja -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
  cmake --build $O/sdl16k --target SDL3-shared
fi
cp $O/sdl16k/libSDL3.so $O/lib/lib/$ABI/

# 게임
CC=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android$API-clang
INC=(-Ihost -I$SRC/include)
cd $N
OBJS=()
mkdir -p $O/obj
for f in host/*.c restored/*.c; do
  o=$O/obj/${${f//\//_}%.c}.o; OBJS+=$o
  [[ $o -nt $f && $o -nt host/host.h && $o -nt host/ui.h ]] || $CC -O2 -w -fPIC -std=gnu11 $INC -c $f -o $o &
done
RML=$B/deps/android/install
for f in host/*.cpp; do
  o=$O/obj/${${f//\//_}%.cpp}.o; OBJS+=$o
  [[ $o -nt $f && $o -nt host/ui.h && $o -nt host/ui_render.h ]] || $CC++ -O2 -w -fPIC -std=c++17 -DRMLUI_STATIC_LIB $INC -I$RML/include -c $f -o $o &
done
tr=$O/obj/tr.o; OBJS+=$tr
[[ $tr -nt $B/tr.c ]] || $CC -O1 -w -fPIC -fvisibility=hidden -std=gnu11 -Ihost -c $B/tr.c -o $tr &
wait
$CC++ -shared -static-libstdc++ -Wl,-z,max-page-size=16384 $OBJS -L$O/lib/lib/$ABI -L$RML/lib -lrmlui -lfreetype -lSDL3 -lm -landroid -llog -o $O/lib/lib/$ABI/libmain.so

# 자바와 패키징
python3 $HERE/mkpak.py $B $O/assets/game.pak $B/ui
# SDL의 Android 파일 창은 기본 이름을 받지 않아 저장 창에만 날짜 이름을 넣는다.
JSRC=$O/java-src; rm -rf $JSRC; mkdir -p $JSRC; cp $SRC/android-project/app/src/main/java/org/libsdl/app/*.java $JSRC/
python3 $HERE/patch_activity.py $JSRC/SDLActivity.java
$JDK/javac -nowarn -source 8 -target 8 -cp $JAR -d $O/java $JSRC/*.java 2>/dev/null
$BT/d8 --release --lib $JAR --min-api $API --output $O $O/java/org/libsdl/app/*.class
# CHRONOS_DEBUGGABLE=1: run-as로 세이브에 접근할 수 있는 진단 빌드.
# 아이콘. 타이틀 화면을 Mac 빌드로 띄워 문구가 깜박여 꺼진 프레임(339)을 받아 만든다. 원본 파생물이라 build에만 둔다.
if [[ ! -f $O/res/mipmap-anydpi-v26/ic_launcher.xml ]]; then
  F=$O/icon-frames; rm -rf $F $O/res; mkdir -p $F/save
  print '340 quit' > $F/script.txt
  CHRONOS_FRAMELOG=$F CHRONOS_SCRIPT=$F/script.txt $B/chronos --windowed --save $F/save >/dev/null 2>&1
  python3 $HERE/mkicon.py $F/$(awk '$1==339{print $2}' $F/log.txt).raw $O/res
fi
$BT/aapt2 compile --dir $O/res -o $O/res.zip
$BT/aapt2 link ${CHRONOS_DEBUGGABLE:+--debug-mode} -o $O/base.apk -I $JAR --manifest $HERE/AndroidManifest.xml -A $O/assets -R $O/res.zip --auto-add-overlay
python3 $HERE/patch_manifest.py $O/base.apk $BT/aapt2
cd $O
zip -q -j base.apk classes.dex
(cd lib && zip -q -r ../base.apk lib)
$BT/zipalign -f -p 16 base.apk aligned.apk
# 서명 키. 키가 바뀌면 설치된 앱을 지워야 업데이트되므로(세이브 유실) build 밖에 두고 계속 쓴다. git에는 올리지 않는다.
KS=$HERE/chronos.keystore
[[ -f $KS ]] || $JDK/keytool -genkeypair -keystore $KS -storetype PKCS12 -storepass chronoswing -keypass chronoswing \
  -alias chronos -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Chronos Wing" 2>/dev/null
$BT/apksigner sign --ks $KS --ks-key-alias chronos --ks-pass pass:chronoswing --out chronos.apk aligned.apk
ls -la chronos.apk
