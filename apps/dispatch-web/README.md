# Dispatch Web Demo

智能交通锥调度中心静态演示页面。

页面可以直接打开 `index.html`。如果云端 API 可用，页面会从
`/api/map/layers` 和 `/api/alerts` 拉取路锥、事件、风险段和告警；如果云端不可用，则自动使用本地演示数据。

如需配置云端地址或高德地图，复制 `config.example.js` 为
`config.local.js`，填入本地配置。页面启动时会自动尝试加载该本地配置文件。

不要提交真实地图 Key、安全密钥或云端令牌。

## 局域网双路线调度

云端需使用 `--host 0.0.0.0` 启动。将 `config.local.js` 中的地址改为管理
电脑局域网 IP，例如：

```js
window.DISPATCH_WEB_CONFIG = {
  cloudApiBaseUrl: "http://192.168.1.100:8000"
};
```

“车辆协同”页面每秒读取路线评分和车辆位置。系统默认选中低风险路线，
调度员可以切换另一条路线后下发给指定车辆。
