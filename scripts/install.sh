#!/bin/bash
#
# Explorer for Linux 安装器 —— 与 CWD/调用位置无关。
#
# 用法:
#   ./install.sh                    # 仅生成配置（安全预览）
#   ./install.sh apply              # 生效：启用 systemd 强守护
#   ./install.sh --bin-dir ~/bin    # 自定义二进制目录
#   ./install.sh --build-dir /tmp/obuild   # 使用外部构建目录（产物不在项目内时）
#   ./install.sh --no-build         # 产物缺失时不自动构建，直接报错退出
#   ./install.sh --help             # 打印用法
#
set -euo pipefail

SCRIPT_PATH="$(readlink -f "$0")"
SOURCE_DIR="$(cd "$(dirname "$SCRIPT_PATH")/.." && pwd)"
BUILD_DIR="$SOURCE_DIR/build"
BIN_DIR="$HOME/.local/bin"
APPLY="no"
NO_BUILD="no"

while [[ $# -gt 0 ]]; do
    case "$1" in
        apply) APPLY="yes" ;;
        --no-build) NO_BUILD="yes" ;;
        --bin-dir) BIN_DIR="$2"; shift ;;
        --build-dir) BUILD_DIR="$2"; shift ;;
        --help|-h)
            grep '^#   ' "$0" | sed 's/^#   /  /'
            exit 0
            ;;
        *) echo "未知参数: $1 (见 --help)"; exit 2 ;;
    esac
    shift
done

CFG_DIR="$HOME/.config/explorer-linux"

echo "=== Explorer for Linux 安装器 ==="
echo "源目录:   $SOURCE_DIR"
echo "构建目录: $BUILD_DIR"
echo "二进制:   $BIN_DIR"
echo "配置:     $CFG_DIR"
echo "模式:     ${APPLY}"

if [ ! -x "$BUILD_DIR/bin/winlogin.exe" ]; then
    if [ "$NO_BUILD" = "yes" ]; then
        echo "[错误] 未找到 $BUILD_DIR/bin/winlogin.exe（已指定 --no-build）"
        exit 1
    fi
    echo "未找到构建产物，自动构建到 $BUILD_DIR ..."
    cmake -B "$BUILD_DIR" -S "$SOURCE_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

mkdir -p "$BIN_DIR" "$CFG_DIR"

install -m755 "$BUILD_DIR/bin/winlogin.exe"     "$BIN_DIR/winlogin.exe"
install -m755 "$BUILD_DIR/bin/explorer.exe"     "$BIN_DIR/explorer.exe"
install -m755 "$BUILD_DIR/bin/crashguard"       "$BIN_DIR/crashguard"
install -m755 "$BUILD_DIR/bin/explorer-killall" "$BIN_DIR/explorer-killall"
install -m755 "$SOURCE_DIR/config/explorer-guard.sh" "$BIN_DIR/explorer-guard.sh"
echo "[ok] 已安装 4 个二进制 + 守卫脚本到 $BIN_DIR"

# 路径含空格时，systemd Exec= 与桌面 Exec= 需要对完整命令加引号
Q=''
if [[ "$BIN_DIR" == *" "* ]]; then
    Q='"'
fi
FULL="${BIN_DIR//&/\\&}"

render() {
    sed -e "s|__BINDIR__/winlogin.exe|${Q}${FULL}/winlogin.exe${Q}|g" \
        -e "s|__BINDIR__/explorer-guard.sh|${Q}${FULL}/explorer-guard.sh${Q}|g" \
        -e "s|__BINDIR__|${Q}${FULL}${Q}|g" "$1" > "$2"
}

render "$SOURCE_DIR/config/explorer-linux.desktop" "$CFG_DIR/explorer-linux.desktop"
render "$SOURCE_DIR/config/explorer-winlogin.service" "$CFG_DIR/explorer-winlogin.service"
render "$SOURCE_DIR/config/explorer-guard.service" "$CFG_DIR/explorer-guard.service"
cp "$SOURCE_DIR/config/hyprland-dropin.conf" "$CFG_DIR/hyprland-dropin.conf"
cp "$SOURCE_DIR/config/niri-dropin.kdl"      "$CFG_DIR/niri-dropin.kdl"

if [[ "${XDG_CURRENT_DESKTOP:-}" =~ (KDE|GNOME|X-Cinnamon|XFCE|LXQt|MATE) ]]; then
    AUTOSTART_DIR="$HOME/.config/autostart"
    mkdir -p "$AUTOSTART_DIR"
    cp "$CFG_DIR/explorer-linux.desktop" "$AUTOSTART_DIR/explorer-linux.desktop"
    echo "[ok] XDG autostart 已放置: $AUTOSTART_DIR/explorer-linux.desktop"
fi

echo ""
echo "=== 环境集成 ==="
case "${XDG_CURRENT_DESKTOP:-}" in
    *Hyprland*)
        echo "检测到 Hyprland，请在 hyprland.conf 末尾加入:"
        echo "  source = $CFG_DIR/hyprland-dropin.conf"
        ;;
    *Niri*|*niri*)
        echo "检测到 Niri，请在 config.kdl 末尾加入:"
        echo "  include $CFG_DIR/niri-dropin.kdl"
        ;;
    *KDE*|*GNOME*|*XFCE*|*MATE*|*Cinnamon*|*LXQt*)
        echo "检测到桌面环境，autostart 已就绪；如需 systemd 强守护:"
        echo "  systemctl --user daemon-reload"
        echo "  systemctl --user enable --now explorer-winlogin.service"
        ;;
    *)
        echo "未识别的 XDG_CURRENT_DESKTOP='${XDG_CURRENT_DESKTOP:-空}'"
        echo "请手动选择：Hyprland/Niri 用上面的 drop-in；其他桌面用 autostart 或 systemd。"
        ;;
esac

if [ "$APPLY" = "yes" ]; then
    if command -v systemctl >/dev/null 2>&1; then
        mkdir -p "$HOME/.config/systemd/user"
        cp "$CFG_DIR/explorer-winlogin.service" "$HOME/.config/systemd/user/"
        cp "$CFG_DIR/explorer-guard.service"    "$HOME/.config/systemd/user/"
        systemctl --user daemon-reload
        systemctl --user enable --now explorer-winlogin.service
        echo "[ok] systemd 用户单元已启用: explorer-winlogin.service (Restart=always)"
        echo "[warn] 如需桌面阻断守卫（剪断即跳电），执行:"
        echo "       systemctl --user enable --now explorer-guard.service"
        echo "       并在单元里将 EXPLORER_SESSION_KILL 改为 1"
    else
        echo "[warn] 无 systemctl，跳过 systemd 集成。"
    fi
else
    echo ""
    echo "=== 逃生通道（请背下来）==="
    echo "  Shift+F10         强制清场（黑/白屏与正常状态均生效）"
    echo "  会话终结门控:      export EXPLORER_SESSION_KILL=1 后 explorer-killall --session"
    echo ""
    echo "  想先看 UI（无守护的开发模式）:"
    echo "  $SOURCE_DIR/scripts/build.sh $BUILD_DIR explorer"
    echo ""
    echo "下一步: ./scripts/install.sh apply   # 启用 systemd 强守护"
    echo "卸载:  explorer-killall --session 之后删掉上述生成文件即可"
fi