CC=gcc
COMMON_COMPILER_FLAGS="-g3 -Wall -std=c++20"

BUILD_FLAGS="-DHITMAN_DEBUG=1"

BASE_DIR="${HOME}/Projects/Games/Hitman/"

SDL2_INCLUDE_FLAGS="-I$BASE_DIR\lib\SDL2-2.26.1\x86_64-w64-mingw32\include"
SDL2_LINKER_FLAGS="`$BASE_DIR/lib/SDL2-2.26.1/x86_64-w64-mingw32/bin/sdl2-config --cflags --libs`"

COMMON_LINKER_FLAGS="-ldl"

mkdir -p $BASE_DIR/build
pushd $BASE_DIR/build

# Create Game service
$CC $COMMON_COMPILER_FLAGS -shared -o ./libhitman.so -fPIC $BASE_DIR/src/hitman.cpp 

# Create platform layer that uses platform agnostic Game
$CC $COMMON_COMPILER_FLAGS $BUILD_FLAGS -o hitman_win32.exe -fPIC $BASE_DIR/src/platform.cpp $SDL2_INCLUDE_FLAGS $SDL2_LINKER_FLAGS $COMMON_LINKER_FLAGS

# TODO: check iof we can add a DLL search path
cp $BASE_DIR/lib/SDL2-2.26.1/x86_64-w64-mingw32/bin/SDL2.dll .

popd
