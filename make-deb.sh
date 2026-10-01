#!/bin/sh
# 一键构建 Debian/Ubuntu 安装包。
#   ./make-deb.sh
# 产物：build-pkg/mcl_<版本>_<架构>.deb
#
# 可用环境变量覆盖：BUILD_DIR、JOBS
set -eu

cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build-pkg}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" -j "$JOBS"
cmake --build "$BUILD_DIR" --target package

printf '\n产物：\n'
ls -lh "$BUILD_DIR"/mcl_*.deb
