#!/bin/zsh

set -e

cmake -S. -Bbuild -DBUILD_TESTS=1
cmake --build build -j10
pushd build && ctest --output-on-failure && popd
