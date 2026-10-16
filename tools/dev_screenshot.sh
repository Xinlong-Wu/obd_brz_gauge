#!/usr/bin/env bash
# dev_screenshot.sh — 从取图热点拉取真机屏幕帧
#
# 前提:固件开 OBD_SCREENSHOT(+ 可选 OBD_SCREENSHOT_AUTO_START)或已进
# OTA 模式页;热点 SSID = OBD-Gauge-View-XXXX(WiFi AP MAC 后两字节),
# 密码 88888888,设备 IP 192.168.4.1(screen_capture_server.c)。
#
# 用法:
#   tools/dev_screenshot.sh [输出文件] [设备IP] [SSID]
#   输出默认 /tmp/device_screen.bmp;SSID 缺省为附近扫描
#   OBD-Gauge-View-*。切热点前记住当前 Wi-Fi,取图后回连——回连用
#   系统已记住的密码(不带密码参数的 setairportnetwork),不输明文、
#   不忘记任何网络;取图前未关联则断开热点回到未关联状态。
# 注意:建议以后台方式运行——关联设备热点期间本机断网,前台长命令
# 会被会话中断。
set -uo pipefail

OUT=${1:-/tmp/device_screen.bmp}
IP=${2:-192.168.4.1}
SSID_ARG=${3:-}
AP_PASS=88888888
AP_PREFIX="OBD-Gauge-View"

IFACE=$(networksetup -listallhardwareports | awk '/^Hardware Port: Wi-Fi$/{getline; print $2}')
if [[ -z "$IFACE" ]]; then
  echo "未找到 Wi-Fi 接口" >&2
  exit 1
fi

# ---- 记住当前 Wi-Fi(便于恢复) ----
CUR_SSID=""; WAS_ASSOCIATED=0
line=$(networksetup -getairportnetwork "$IFACE" 2>/dev/null || true)
if [[ "$line" == "Current Wi-Fi Network: "* ]]; then
  WAS_ASSOCIATED=1
  CUR_SSID=${line#"Current Wi-Fi Network: "}
fi

restore_wifi() {
  if [[ -n "$CUR_SSID" ]]; then
    echo "[dev_screenshot] 回连 Wi-Fi: $CUR_SSID(用系统已记住的密码,不带密码参数)"
    networksetup -setairportnetwork "$IFACE" "$CUR_SSID" >/dev/null 2>&1 \
      || echo "[dev_screenshot] 回连失败,请手动连接 $CUR_SSID" >&2
  elif [[ "$WAS_ASSOCIATED" == "0" ]]; then
    # 取图前未关联任何 Wi-Fi:断开设备热点,回到原始未关联状态
    echo "[dev_screenshot] 断开热点(回到未关联状态)"
    networksetup -setairportpower "$IFACE" off >/dev/null 2>&1 || true
    sleep 1
    networksetup -setairportpower "$IFACE" on >/dev/null 2>&1 || true
  fi
}
trap restore_wifi EXIT

# ---- 找设备热点(显式指定优先;否则附近扫描) ----
SSID=$SSID_ARG
if [[ -z "$SSID" ]]; then
  SSID=$(system_profiler SPAirPortDataType -json 2>/dev/null | python3 -c '
import json, sys
data = json.load(sys.stdin)
sp = data.get("SPAirPortDataType", {})
if isinstance(sp, list): sp = sp[0] if sp else {}
names = [n.get("SPAirPortNetworkName", "") for n in sp.get("SPAirPortNearbyNetworks", [])]
print(next((n for n in names if n.startswith("'"$AP_PREFIX"'")), ""))')
fi
if [[ -z "$SSID" ]]; then
  echo "附近未发现 ${AP_PREFIX}-* 热点(设备未开机/未进取图模式?可用第 3 参指定 SSID)" >&2
  exit 1
fi
echo "[dev_screenshot] 加入热点: $SSID"

# ---- 切换 → 确认关联与 DHCP → 取图(trap 保证退出即恢复) ----
networksetup -setairportnetwork "$IFACE" "$SSID" "$AP_PASS" >/dev/null 2>&1
ok=0
for i in $(seq 1 25); do
  sleep 1
  cur=$(networksetup -getairportnetwork "$IFACE" 2>/dev/null || true)
  ip=$(ifconfig "$IFACE" 2>/dev/null | awk '/inet 192\.168\.4\./{print $2}')
  echo "[dev_screenshot] poll$i: $cur / ${ip:--}"
  if [[ "$cur" == *"Current Wi-Fi Network: $SSID"* && -n "$ip" ]]; then
    ok=1
    break
  fi
done
if [[ "$ok" != 1 ]]; then
  echo "关联/DHCP 未就绪(期望 192.168.4.x)" >&2
  exit 1
fi
echo "[dev_screenshot] 已关联 $SSID,本机 $ip"

for i in $(seq 1 5); do
  if curl -s --max-time 3 -o "$OUT" "http://$IP/screenshot.bmp"; then
    echo "[dev_screenshot] 已保存: $OUT ($(stat -f%z "$OUT") bytes)"
    exit 0
  fi
  sleep 1
done
echo "取图失败: http://$IP/screenshot.bmp 不可达" >&2
exit 1
