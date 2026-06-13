#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CLOUD_DIR="$ROOT_DIR/services/cloud-api"
WEB_DIR="$ROOT_DIR/apps/dispatch-web"
VEHICLE_DIR="$ROOT_DIR/apps/vehicle-desktop-simulator"
VENV_DIR="$CLOUD_DIR/.venv"
RUN_DIR="/tmp/smart-traffic-cone-${USER:-user}"

CLOUD_PORT="${CLOUD_PORT:-8000}"
WEB_PORT="${WEB_PORT:-8091}"
VEHICLE_ID="${VEHICLE_ID:-pi-car-001}"
START_VEHICLE="${START_VEHICLE:-1}"
OPEN_BROWSER="${OPEN_BROWSER:-1}"

mkdir -p "$RUN_DIR"

child_pids=()

cleanup() {
  trap - INT TERM EXIT
  if ((${#child_pids[@]})); then
    kill "${child_pids[@]}" 2>/dev/null || true
    wait "${child_pids[@]}" 2>/dev/null || true
  fi
  echo
  echo "智能交通锥演示服务已停止。"
}

trap cleanup INT TERM EXIT

port_in_use() {
  ss -ltn 2>/dev/null | awk '{print $4}' | grep -Eq ":$1$"
}

if port_in_use "$CLOUD_PORT"; then
  echo "错误：云端端口 $CLOUD_PORT 已被占用。"
  exit 1
fi

if port_in_use "$WEB_PORT"; then
  echo "错误：管理网页端口 $WEB_PORT 已被占用。"
  exit 1
fi

if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  echo "创建云端 Python 虚拟环境..."
  python3 -m venv "$VENV_DIR"
fi

if [[ ! -x "$VENV_DIR/bin/uvicorn" ]]; then
  echo "安装云端依赖..."
  "$VENV_DIR/bin/pip" install -r "$CLOUD_DIR/requirements.txt"
fi

LAN_IP="${LAN_IP:-$(hostname -I | awk '{for (i=1; i<=NF; i++) if ($i !~ /^127\\./ && $i !~ /^198\\.18\\./) {print $i; exit}}')}"
LAN_IP="${LAN_IP:-127.0.0.1}"
CLOUD_URL="http://${LAN_IP}:${CLOUD_PORT}"
WEB_URL="http://${LAN_IP}:${WEB_PORT}"

cat >"$WEB_DIR/config.local.js" <<EOF
window.DISPATCH_WEB_CONFIG = {
  cloudApiBaseUrl: "${CLOUD_URL}",
  amapKey: "",
  amapSecurityCode: ""
};
EOF

cat >"$VEHICLE_DIR/config.local.json" <<EOF
{
  "cloud_api_base_url": "${CLOUD_URL}",
  "vehicle_id": "${VEHICLE_ID}",
  "poll_interval_ms": 1000,
  "default_speed_kph": 30
}
EOF

echo "启动云端 API..."
(
  cd "$CLOUD_DIR"
  exec "$VENV_DIR/bin/uvicorn" app.main:app --host 0.0.0.0 --port "$CLOUD_PORT"
) >"$RUN_DIR/cloud-api.log" 2>&1 &
child_pids+=("$!")

echo "启动管理网页..."
(
  cd "$WEB_DIR"
  exec python3 -m http.server "$WEB_PORT" --bind 0.0.0.0
) >"$RUN_DIR/dispatch-web.log" 2>&1 &
child_pids+=("$!")

for _ in {1..30}; do
  if curl -fsS "http://127.0.0.1:${CLOUD_PORT}/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.2
done

if ! curl -fsS "http://127.0.0.1:${CLOUD_PORT}/health" >/dev/null 2>&1; then
  echo "云端启动失败，日志：$RUN_DIR/cloud-api.log"
  exit 1
fi

if [[ "$START_VEHICLE" == "1" ]]; then
  if [[ -n "${DISPLAY:-}" ]]; then
    echo "启动车辆上位机..."
    (
      cd "$VEHICLE_DIR"
      exec python3 app.py
    ) >"$RUN_DIR/vehicle-desktop.log" 2>&1 &
    child_pids+=("$!")
  else
    echo "未检测到图形显示环境，已跳过车辆上位机。"
  fi
fi

if [[ "$OPEN_BROWSER" == "1" ]] && command -v xdg-open >/dev/null 2>&1; then
  xdg-open "http://127.0.0.1:${WEB_PORT}/" >"$RUN_DIR/browser.log" 2>&1 || true
fi

echo
echo "管理网页（本机）：http://127.0.0.1:${WEB_PORT}"
echo "管理网页（局域网）：${WEB_URL}"
echo "API 文档：${CLOUD_URL}/docs"
echo "运行日志：${RUN_DIR}"
echo
echo "按 Ctrl+C 停止全部服务。"

wait
