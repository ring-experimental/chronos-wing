#!/bin/zsh
# deps.sh <mac|win|android>: FreeType과 RmlUi를 정적 라이브러리로 build/deps/<대상>에 빌드한다. 소스는 받아 둔 build/deps 것을 쓴다.
set -eu
P=$1; N=${0:A:h}; D=$N/build/deps; O=$D/$P; I=$O/install
SDK=${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}
FT=freetype-2.13.3
[[ -f $D/$FT.tar.xz ]] || curl -fsSL -o $D/$FT.tar.xz https://download.savannah.gnu.org/releases/freetype/$FT.tar.xz
[[ -d $D/$FT ]] || tar xJf $D/$FT.tar.xz -C $D
case $P in
  mac) T=() ;;
  win) T=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres) ;;
  android) T=(-DCMAKE_TOOLCHAIN_FILE=$SDK/ndk/27.2.12479018/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static) ;;
  *) print "대상: mac win android"; exit 1 ;;
esac
C=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=$I -DCMAKE_FIND_ROOT_PATH=$I -DCMAKE_PREFIX_PATH=$I -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH)
cmake -S $D/$FT -B $O/ft $C $T -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON >/dev/null
cmake --build $O/ft --target install >/dev/null
cmake -S $D/rmlui -B $O/rml $C $T -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DBUILD_TESTING=OFF -DRMLUI_FONT_ENGINE=freetype >/dev/null
cmake --build $O/rml --target install >/dev/null
ls $I/lib/*.a
