#!/usr/bin/env bash
# Unidict 一键安装（Linux / macOS）
#
# 用法（一条命令）:
#   curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
#
# 从每日构建（GitHub Pages 固定直链，匿名可下）下载对应 OS+架构 的包安装:
#   Linux x64/arm64 -> unidict_cli_std 到 <PREFIX>/bin，dict.json 到 <PREFIX>/share/unidict
#   macOS arm64     -> unidict_gui.app 到 /Applications，cli 到 /usr/local/bin
#
# 幂等: 重跑即升级（覆盖安装）。
# 自定义前缀: UNIDICT_PREFIX=$HOME/.local curl ... | bash
# 说明: CLI 是查词工具无常驻服务语义，不做服务注册；GUI 启动验证见包内
#       PLATFORM-NOTES.txt。CLI 无 --version，安装后用冒烟命令验证:
#       UNIDICT_DICTS=dict.json unidict_cli_std hello
set -euo pipefail

REPO_URL="https://github.com/cuihairu/unidict"
# 滚动 nightly Release 直链：public repo 的 release asset 匿名可下
# （GitHub App 看不到 artifacts，Pages 下载页是备用镜像）
BASE_URL="${REPO_URL}/releases/download/nightly"

log() { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
err() { printf '\033[1;31m错误:\033[0m %s\n' "$*" >&2; exit 1; }

# ---------- OS ----------
case "$(uname -s)" in
  Linux) OS=linux ;;
  Darwin) OS=macos ;;
  *) err "不支持的操作系统: $(uname -s)（本脚本支持 Linux/macOS；Windows 请用 install.ps1:
       irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex" ;;
esac

# ---------- ARCH ----------
ARCH_RAW="$(uname -m)"
case "$ARCH_RAW" in
  x86_64|amd64)
    if [ "$OS" = linux ]; then PKG=unidict-linux-x64.zip
    else
      err "CPU 架构 ${ARCH_RAW}（Intel Mac）：每日构建未提供 macOS x86_64 包
       （Qt 6.10 起不再分发 Intel macOS 库）。请用 Apple Silicon 设备，或按
       README 从源码构建: ${REPO_URL}#构建"
    fi ;;
  arm64|aarch64)
    if [ "$OS" = macos ]; then PKG=unidict-macos-arm64.zip
    else PKG=unidict-linux-arm64.zip; fi ;;
  *)
    err "不支持的 CPU 架构: ${ARCH_RAW}（支持 x86_64 / arm64 / aarch64；当前矩阵
       未覆盖你的平台，可到 ${REPO_URL}/issues 提需求并附 uname -m 输出" ;;
esac

# ---------- 依赖 ----------
command -v curl >/dev/null 2>&1 || err "缺少 curl，请先安装"

# ---------- 安装方式：Linux 优先发行版系统包（deb/rpm），不可用回落 zip ----------
PKG_SYS=""
PKG_DL="$PKG"
if [ "$OS" = linux ]; then
  if command -v dpkg >/dev/null 2>&1; then
    PKG_SYS=deb; PKG_DL="${PKG%.zip}.deb"
  elif command -v rpm >/dev/null 2>&1; then
    PKG_SYS=rpm; PKG_DL="${PKG%.zip}.rpm"
  fi
fi
EXT="${PKG_SYS:-zip}"
if [ -z "$PKG_SYS" ]; then
  command -v unzip >/dev/null 2>&1 || err "缺少 unzip，请先安装（apt/dnf/pacman install unzip）"
fi

# ---------- 下载 ----------
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
log "下载 ${BASE_URL}/${PKG_DL} ..."
curl -fsSL --retry 3 --retry-delay 2 -o "$TMP/pkg.$EXT" "${BASE_URL}/${PKG_DL}" \
  || err "下载失败。排查: 1) 网络可达 ${BASE_URL} 2) nightly Release 是否已发布（每天 05:17 北京时间自动更新；若尚未发布可到 ${REPO_URL}/actions 手动触发 Daily Build）"
if [ -z "$PKG_SYS" ]; then
  unzip -q -o "$TMP/pkg.zip" -d "$TMP/pkg" || err "解包失败（包损坏？重试或到 ${REPO_URL}/actions 手动下载）"
fi

# ---------- 安装 ----------
# sudo: 无写权限时非交互探测 sudo -n；管道安装下无法交互输密码，给明确指引
as_root() {
  if [ "$(id -u)" = 0 ]; then "$@"
  elif sudo -n true 2>/dev/null; then sudo -n "$@"
  else
    err "无写权限且 sudo 不可用。二选一重跑:
       1) curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | sudo bash
       2) UNIDICT_PREFIX=\$HOME/.local curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash"
  fi
}
# 按需提权：先按当前身份执行（用户前缀 / macOS admin 组可写目录不需要
# root），失败才走 as_root（sudo 免密直通，否则给明确指引退出）
maybe_root() {
  "$@" 2>/dev/null || as_root "$@"
}

if [ "$OS" = linux ] && [ -n "$PKG_SYS" ]; then
  # 系统包路径：装到 /usr，交包管理器统一管理（升级重跑本脚本，卸载见下）
  F="$TMP/pkg.$PKG_SYS"
  if [ "$PKG_SYS" = deb ]; then
    if command -v apt-get >/dev/null 2>&1; then
      maybe_root apt-get install -y "$F"
    else
      maybe_root dpkg -i "$F"
    fi
  else
    if command -v dnf >/dev/null 2>&1; then maybe_root dnf install -y "$F"
    elif command -v yum >/dev/null 2>&1; then maybe_root yum install -y "$F"
    elif command -v zypper >/dev/null 2>&1; then maybe_root zypper --non-interactive install "$F"
    else maybe_root rpm -U --replacepkgs "$F"
    fi
  fi
  [ -x /usr/bin/unidict_cli_std ] || err "系统包安装后未找到 /usr/bin/unidict_cli_std"
  log "已安装系统包 unidict: /usr/bin/unidict_cli_std + /usr/share/unidict/dict.json"
  UNINSTALL_HINT="$( [ "$PKG_SYS" = deb ] && echo 'sudo apt remove unidict' || echo 'sudo dnf remove unidict' )"
  INSTALLED_BIN=/usr/bin/unidict_cli_std
  INSTALLED_DICT=/usr/share/unidict/dict.json
elif [ "$OS" = linux ]; then
  PREFIX="${UNIDICT_PREFIX:-/usr/local}"
  BIN_DIR="$PREFIX/bin"
  SHARE_DIR="$PREFIX/share/unidict"
  # 按需提权：目标可写（如 UNIDICT_PREFIX=$HOME/...）就不碰 sudo
  maybe_root mkdir -p "$BIN_DIR" "$SHARE_DIR"
  [ -f "$TMP/pkg/unidict_cli_std" ] || err "包内缺 unidict_cli_std（包不完整？）"
  maybe_root install -m 755 "$TMP/pkg/unidict_cli_std" "$BIN_DIR/unidict_cli_std"
  maybe_root install -m 644 "$TMP/pkg/dict.json" "$SHARE_DIR/dict.json" 2>/dev/null || true
  maybe_root install -m 644 "$TMP/pkg/PLATFORM-NOTES.txt" "$SHARE_DIR/" 2>/dev/null || true
  log "已安装 unidict_cli_std -> ${BIN_DIR}（示例词典: ${SHARE_DIR}/dict.json）"
  INSTALLED_BIN="$BIN_DIR/unidict_cli_std"
  INSTALLED_DICT="$SHARE_DIR/dict.json"
  UNINSTALL_HINT="rm -f ${BIN_DIR}/unidict_cli_std ${SHARE_DIR}/dict.json"
else
  APP_SRC="$TMP/pkg/unidict_qml.app"
  [ -d "$APP_SRC" ] || err "包内缺 unidict_qml.app（包不完整？）"
  maybe_root mkdir -p /Applications
  maybe_root rm -rf /Applications/unidict_qml.app
  maybe_root cp -R "$APP_SRC" /Applications/
  # curl 下载一般无 quarantine，但浏览器中转可能带上；一并清掉保险
  sudo -n xattr -cr /Applications/unidict_qml.app 2>/dev/null || xattr -cr /Applications/unidict_qml.app 2>/dev/null || true
  if [ -f "$TMP/pkg/unidict_cli_std" ]; then
    maybe_root mkdir -p /usr/local/bin
    maybe_root install -m 755 "$TMP/pkg/unidict_cli_std" /usr/local/bin/unidict_cli_std
    maybe_root install -m 755 "$TMP/pkg/unidict_cli" /usr/local/bin/unidict_cli 2>/dev/null || true
    INSTALLED_BIN="/usr/local/bin/unidict_cli_std"
  fi
  mkdir -p "$HOME/.unidict" && install -m 644 "$TMP/pkg/dict.json" "$HOME/.unidict/dict.json" 2>/dev/null || true
  log "已安装 unidict_qml.app -> /Applications（CLI: ${INSTALLED_BIN:-未在包内}）"
  INSTALLED_DICT="$HOME/.unidict/dict.json"
  UNINSTALL_HINT="rm -rf /Applications/unidict_qml.app ${INSTALLED_BIN:-} $HOME/.unidict/dict.json"
fi

# ---------- 冒烟验证 ----------
if [ -n "${INSTALLED_BIN:-}" ] && [ -f "$INSTALLED_BIN" ]; then
  log "冒烟验证: UNIDICT_DICTS=dict.json unidict_cli_std hello"
  if OUT="$(UNIDICT_DICTS="$INSTALLED_DICT" "$INSTALLED_BIN" hello 2>&1)" && printf '%s' "$OUT" | grep -qi "greeting"; then
    log "验证通过: $OUT"
  else
    err "冒烟验证未通过（输出: ${OUT:-空}）。请带着输出到 ${REPO_URL}/issues 反馈"
  fi
fi

log "完成。卸载: ${UNINSTALL_HINT:-}"
