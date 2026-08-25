#!/bin/bash
set -e

# 切换到脚本所在目录（项目根目录），避免在 build/ 里误执行
cd "$(dirname "$0")"

echo "Project root: $(pwd)"

echo "==> Installing dependencies (requires sudo)..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libglfw3-dev \
    libassimp-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev

echo "==> Configuring..."
./configure.sh

echo "==> Building..."
./build.sh

echo ""
echo "Build complete. Run from bin directory:"
echo "  cd bin && ./model_loading backpack/backpack.obj"
