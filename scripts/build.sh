#!/bin/bash
#
# Explorer for Linux 统一构建入口。
# 与 CWD 无关：无论从哪里调用，都会在项目根上工作；构建目录可任意指定（相对路径基于项目根解析）。
#
# 用法:
#   ./build.sh                 # 就地构建到 <项目根>/build
#   ./build.sh /tmp/out        # 构建到任意目录（out-of-tree）
#   ./build.sh build run       # 构建后前台启动 winlogin.exe（Ctrl+C 优雅退出）
#   ./build.sh build explorer  # 构建后前台直跑 explorer.exe（开发模式，无守护/不扑杀）
#
set -euo pipefail

SCRIPT_PATH="$(readlink -f "$0")"
SOURCE_DIR="$(cd "$(dirname "$SCRIPT_PATH")/.." && pwd)"

BUILD_DIR="${1:-build}"
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$SOURCE_DIR/$BUILD_DIR"
fi
MODE="${2:-build}"

cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

case "$MODE" in
    run)
        echo ">>> 启动 winlogin.exe（Ctrl+C 退出）"
        exec "$BUILD_DIR/bin/winlogin.exe"
        ;;
    explorer)
        echo ">>> 开发模式：直接启动 explorer.exe（无守护、EXPLORER_SUPERVISED 为空）"
        exec "$BUILD_DIR/bin/explorer.exe"
        ;;
    *)
        echo
        echo ">>> 构建完成。产物目录: $BUILD_DIR/bin"
        echo "      winlogin.exe / explorer.exe / crashguard / explorer-killall"
        echo "      测试 UI (开发模式):  $SOURCE_DIR/scripts/build.sh $BUILD_DIR explorer"
        echo "      测试守护 (完整模式): $SOURCE_DIR/scripts/build.sh $BUILD_DIR run"
        echo ">>> 安装: $SOURCE_DIR/scripts/install.sh --build-dir $BUILD_DIR"
        ;;
esac