#!/bin/bash
# 始终在项目根目录编译
set -e
cd "$(dirname "$0")"

if [ ! -f build/CMakeCache.txt ]; then
    echo "No CMake cache in build/. Running configure first..."
    ./configure.sh
fi

# 若 cache 仍在源码根目录，说明之前 in-source 配过，重新配置到 build/
if [ -f CMakeCache.txt ] && [ ! -f build/CMakeCache.txt ]; then
    echo "Found in-source cache; reconfiguring into build/ ..."
    ./configure.sh
fi

cmake --build build --parallel "$(nproc 2>/dev/null || echo 2)" "$@"
