#!/usr/bin/env bash
# unidict-relay 一键安装（Linux / macOS）
#
# 从每日构建（nightly release，固定 tag `nightly`，匿名可直链下载）拉取与本机
# OS/架构匹配的 unidict-relay 并安装，可选注册开机自启服务（systemd / launchd）。
#
# 用法:
#   # Linux / macOS 一键（自动检测 OS 与架构）:
#   curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
#
#   # 带参数（注册开机自启服务）:
#   curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | \
#     bash -s -- --with-service --bind 127.0.0.1:8788
#
#   # 静默安装（零交互）:
#   ./install.sh --silent --bind 127.0.0.1:8788
#
#   # 本地执行:
#   ./install.sh [--with-service] [--bind HOST:PORT] [选项]
#
# 幂等：重跑即升级（覆盖二进制；已注册服务则自动重启加载新二进制）。
# 兼容 macOS 自带 bash 3.2（无关联数组/大小写展开等 4.0 特性）。
set -euo pipefail

REPO_DEFAULT="cuihairu/unidict"
RELEASE_TAG_DEFAULT="nightly"
LABEL="com.cuihairu.unidict-relay"
UNIT_NAME="unidict-relay.service"

info() { printf '%s\n' "$*"; }
warn() { printf '[警告] %s\n' "$*" >&2; }
die()  { printf '错误: %s\n' "$*" >&2; exit 1; }

usage() {
	cat <<'EOF'
unidict-relay 一键安装（Linux / macOS）

选项:
  --install-dir DIR   安装目录（默认 /usr/local/bin，无写权限时 ~/.local/bin）
  --with-service      注册开机自启服务（Linux systemd / macOS launchd）
  --bind HOST:PORT    监听地址（默认 127.0.0.1:8788，仅注册服务时写入配置；
                      HOST 须为 IP——LAN 内其他设备访问用 0.0.0.0）
  --silent            兼容旗标：本脚本全程零交互（无提示可静默）
  -h, --help          显示本帮助

安全口径: unidict-relay 无内置认证（中转只见密文，协议面天然抗窃读），
默认绑定回环地址；暴露到公网请自行加反向代理鉴权。

环境变量（curl | bash 管道形态无法传参时使用）:
  UNIDICT_WITH_SERVICE=1  等价 --with-service
  UNIDICT_SILENT=1        等价 --silent
  UNIDICT_BIND            等价 --bind
  UNIDICT_INSTALL_DIR
  UNIDICT_REPO（默认 cuihairu/unidict）
  UNIDICT_RELEASE（默认 nightly）

重跑即升级（幂等）。安装完成后自动启动临时实例做探活验证
（GET /api/sync/relay/ping），通过后即关闭。
EOF
}

# ---------- 参数与环境变量 ----------
INSTALL_DIR="${UNIDICT_INSTALL_DIR:-}"
WITH_SERVICE="${UNIDICT_WITH_SERVICE:-0}"
BIND_ADDR="${UNIDICT_BIND:-}"
REPO="${UNIDICT_REPO:-$REPO_DEFAULT}"
RELEASE_TAG="${UNIDICT_RELEASE:-$RELEASE_TAG_DEFAULT}"

while [ $# -gt 0 ]; do
	case "$1" in
	--install-dir)
		[ $# -ge 2 ] || die "--install-dir 需要一个目录参数"
		INSTALL_DIR="$2"
		shift 2
		;;
	--with-service) WITH_SERVICE=1; shift ;;
	# --silent：保留旗标兼容，脚本本身全程零交互
	--silent) shift ;;
	--bind)
		[ $# -ge 2 ] || die "--bind 需要一个 HOST:PORT 参数"
		BIND_ADDR="$2"
		shift 2
		;;
	-h | --help)
		usage
		exit 0
		;;
	*)
		die "未知参数: $1（-h 查看用法）"
		;;
	esac
done

# ---------- OS / 架构检测 ----------
OS_RAW="$(uname -s)"
ARCH_RAW="$(uname -m)"

case "$OS_RAW" in
Linux) OS="linux" ;;
Darwin) OS="macos" ;;
*)
	die "不支持的操作系统: $OS_RAW —— 本脚本支持 Linux 与 macOS；Windows 请用 install.ps1"
	;;
esac

case "$ARCH_RAW" in
x86_64 | amd64) ARCH="x64" ;;
aarch64 | arm64) ARCH="arm64" ;;
armv7l | armv8l | armhf | arm)
	die "不支持的架构: $ARCH_RAW（armv7）—— nightly 未提供 armv7 产物"
	;;
armv6l)
	die "不支持的架构: $ARCH_RAW（armv6，如树莓派 Zero/1）—— nightly 未提供 armv6 产物"
	;;
i386 | i486 | i586 | i686 | x86)
	die "不支持的架构: $ARCH_RAW（32 位 x86）—— nightly 未提供 386 产物"
	;;
mips | mipsel | mips64*)
	die "检测到 MIPS 架构（$ARCH_RAW）：通用 nightly 未提供 MIPS 产物"
	;;
*)
	die "不支持的架构: $ARCH_RAW —— 已支持: x86_64、aarch64/arm64"
	;;
esac

# nightly 资产名与 daily-build 平台矩阵对齐: linux-x64 / linux-arm64 / macos-arm64
[ "$OS" = "macos" ] && [ "$ARCH" = "x64" ] &&
	die "nightly 未提供 macOS x64 产物（仅 Apple Silicon macos-arm64）"
PKG_NAME="unidict-${OS}-${ARCH}.zip"
URL="https://github.com/${REPO}/releases/download/${RELEASE_TAG}/${PKG_NAME}"

info "unidict-relay 一键安装"
info "  系统: ${OS} (${OS_RAW})"
info "  架构: ${ARCH} (${ARCH_RAW})"
info "  来源: ${URL}"
info ""

# ---------- root 提权辅助（非交互：只接受免密 sudo） ----------
as_root() {
	if [ "$(id -u)" -eq 0 ]; then
		"$@"
	elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
		sudo -n "$@"
	else
		return 127
	fi
}

# ---------- 下载工具 ----------
fetch() {
	# fetch <url> <输出文件>
	if command -v curl >/dev/null 2>&1; then
		curl -fsSL --retry 3 --connect-timeout 15 -o "$2" "$1"
	elif command -v wget >/dev/null 2>&1; then
		wget -q --tries=3 -O "$2" "$1"
	else
		die "需要 curl 或 wget 之一来下载产物，请先安装"
	fi
}

TMPDIR_DL="$(mktemp -d)"
RELAY_PID=""
cleanup() {
	[ -n "$RELAY_PID" ] && kill "$RELAY_PID" 2>/dev/null || true
	rm -rf "$TMPDIR_DL"
}
trap cleanup EXIT

info "下载 nightly 产物..."
if ! fetch "$URL" "$TMPDIR_DL/pkg.zip"; then
	die "下载失败: $URL
  - 404：该平台（${PKG_NAME}）的 nightly 产物可能尚未生成——每日构建在近 24h 有提交时于 00:00 UTC 重建；
  - 网络问题：请检查代理或稍后重试。"
fi

# 平台 zip 整包收 CLI+relay+示例词典（B5 起 relay 随包），解包取 unidict-relay
mkdir -p "$TMPDIR_DL/pkg"
if command -v unzip >/dev/null 2>&1; then
	unzip -q "$TMPDIR_DL/pkg.zip" -d "$TMPDIR_DL/pkg"
elif command -v python3 >/dev/null 2>&1; then
	python3 -c 'import zipfile, sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
		"$TMPDIR_DL/pkg.zip" "$TMPDIR_DL/pkg"
else
	die "解包需要 unzip 或 python3 之一，请先安装"
fi
[ -f "$TMPDIR_DL/pkg/unidict-relay" ] ||
	die "解包异常：${PKG_NAME} 内未找到 unidict-relay（nightly 产物是否过旧？每日构建 00:00 UTC 重建）"

# ---------- 安装目录决策 ----------
if [ -z "$INSTALL_DIR" ]; then
	if [ "$(id -u)" -eq 0 ]; then
		INSTALL_DIR="/usr/local/bin"
	elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
		INSTALL_DIR="/usr/local/bin"
	else
		INSTALL_DIR="${HOME}/.local/bin"
	fi
fi
mkdir -p "$INSTALL_DIR" 2>/dev/null || as_root mkdir -p "$INSTALL_DIR" ||
	die "无法创建安装目录: $INSTALL_DIR"

if [ -w "$INSTALL_DIR" ]; then
	install -m 0755 "$TMPDIR_DL/pkg/unidict-relay" "$INSTALL_DIR/unidict-relay"
else
	as_root install -m 0755 "$TMPDIR_DL/pkg/unidict-relay" "$INSTALL_DIR/unidict-relay" ||
		die "安装目录不可写且无可用的免密 sudo: $INSTALL_DIR（可用 --install-dir 指定其他目录）"
fi

BIN_PATH="$INSTALL_DIR/unidict-relay"

# ---------- 安装后验证：起临时实例做探活（无 --data，纯内存） ----------
# 二进制没有 --version 旗标，探活 GET /api/sync/relay/ping 才是真实健康口径
verify_relay() {
	local port pong ok
	# 本脚本只做安装验证：随机高位端口起临时实例，探活通过即关
	port="$((RANDOM % 20000 + 30000))"
	"$BIN_PATH" --host 127.0.0.1 --port "$port" >/dev/null 2>&1 &
	RELAY_PID=$!
	pong=""
	ok=0
	for _ in 1 2 3 4 5 6 7 8 9 10; do
		pong="$(curl -fsSL "http://127.0.0.1:${port}/api/sync/relay/ping" 2>/dev/null ||
			wget -qO- "http://127.0.0.1:${port}/api/sync/relay/ping" 2>/dev/null)" &&
			case "$pong" in
			*unidict-sync-relay*) ok=1; break ;;
			esac
		kill -0 "$RELAY_PID" 2>/dev/null || break  # 进程已退（bind 失败等），别空等
		sleep 1
	done
	kill "$RELAY_PID" 2>/dev/null || true
	wait "$RELAY_PID" 2>/dev/null || true
	RELAY_PID=""
	[ "$ok" = "1" ] ||
		die "安装后验证失败：unidict-relay 未在 127.0.0.1:${port} 应答探活
  （挂载点是否 noexec？防火墙是否拦回环？重试: ${BIN_PATH} --host 127.0.0.1 --port 8788）"
	info "已安装并通过探活: ${pong} -> $BIN_PATH"
}
verify_relay

case ":$PATH:" in
*":$INSTALL_DIR:"*) ;;
*)
	warn "$INSTALL_DIR 不在当前 PATH 中——请加入 PATH 后再直接使用 unidict-relay 命令"
	;;
esac

# ---------- 配置文件路径 ----------
CONFIG_DIR_SYSTEM="/etc/unidict-relay"
CONFIG_DIR_USER="${XDG_CONFIG_HOME:-$HOME/.config}/unidict-relay"
ENV_FILE_SYSTEM="${CONFIG_DIR_SYSTEM}/env"
ENV_FILE_USER="${CONFIG_DIR_USER}/env"

# --bind HOST:PORT 拆成二进制的 --host <IP> --port <int>（二进制只认独立旗标）
split_bind() {
	CONF_HOST="${BIND_ADDR%%:*}"
	CONF_PORT="${BIND_ADDR##*:}"
	[ "$CONF_HOST" = "$BIND_ADDR" ] && die "--bind 需要 HOST:PORT 形态: $BIND_ADDR"
	case "$CONF_HOST" in
	'' | *[!0-9.]*)
		# 域名放宽给手动运行；服务配置里二进制 inet_pton 只认 IP
		warn "监听地址 $CONF_HOST 不是 IP——unidict-relay 只接受 IP（0.0.0.0/127.0.0.1 等）"
		;;
	esac
	case "$CONF_PORT" in
	'' | *[!0-9]*) die "--bind 端口须为数字: $BIND_ADDR" ;;
	esac
}

write_env_file() {
	# $1 = 目标 env 文件；未显式给 --bind 时落默认值（回环 + 二进制默认端口）
	local tmp
	tmp="$(mktemp)"
	[ -n "$BIND_ADDR" ] || BIND_ADDR="127.0.0.1:8788"
	split_bind
	{
		printf '# unidict-relay 服务配置（install.sh 生成/更新）\n'
		printf 'HOST=%s\n' "$CONF_HOST"
		printf 'PORT=%s\n' "$CONF_PORT"
	} >"$tmp"
	chmod 600 "$tmp"
	mkdir -p "$(dirname "$1")" 2>/dev/null || as_root mkdir -p "$(dirname "$1")" ||
		die "无法创建配置目录: $(dirname "$1")"
	if [ -w "$(dirname "$1")" ]; then
		mv "$tmp" "$1"
	else
		as_root mv "$tmp" "$1" || die "无法写入配置文件: $1"
	fi
	as_root chown root:root "$1" 2>/dev/null || true
}

# systemd 单元
SYSTEMD_UNIT_SYSTEM="/etc/systemd/system/${UNIT_NAME}"
SYSTEMD_UNIT_USER="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/${UNIT_NAME}"

xml_escape() {
	printf '%s' "$1" | sed -e 's/&/\&/g' -e 's/</\</g' -e 's/>/\>/g'
}

write_systemd_unit() {
	# $1 = system|user, $2 = unit 路径
	if [ "$1" = "system" ]; then
		as_root tee "$2" >/dev/null <<UNIT
[Unit]
Description=Unidict Relay - Local Sync Relay Server
Documentation=https://github.com/cuihairu/unidict
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=unidict-relay
Group=unidict-relay
EnvironmentFile=${ENV_FILE_SYSTEM}
ExecStart=${BIN_PATH} --host \${HOST} --port \${PORT}
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=unidict-relay
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/unidict-relay
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
UNIT
	else
		mkdir -p "$(dirname "$2")"
		tee "$2" >/dev/null <<UNIT
[Unit]
Description=Unidict Relay - Local Sync Relay Server
Documentation=https://github.com/cuihairu/unidict

[Service]
Type=simple
EnvironmentFile=${ENV_FILE_USER}
ExecStart=${BIN_PATH} --host \${HOST} --port \${PORT}
Restart=always
RestartSec=5
NoNewPrivileges=true

[Install]
WantedBy=default.target
UNIT
	fi
}

register_systemd() {
	if [ "$(id -u)" -eq 0 ] || as_root true 2>/dev/null; then
		# ---- 系统级：unidict-relay 专用用户 + /etc/unidict-relay/env + 系统单元 ----
		# 系统单元沙箱（PrivateTmp/ProtectHome）看不见 /tmp 与 $HOME 下的二进制，
		# 装那里必 203/EXEC 崩溃循环——注册前 fail-fast 给出路
		case "$BIN_PATH" in
		/tmp/* | "$HOME"/*)
			die "系统级服务沙箱看不见 $BIN_PATH（/tmp 与家目录被隔离）
  ——请用默认安装路径（/usr/local/bin）或 --install-dir 指到系统路径后重跑"
			;;
		esac
		if ! id unidict-relay >/dev/null 2>&1; then
			NOLOGIN="$(command -v nologin || echo /usr/sbin/nologin)"
			as_root useradd --system --user-group --home-dir /var/lib/unidict-relay \
				--shell "$NOLOGIN" unidict-relay ||
				die "创建系统用户 unidict-relay 失败"
		fi
		as_root mkdir -p /var/lib/unidict-relay
		as_root chown unidict-relay:unidict-relay /var/lib/unidict-relay
		write_env_file "$ENV_FILE_SYSTEM"
		write_systemd_unit system "$SYSTEMD_UNIT_SYSTEM"
		as_root systemctl daemon-reload ||
			die "systemctl daemon-reload 失败"
		as_root systemctl enable --now unidict-relay ||
			die "服务启动失败——查看日志: journalctl -u unidict-relay -n 20 --no-pager"
		sleep 1
		svc_state="$(systemctl is-active unidict-relay 2>/dev/null || true)"
		if [ "$svc_state" = "active" ]; then
			info "服务已注册并启动: ${SYSTEMD_UNIT_SYSTEM}（active，开机自启）"
		else
			warn "服务已注册但状态为 ${svc_state:-unknown}——查看: journalctl -u unidict-relay -n 20 --no-pager"
		fi
	else
		# ---- 用户级：~/.config systemd user 单元 ----
		command -v systemctl >/dev/null 2>&1 ||
			die "未找到 systemd，无法注册服务"
		systemctl --user show >/dev/null 2>&1 ||
			die "无法访问用户级 systemd 会话（非登录环境？）——请改用 sudo 运行本脚本注册系统级服务"
		write_env_file "$ENV_FILE_USER"
		write_systemd_unit user "$SYSTEMD_UNIT_USER"
		systemctl --user daemon-reload || die "systemctl --user daemon-reload 失败"
		systemctl --user enable --now unidict-relay ||
			die "用户级服务启动失败——查看: journalctl --user -u unidict-relay -n 20 --no-pager"
		sleep 1
		svc_state="$(systemctl --user is-active unidict-relay 2>/dev/null || true)"
		if [ "$svc_state" = "active" ]; then
			info "用户级服务已注册并启动: ${SYSTEMD_UNIT_USER}（active）"
			warn "如需未登录时也随开机运行: loginctl enable-linger $USER"
		else
			warn "用户级服务已注册但状态为 ${svc_state:-unknown}——查看: journalctl --user -u unidict-relay -n 20 --no-pager"
		fi
	fi
}

# launchd plist
LAUNCHD_PLIST_SYSTEM="/Library/LaunchDaemons/${LABEL}.plist"
LAUNCHD_PLIST_USER="$HOME/Library/LaunchAgents/${LABEL}.plist"

write_launchd_plist() {
	# $1 = daemon|agent, $2 = plist 路径
	local args log_dir
	[ -n "$BIND_ADDR" ] || BIND_ADDR="127.0.0.1:8788"
	split_bind
	args="<string>${BIN_PATH}</string>
      <string>--host</string><string>$(xml_escape "$CONF_HOST")</string>
      <string>--port</string><string>$(xml_escape "$CONF_PORT")</string>"

	if [ "$1" = "daemon" ]; then
		log_dir="/var/log"
	else
		log_dir="${HOME}/.unidict-relay"
		mkdir -p "$log_dir"
	fi
	{
		printf '<?xml version="1.0" encoding="UTF-8"?>\n'
		printf '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n'
		printf '<plist version="1.0">\n<dict>\n'
		printf '  <key>Label</key><string>%s</string>\n' "$LABEL"
		printf '  <key>ProgramArguments</key>\n  <array>\n      %s\n  </array>\n' "$args"
		printf '  <key>RunAtLoad</key><true/>\n'
		printf '  <key>KeepAlive</key><true/>\n'
		printf '  <key>ThrottleInterval</key><integer>10</integer>\n'
		printf '  <key>StandardOutPath</key><string>%s/unidict-relay.log</string>\n' "$log_dir"
		printf '  <key>StandardErrorPath</key><string>%s/unidict-relay.err.log</string>\n' "$log_dir"
		printf '</dict>\n</plist>\n'
	} >"$2"
}

register_launchd() {
	if [ "$(id -u)" -eq 0 ]; then
		PLIST="$LAUNCHD_PLIST_SYSTEM"
		DOMAIN="system"
		KIND="daemon"
	else
		PLIST="$LAUNCHD_PLIST_USER"
		DOMAIN="gui/$(id -u)"
		KIND="agent"
	fi
	command -v launchctl >/dev/null 2>&1 || die "未找到 launchctl，无法注册服务"
	mkdir -p "$(dirname "$PLIST")"
	if [ ! -f "$PLIST" ] || [ -n "$BIND_ADDR" ]; then
		write_launchd_plist "$KIND" "$PLIST"
	fi
	chmod 644 "$PLIST" 2>/dev/null || true
	launchctl bootout "$DOMAIN/$LABEL" >/dev/null 2>&1 || true
	launchctl bootstrap "$DOMAIN" "$PLIST" ||
		die "launchctl bootstrap 失败: $PLIST"
	launchctl enable "$DOMAIN/$LABEL" || true
	launchctl kickstart -k "$DOMAIN/$LABEL" >/dev/null 2>&1 ||
		warn "launchctl kickstart 未成功——查看日志: $PLIST 同目录对应 err.log"
	info "launchd 服务已注册: $PLIST（RunAtLoad + KeepAlive）"
}

if [ "$WITH_SERVICE" = "1" ]; then
	case "$OS" in
	linux)
		command -v systemctl >/dev/null 2>&1 ||
			die "Linux 上注册服务需要 systemd（未找到 systemctl）"
		register_systemd
		;;
	macos)
		register_launchd
		;;
	*)
		die "内部错误: 未知 OS $OS（服务注册未执行）"
		;;
	esac
else
	# 未要求注册服务：显式传了参数则更新既有服务的配置，随后重启加载新二进制
	# （重跑=升级/改配置，幂等）
	if [ -f "$SYSTEMD_UNIT_SYSTEM" ] && as_root systemctl is-enabled unidict-relay >/dev/null 2>&1; then
		if [ -n "$BIND_ADDR" ]; then
			write_env_file "$ENV_FILE_SYSTEM"
			info "已更新服务连接配置: $ENV_FILE_SYSTEM"
		fi
		if as_root systemctl restart unidict-relay; then
			info "已检测到既有系统服务，已重启加载新版本"
		else
			warn "既有系统服务重启失败——查看: journalctl -u unidict-relay -n 20 --no-pager"
		fi
	elif [ -f "$SYSTEMD_UNIT_USER" ] && systemctl --user is-enabled unidict-relay >/dev/null 2>&1; then
		if [ -n "$BIND_ADDR" ]; then
			write_env_file "$ENV_FILE_USER"
			info "已更新服务连接配置: $ENV_FILE_USER"
		fi
		if systemctl --user restart unidict-relay; then
			info "已检测到既有用户级服务，已重启加载新版本"
		else
			warn "既有用户级服务重启失败"
		fi
	elif [ -f "$LAUNCHD_PLIST_SYSTEM" ] && [ "$(id -u)" -eq 0 ] && command -v launchctl >/dev/null 2>&1; then
		if [ -n "$BIND_ADDR" ]; then
			register_launchd
		else
			launchctl kickstart -k "system/$LABEL" >/dev/null 2>&1 &&
				info "已检测到既有 launchd 服务，已重启加载新版本"
		fi
	elif [ -f "$LAUNCHD_PLIST_USER" ] && command -v launchctl >/dev/null 2>&1; then
		if [ -n "$BIND_ADDR" ]; then
			register_launchd
		else
			launchctl kickstart -k "gui/$(id -u)/$LABEL" >/dev/null 2>&1 &&
				info "已检测到既有 launchd 服务，已重启加载新版本"
		fi
	elif [ -n "$BIND_ADDR" ]; then
		# 无任何既有服务：配置写入用户级（装机时落盘）
		write_env_file "$ENV_FILE_USER"
		info "连接配置已写入: $ENV_FILE_USER"
	fi
fi

info ""
info "完成。下一步:"
if [ "$WITH_SERVICE" != "1" ]; then
	info "  注册开机自启服务: ./install.sh --with-service --bind 127.0.0.1:8788"
	if [ -n "$BIND_ADDR" ]; then
		split_bind
		info "  或手动启动: unidict-relay --host $CONF_HOST --port $CONF_PORT"
	else
		info "  未配置监听——之后自己手动执行配置:"
		info "    unidict-relay --host 127.0.0.1 --port 8788"
		info "    或重跑本脚本: ./install.sh --bind 127.0.0.1:8788（自动写入配置）"
	fi
fi
info "  探活验证: curl http://127.0.0.1:8788/api/sync/relay/ping"