CC=gcc

SDL2_INCLUDE_FLAGS="-Iw:\hitman\lib\SDL2-2.26.1\x86_64-w64-mingw32\include"
SDL2_LINKER_FLAGS="`W:/hitman/lib/SDL2-2.26.1/x86_64-w64-mingw32/bin/sdl2-config --cflags --libs`"

mkdir -p w:/hitman/build
pushd w:/hitman/build
$CC -g3 w:/hitman/src/platform_awe.cpp -o w:/hitman/build/win32_awe.exe $SDL2_INCLUDE_FLAGS $SDL2_LINKER_FLAGS
popd
