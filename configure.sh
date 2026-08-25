#!/bin/bash
# 始终在项目根目录执行 cmake，避免在 build/ 里误运行
set -e
cd "$(dirname "$0")"
echo "Project root: $(pwd)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "$@"
