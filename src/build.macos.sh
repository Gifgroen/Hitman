CC=gcc

BASE_DIR="${HOME}/Projects/Games/Hitman/"

COMMON_COMPILER_FLAGS="-g3 -Wall -std=c++11"
BUILD_FLAGS="-DHITMAN_DEBUG=1"

SDL2_LINKER_FLAGS="`sdl2-config --cflags --libs`"

mkdir -p $BASE_DIR/build
pushd $BASE_DIR/build

# Create Game service
$CC $COMMON_COMPILER_FLAGS -shared -o ./libhitman.so -fPIC $BASE_DIR/src/hitman.cpp 

# Create platform layer that uses platform agnostic Game
$CC $COMMON_COMPILER_FLAGS $BUILD_FLAGS -o hitman_macos -fPIC $BASE_DIR/src/platform.cpp $SDL2_LINKER_FLAGS

popd
