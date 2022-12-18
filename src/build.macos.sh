CC=gcc
BASE_DIR="/Users/me/Projects/Games/Hitman/"

mkdir -p $BASE_DIR/build
pushd $BASE_DIR/build
$CC -g3 $BASE_DIR/src/platform_awe.cpp -o $BASE_DIR/build/macos_awe.out `sdl2-config --cflags --libs`
popd
