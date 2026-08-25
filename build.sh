#!/bin/bash
# 始终在项目根目录编译
set -e
cd "$(dirname "$0")"


cmake --build build  "$@"
