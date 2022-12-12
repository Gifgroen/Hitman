@echo off

mkdir -p w:/hitman/build
pushd w:/hitman/build
cl -Zi w:/hitman/src/win32_awe.cpp
popd
