#!/bin/bash
# 始终在项目根目录执行 cmake（兼容 CMake < 3.13，不依赖 -S/-B）
set -e
cd "$(dirname "$0")"
echo "Project root: $(pwd)"

# 清掉误生成在源码目录的 cache，避免再次 in-source 配置
if [ -f CMakeCache.txt ]; then
    echo "Removing in-source CMakeCache.txt ..."
    rm -f CMakeCache.txt
    rm -rf CMakeFiles
fi

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release "$@"

if [ ! -f CMakeCache.txt ]; then
    echo "ERROR: configure failed (no CMakeCache.txt in build/)" >&2
    exit 1
fi

echo "Configure OK. Cache: $(pwd)/CMakeCache.txt"
