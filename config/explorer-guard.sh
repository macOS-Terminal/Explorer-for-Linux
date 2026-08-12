#!/bin/bash
#
# explorer-guard.sh — 启动阶段强绑定守卫
#
# 仅在 EXPLORER_SESSION_KILL=1 时生效：启动后 15 秒宽限期，若 winlogin.exe 与
# explorer.exe 双双失踪超过 5 分钟观测窗口，则直接终止当前用户会话（退回 TTY）。
# 默认 EXPLORER_SESSION_KILL=0，守卫只做监察不杀人。
# 逃生通道：Shift+F10 → explorer-killall --session（同样受该变量门控）。

BIND="${EXPLORER_SESSION_KILL:-0}"
if [ "$BIND" != "1" ]; then
    echo "explorer-guard: 监察模式（EXPLORER_SESSION_KILL=1 才启用会话终结）"
    exit 0
fi

SID="${XDG_SESSION_ID:-}"
[ -z "$SID" ] && exit 0

sleep 15

for ((i = 0; i < 10; i++)); do
    if ! pgrep -x winlogin.exe >/dev/null 2>&1 && ! pgrep -x explorer.exe >/dev/null 2>&1; then
        echo "explorer-guard: 链路断裂，终止会话 $SID（桌面被阻断）"
        loginctl terminate-session "$SID" 2>/dev/null
        exit 0
    fi
    sleep 30
done

echo "explorer-guard: 启动阶段观测完成，链路健康"
exit 0