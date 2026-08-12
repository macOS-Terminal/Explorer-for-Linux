# Explorer for Linux

在 Linux 上深度复刻 Windows 11 资源管理器的极客项目，还原其经典的"未响应"体验。

> 注意：本项目包含崩溃接管与强制退出功能。默认配置不会破坏任何东西，但打开
> EXPLORER_SESSION_KILL=1 后它会真的终止你的用户会话。只装在你自己的机器上，
> 装之前备份配置，并背下 **Shift+F10**。

## 架构

```
winlogin.exe (守护)                explorer.exe (主程序)
  ├─ Supervisor: QProcess 监护        ├─ Windows 11 风格 UI (QSS 深浅双套)
  │   崩溃/FailedToStart → 接管        │   ├─ 标题栏: 图标 + 标题 + 最小化/最大化/✕
  ├─ CrashScreen                      │   ├─ 命令栏: 后退/前进/向上 + 地址栏
  │   ├─ Wayland → crashguard          │   │   + 面包屑 + 搜索框
  │   │   (纯 C layer-shell 黑/白屏    │   ├─ 此电脑: 挂载点 → C:/D:/E: 盘符
  │   │    全屏接管 + 按键)            │   ├─ 侧边栏: 主页/下载/文档/图片/
  │   └─ X11     → 全屏 Qt 窗(兜底)    │   │   OneDrive/此电脑/回收站
  ├─ KeyBackend:X11 XGrabKey 全局抓     │   ├─ 文件列表: QFileSystemModel
  │    Shift+F10 / Super+E             │   └─ 未响应套壳: 点击→套壳→递归
  │   SIGUSR1 → 与 explorer-killall 联动│       深度≥2 出「关闭程序」→ SIGSEGV
  └─ 托盘图标                         └─ ⚠ 关不掉: ✕ = 未响应套壳起点

crashguard (纯 C, ~0 依赖)
  - layer-shell: top层黑/白屏 + 键盘 exclusive
  - Super+E → stdout 通知 winlogin 复活 explorer, 自身降 background 层(黑屏不退)
  - Shift+F10 → 直接 exec explorer-killall --session（最后的逃生）
  - PDEATHSIG: 父进程死亡即自灭

explorer-killall
  - 按 comm 精确 SIGKILL winlogin.exe / explorer.exe / crashguard
  - --session: 另 terminate-session 退回 TTY（EXPLORER_SESSION_KILL=1 门控）
```

## 构建（任意位置均可）

项目与构建目录完全解耦：可以把源码放在任何文件夹、在任何目录执行构建，产物自包含
（各二进制运行时只依赖**自身所在目录**寻找同伴，不写死任何绝对路径）。

```bash
./scripts/build.sh                # 就地构建（CWD 无关）
./scripts/build.sh /tmp/out       # out-of-tree，构建目录任意
./scripts/build.sh build run      # 构建后直接前台跑 winlogin.exe（Ctrl+C 退出）
./scripts/build.sh build explorer # 构建后前台直跑 explorer.exe（无守护的开发模式）

# 或传统方式：
cmake -B <构建目录> -S <项目根>
cmake --build <构建目录> -j
cmake --install <构建目录> --prefix ~/.local    # 内置 install 规则
```

产物：`<构建目录>/bin/{winlogin.exe, explorer.exe, crashguard, explorer-killall}`

依赖：Qt 6 Widgets、libxcb(+keysyms)、wayland-client（可选，缺失自动降级）。
如果你的 Qt 不带 layer-shell 私有头也没关系——crashguard 自带协议生成器。

## 安装 / 卸载

```bash
./scripts/install.sh              # 仅生成配置（安全预览）
./scripts/install.sh apply        # 启用 systemd 强守护 (Restart=always)
./scripts/install.sh --build-dir <构建目录>   # 指定构建目录
# 卸载: 删除 ~/.local/bin/{winlogin*,explorer*,crashguard*,explorer-killall*}、
#        ~/.config/explorer-linux、autostart 条目、systemd 单元
```

安装器同样 CWD/调用位置无关，二进制目录含空格时自动为 systemd/desktop 引号转义。

## 使用方法

### 启动

- **正式（守护模式）**：由 `winlogin.exe` 拉起 explorer.exe 并监护——正常退出会被静默
  重生，崩溃由黑/白屏接管。装好后的桌面 autostart / systemd 也会自动走这条路。
- **开发模式**：直接运行 `explorer.exe`（`EXPLORER_SUPERVISED` 为空时，「关闭程序」
  只清弹层、不 SIGSEGV，可安全调试）。

### 界面速览

| 区域 | 说明 |
|---|---|
| 标题栏 | 图标 + 当前页标题；右侧最小化 / 最大化 / ✕（✕ 会触发未响应套壳） |
| 命令栏 | ◂ 后退 / ▸ 前进 / ▲ 向上；可编辑地址栏（回车导航）；面包屑点击直达 |
| 剪贴路径 | 地址栏输入 Linux 路径（如 `/home/你`）回车即跳转 |
| 搜索框 | 回车 → 状态栏"正在搜索…"卡 1.6 秒 → 60% 概率未响应（经典手法） |
| 侧边栏 | 主页 / 下载 / 文档 / 图片 / OneDrive / 此电脑 / 回收站，单击直达 |
| 此电脑页 | 盘符卡片（`本地磁盘 (C:)`…）+ 标准文件夹；**双击**打开 |
| 文件视图 | 树形列表（名称 / 大小 / 类型 / 修改日期），双击文件夹进入 |
| 状态栏 | 就绪 / 正在计算项目大小… / 找不到路径… 等提示 |

### 快捷键

| 按键 | 行为 |
|---|---|
| `Alt+←` / `Alt+→` | 后退 / 前进（地址栏/面包屑 → 也可点按钮） |
| 回车 | 地址栏中导航；搜索框中触发"搜索" |
| 双击 | 此电脑盘符 / 文件夹卡片 / 文件列表目录 进入 |
| `Shift+F10` | 强制清场（黑/白屏时由图层直接捕获，正常运行由 XGrabKey / drop-in bind） |
| `Super+E` | 崩溃接管后复活 explorer.exe，黑/白屏降级 |
| 点击未响应框 | 套娃 +1；深度≥2 出现「关闭程序」→ 真 SIGSEGV → winlogin 接管 |
| ✕ 关闭窗口  | 等效于触发未响应套壳（关不掉） |

### 未响应机制速览

- 关闭窗口 / 关闭最后一个弹层：等效于触发未响应。
- 大目录陷阱：浏览 `/` 或 >3000 项的目录会假装"正在计算项目大小…"卡 2.2 秒；
  另有 12% 随机卡顿和每 15 次导航一次的彩蛋。
- 搜索框 60% 概率在 1.6 秒"搜索"后套壳。
- 全部弹层均可点掉，且始终有耐心按钮——玩的就是桥段。

## 行为细节

- 黑/白屏颜色：由 QPalette 亮度判定（Qt 平台主题 → GTK/KDE 配置兜底），暗色主题=黑屏。
- 盘符映射：`/proc/self/mountinfo` 排除伪文件系统与 fuse 后按挂载点深度排序，根目录=C:。
- 按键捕获（Wayland）：crashguard 以 layer-shell `keyboard-interactivity=exclusive`
  独占键盘，直接吃下 Super+E / Shift+F10（evdev 键码）。
- "强绑定"守卫（explorer-guard.service，opt-in）：启动 15s 宽限后观测 5 分钟，
  两进程双双失踪 → terminate-session 退回 TTY。
- GNOME(Wayland) 无 wlr-layer-shell：崩溃接管降级为普通全屏 Qt 窗（需 crashguard
  probe 通过时才启用 layer-shell，否则自动降级）。
- 开发模式：以 `EXPLORER_SUPERVISED` 为空运行 explorer.exe 时，「关闭程序」只清弹层不崩溃。

## 已知边界

1. Wayland 世界没有通用的全局快捷键：配好合成器绑定（仓库已提供 Hyprland / Niri
   drop-in）。
2. 崩溃状态下按键由 crashguard 独占键盘捕获，与合成器无关，全平台可靠。
3. 黑/白屏在 wlroots 系合成器上通过 background 层持久(即"永不恢复")；
   X11 上通过 hide() 模拟。