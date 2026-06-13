# 智能交通锥

智能交通锥项目的团队工作区。

系统在每个智能交通锥内部使用 ESP32 边缘节点，采集定位、四路超声波测距、摄像头和设备健康状态数据。云端接收遥测数据，生成道路事件和告警，并向地图或车端系统发布已确认的道路预警信息。

## 仓库结构

```text
apps/
  dispatch-web/       静态调度中心演示页面。
  edge-cone-node/     单个交通锥节点的 PlatformIO 固件项目。
  gps-test-pio/       独立的 PlatformIO GPS UART 台架测试项目。
  pi-vehicle-simulator/
                      树莓派车辆端导航模拟器。
  vehicle-desktop-simulator/
                      Linux/树莓派车辆桌面上位机。
components/
  cone_device/        可复用的硬件模块接口。
services/
  cloud-api/          FastAPI 云端 API 骨架。
contracts/
  telemetry.schema.json
  vehicle-warning.md
  vehicle-navigation.md
docs/
  development/        团队开发指南。
  product/            产品报告和设计文档。
```

仓库根目录只是工作区，不是 PlatformIO 固件项目。请在 `apps/edge-cone-node` 目录下运行固件相关命令。

## 快速开始

一键启动云端、管理 Web 和车辆桌面上位机：

```bash
chmod +x start_demo.sh
./start_demo.sh
```

脚本会自动使用当前局域网 IP 写入本地配置。按 `Ctrl+C` 可停止全部服务。

固件：

```powershell
cd apps/edge-cone-node
pio run -e esp32dev
```

GPS 台架测试：

```powershell
cd apps/gps-test-pio
pio run -e esp32-s3-devkitc-1
```

云端 API：

```bash
cd services/cloud-api
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

调度网页：

```powershell
cd apps/dispatch-web
start index.html
```

树莓派车辆端模拟器：

```powershell
cd apps/pi-vehicle-simulator
python server.py
```

然后打开 `http://127.0.0.1:8090`。车辆端会在导航开始时创建导航会话，并在模拟行驶中通过 HTTP 轮询云端动态建议。

Linux/树莓派桌面上位机：

```bash
cd apps/vehicle-desktop-simulator
cp config.example.json config.local.json
python3 app.py
```

在 `config.local.json` 中把云端地址改为管理电脑的局域网 IP。管理 Web
在“车辆协同”页面评估两条固定路线并下发，上位机领取路线后模拟行驶和上传位置。

## 开发边界

- `apps/edge-cone-node` 负责产品编排和上传调度。
- `apps/pi-vehicle-simulator` 负责车辆端导航、路径调整和车道级领航模拟。
- `apps/vehicle-desktop-simulator` 负责局域网车辆任务领取、位置模拟和桌面展示。
- `apps/dispatch-web` 负责调度中心静态演示和云端 API 数据展示。
- `components/cone_device` 负责可复用的硬件接口和模块状态。
- `services/cloud-api` 负责云端 HTTP API 行为和 OpenAPI 文档。
- `contracts` 负责固件、云端和客户端之间共享的传输载荷。
- `docs/development` 负责团队流程、代码风格和接口变更规则。

不要提交真实地图密钥、Wi-Fi 凭据、云端令牌或硬件实验室机密。请使用已被 git 忽略的本地配置文件。
