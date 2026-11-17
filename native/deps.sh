#!/bin/zsh
# deps.sh <mac|win|android|web>: FreeType과 RmlUi를 정적 라이브러리로 build/deps/<대상>에 빌드한다. 소스가 build/deps에 없으면 공개 배포본을 받는다.
# web은 Emscripten(brew install emscripten)으로 SDL3(build/deps/src)도 함께 빌드한다. 스레드는 쓰지 않는다.
# 여기서 받는 것은 모두 공개 라이브러리다. 게임 원본은 이 저장소에도, 이 스크립트가 받는 것에도 없다.
set -eu
P=$1; N=${0:A:h}; D=$N/build/deps; O=$D/$P; I=$O/install
SDK=${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}
mkdir -p $D
FT=freetype-2.13.3; SDL=3.4.18; RML=6.3
[[ -f $D/$FT.tar.xz ]] || curl -fsSL -o $D/$FT.tar.xz https://download.savannah.gnu.org/releases/freetype/$FT.tar.xz
[[ -d $D/$FT ]] || tar xJf $D/$FT.tar.xz -C $D
if [[ ! -d $D/rmlui ]]; then
  [[ -f $D/rmlui.tar.gz ]] || curl -fsSL -o $D/rmlui.tar.gz https://github.com/mikke89/RmlUi/archive/refs/tags/$RML.tar.gz
  mkdir -p $D/rmlui && tar xzf $D/rmlui.tar.gz -C $D/rmlui --strip-components 1
fi
if [[ $P == web && ! -d $D/src ]]; then
  [[ -f $D/SDL3-$SDL.tar.gz ]] || curl -fsSL -o $D/SDL3-$SDL.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz
  mkdir -p $D/src && tar xzf $D/SDL3-$SDL.tar.gz -C $D/src --strip-components 1
fi
case $P in
  mac) T=() ;;
  win) T=(-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres) ;;
  android) T=(-DCMAKE_TOOLCHAIN_FILE=$SDK/ndk/27.2.12479018/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static) ;;
  web) T=(-DCMAKE_TOOLCHAIN_FILE=$(em-config EMSCRIPTEN_ROOT)/cmake/Modules/Platform/Emscripten.cmake) ;;
  *) print "대상: mac win android web"; exit 1 ;;
esac
C=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=$I -DCMAKE_FIND_ROOT_PATH=$I -DCMAKE_PREFIX_PATH=$I -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH)
if [[ $P == web ]]; then
  cmake -S $D/src -B $O/sdl $C $T -DBUILD_SHARED_LIBS=OFF -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_PTHREADS=OFF >/dev/null
  cmake --build $O/sdl --target install >/dev/null
fi
cmake -S $D/$FT -B $O/ft $C $T -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON >/dev/null
cmake --build $O/ft --target install >/dev/null
cmake -S $D/rmlui -B $O/rml $C $T -DBUILD_SHARED_LIBS=OFF -DRMLUI_SAMPLES=OFF -DBUILD_TESTING=OFF -DRMLUI_FONT_ENGINE=freetype >/dev/null
cmake --build $O/rml --target install >/dev/null
ls $I/lib/*.a
