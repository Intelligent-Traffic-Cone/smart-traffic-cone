# Raspberry Pi Vehicle Simulator

树莓派车辆端模拟器，用于演示车辆在导航开始时读取调度中心风险数据，并在行驶过程中通过 HTTP 轮询获取路径调整、车道级领航和附近智能路锥信息。

## 本地运行

```powershell
cd apps/pi-vehicle-simulator
python server.py
```

默认地址：

```text
http://127.0.0.1:8090
```

## 配置

复制 `config.example.js` 为 `config.local.js`，按需填写：

```js
window.PI_VEHICLE_CONFIG = {
  cloudApiBaseUrl: "http://127.0.0.1:8000",
  vehicleId: "pi-car-001",
  amapKey: "YOUR_AMAP_WEB_KEY",
  amapSecurityCode: ""
};
```

不要提交真实地图 Key、安全密钥或车辆端凭据。
