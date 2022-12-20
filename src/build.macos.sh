CC=gcc

INITIAL_WORKING_DIRECTORY=$(pwd)
BASE_DIR="${HOME}/Projects/Games/Hitman/"

COMMON_COMPILER_FLAGS="-g3 -Wall -std=c++11"
PLATFORM_LINKER_FLAGS="-L$BASE_DIR/build -lhitman"
SDL2_FLAGS="`sdl2-config --cflags --libs`"

mkdir -p $BASE_DIR/build
pushd $BASE_DIR/build

# Create Game service
$CC $COMMON_COMPILER_FLAGS -shared -o $BASE_DIR/build/libhitman.so -fPIC $BASE_DIR/src/hitman.cpp 

# Create platform layer that uses platform agnostic Game
$CC $COMMON_COMPILER_FLAGS -o $BASE_DIR/build/hitman_macos -fPIC $BASE_DIR/src/platform_macos.cpp $SDL2_FLAGS

popd
