CC=gcc
COMMON_COMPILER_FLAGS="-g3 -Wall -std=c++11"
SDL2_FLAGS="`sdl2-config --cflags --libs`"

BASE_DIR="${HOME}/Projects/Games/Hitman/"
mkdir -p $BASE_DIR/build
pushd $BASE_DIR/build

# Create Game service
$CC $COMMON_COMPILER_FLAGS -shared -o ./libhitman.so -fPIC $BASE_DIR/src/hitman.cpp 

# Create platform layer that uses platform agnostic Game
$CC $COMMON_COMPILER_FLAGS -o hitman_macos -fPIC $BASE_DIR/src/platform_macos.cpp $SDL2_FLAGS

popd
