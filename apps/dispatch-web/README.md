# Dispatch Web Demo

智能交通锥调度中心静态演示页面。

页面可以直接打开 `index.html`。如果云端 API 可用，页面会从
`/api/map/layers` 和 `/api/alerts` 拉取路锥、事件、风险段和告警；如果云端不可用，则自动使用本地演示数据。
当云端返回路锥 `image_url` 时，页面会在路锥详情和设备卡片中显示最近现场快照。

如需配置云端地址或高德地图，复制 `config.example.js` 为
`config.local.js`，填入本地配置。页面启动时会自动尝试加载该本地配置文件。

不要提交真实地图 Key、安全密钥或云端令牌。
