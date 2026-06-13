# Linux / Raspberry Pi Vehicle Desktop Simulator

车辆桌面上位机通过局域网 HTTP 领取管理端下发的固定路线，按设定速度模拟行驶并持续上传位置。

## 运行

```bash
cd apps/vehicle-desktop-simulator
cp config.example.json config.local.json
python3 app.py
```

树莓派系统如未安装 Tkinter：

```bash
sudo apt install python3-tk
```

将 `config.local.json` 中的 `cloud_api_base_url` 改为管理电脑的局域网地址，例如 `http://192.168.1.100:8000`。

操作顺序：

1. 管理电脑启动 cloud-api 和 dispatch-web。
2. 上位机点击“连接”并保持在线。
3. 管理 Web 的“车辆协同”页面选择路线并下发。
4. 上位机领取任务后点击“开始”。
