// Optional local config:
// window.DISPATCH_WEB_CONFIG = {
//   cloudApiBaseUrl: "http://127.0.0.1:8000",
//   amapKey: "YOUR_AMAP_WEB_KEY",
//   amapSecurityCode: ""
// };
let CONFIG = {};
let CLOUD_API_BASE_URL = "http://127.0.0.1:8000";
let AMAP_KEY = "";
let AMAP_SECURITY_CODE = "";

const fallbackData = {
  generated_at: new Date().toISOString(),
  cones: [
    cone("cone-demo-001", 116.39725, 39.90940, "deployed", "medium"),
    cone("cone-demo-002", 116.39755, 39.90932, "deployed", "high"),
    cone("cone-demo-003", 116.39786, 39.90920, "deployed", "high"),
    cone("cone-demo-004", 116.39820, 39.90905, "deployed", "medium"),
    cone("cone-demo-005", 116.39852, 39.90894, "deployed", "low")
  ],
  events: [
    {
      event_id: "evt-demo-001",
      event_type: "construction",
      road_name: "人民路北向主路",
      level: "high",
      status: "active",
      affected_lanes: ["right_lane", "shoulder"],
      description: "右侧施工占道，建议车辆提前向左合流。",
      boundary: [
        point(116.39718, 39.90950),
        point(116.39858, 39.90905),
        point(116.39845, 39.90872),
        point(116.39706, 39.90912)
      ]
    }
  ],
  risk_segments: [
    {
      segment_id: "seg-demo-001",
      event_id: "evt-demo-001",
      road_name: "人民路北向主路",
      level: "high",
      start: point(116.39688, 39.90956),
      end: point(116.39880, 39.90886),
      affected_lanes: ["right_lane", "shoulder"],
      suggested_action: "prepare_left_merge_and_slow_down",
      speed_limit_kph: 30
    }
  ],
  vehicle_warnings: []
};

const state = {
  view: "overview",
  layers: fallbackData,
  alerts: [],
  selectedConeId: "cone-demo-002",
  apiOnline: false,
  demoStep: 0,
  map: null,
  amapLayers: []
};

const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));

function cone(coneId, longitude, latitude, status, risk) {
  return {
    cone_id: coneId,
    status,
    current_risk_level: risk,
    last_seen_at: new Date().toISOString(),
    location: point(longitude, latitude)
  };
}

function point(longitude, latitude) {
  return { longitude, latitude, accuracy_m: 2.5, has_fix: true };
}

function init() {
  bindEvents();
  updateClock();
  setInterval(updateClock, 1000);
  loadAmap();
  refreshCloudData();
  setInterval(refreshCloudData, 2500);
  renderAll();
}

function bindEvents() {
  $$(".nav-item").forEach((button) => {
    button.addEventListener("click", () => {
      state.view = button.dataset.view;
      renderView();
    });
  });

  $("#resetDemoBtn").addEventListener("click", async () => {
    try {
      const layers = await api("/api/demo/reset", { method: "POST" });
      state.layers = layers;
      state.apiOnline = true;
      showToast("云端演示数据已重置，调度中心和车辆端会看到同一组路锥与风险段。");
    } catch (error) {
      state.apiOnline = false;
      showToast("云端不可用，已保留本地演示数据。");
    }
    renderAll();
  });

  $("#syncBtn").addEventListener("click", async () => {
    const event = state.layers.events[0];
    if (!event) return;
    try {
      await api(`/api/events/${event.event_id}/sync`, { method: "POST" });
      showToast("事件已同步为车辆预警，车端导航轮询会收到风险提示。");
      await refreshCloudData();
    } catch (error) {
      showToast("同步接口暂不可用，当前仍展示本地预警效果。");
    }
  });

  $("#dispatchBtn").addEventListener("click", () => showToast("调度任务已派发：现场复核路锥边界和右侧车道占用情况。"));
  $("#closeBtn").addEventListener("click", () => showToast("演示中保留事件状态，实际系统会在云端关闭 Road Event。"));
  $("#nextStepBtn").addEventListener("click", nextDemoStep);
  $("#playDemoBtn").addEventListener("click", playDemo);
  $("#locateBtn").addEventListener("click", () => showToast("真实地图加载后可使用浏览器定位；当前演示以固定路网为基准。"));
  $("#selectAllConesBtn").addEventListener("click", () => showToast("已框选施工边界路锥，云端将据此形成事件边界。"));
  $("#generateEventBtn").addEventListener("click", () => showToast("事件生成入口已对齐云端 Road Event 接口。"));
  $("#replayBtn").addEventListener("click", () => showToast("正在回放：路锥上报、风险段生成、车辆端轮询建议。"));
  $("#newEventBtn").addEventListener("click", () => showToast("新建事件应先写入 contracts，再同步固件、云端和车端。"));
  $("#handleAllBtn").addEventListener("click", () => showToast("告警已批量确认，风险会进入车辆端建议计算。"));
}

async function refreshCloudData() {
  try {
    const [layers, alerts] = await Promise.all([
      api("/api/map/layers"),
      api("/api/alerts")
    ]);
    state.layers = layers;
    state.alerts = alerts;
    state.apiOnline = true;
  } catch (error) {
    state.apiOnline = false;
    if (!state.layers) state.layers = fallbackData;
  }
  renderAll();
}

async function api(path, options = {}) {
  const response = await fetch(`${CLOUD_API_BASE_URL}${path}`, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options
  });
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json();
}

function renderAll() {
  renderView();
  renderHero();
  renderScenarioList();
  renderMap("smartMap");
  renderMap("largeMap");
  renderDetails();
  renderTimeline();
  renderAlerts();
  renderFusion();
  renderEvents();
  renderDevices();
  renderAnalysis();
  renderVehicleSync();
  renderAmapLayers();
}

function renderView() {
  $$(".nav-item").forEach((item) => item.classList.toggle("is-active", item.dataset.view === state.view));
  $$("[data-view-panel]").forEach((panel) => panel.classList.toggle("is-visible", panel.dataset.viewPanel === state.view));
}

function renderScenarioList() {
  $("#scenarioList").innerHTML = `
    <button class="scenario-button is-active">
      <strong>三端协同演示</strong>
      <span>${state.apiOnline ? "云端 API 在线" : "本地降级数据"} / HTTP 轮询</span>
    </button>
    <button class="scenario-button">
      <strong>树莓派车辆端</strong>
      <span>导航开始拉取风险，行驶中实时轮询建议</span>
    </button>
  `;
}

function renderHero() {
  const layers = state.layers;
  const highCones = layers.cones.filter((item) => ["high", "critical"].includes(item.current_risk_level)).length;
  $("#scenarioTitle").textContent = "调度中心、智能路锥、树莓派车端协同";
  $("#scenarioDesc").textContent = "云端汇聚路锥遥测与道路事件，车辆端导航开始时读取风险路段，行驶中按 HTTP 轮询获取路径调整和车道级领航建议。";
  $("#heroMetrics").innerHTML = [
    ["API 状态", state.apiOnline ? "在线" : "降级"],
    ["智能路锥", layers.cones.length],
    ["风险路段", layers.risk_segments.length],
    ["高风险点", highCones]
  ].map(([label, value]) => `<div class="metric"><strong>${value}</strong><span>${label}</span></div>`).join("");
  const apiStatus = $("#apiStatus");
  if (apiStatus) apiStatus.textContent = state.apiOnline ? "API 在线" : "API 离线";
}

function renderMap(mapId) {
  const map = $(`#${mapId}`);
  if (!map) return;
  $$(".cone, .vehicle, .person", map).forEach((node) => node.remove());

  const box = boundsToPercentBox(eventBoundary());
  placeBox(mapId === "largeMap" ? $("#largeEventZone") : $("#eventZone"), box);
  placeBox(mapId === "largeMap" ? $("#largeWarningRadius") : $("#warningRadius"), {
    x: Math.max(5, box.x - 8),
    y: Math.max(5, box.y - 10),
    w: Math.min(90, box.w + 16),
    h: Math.min(90, box.h + 20)
  });

  state.layers.cones.forEach((item) => {
    const position = lngLatToPercent(item.location);
    const button = document.createElement("button");
    button.className = `cone ${riskClass(item.current_risk_level)} ${item.cone_id === state.selectedConeId ? "is-selected" : ""}`;
    button.style.left = `${position.x}%`;
    button.style.top = `${position.y}%`;
    button.title = item.cone_id;
    button.setAttribute("aria-label", `${item.cone_id} 智能路锥`);
    button.addEventListener("click", () => {
      state.selectedConeId = item.cone_id;
      renderAll();
    });
    map.appendChild(button);
  });

  const vehicle = document.createElement("div");
  vehicle.className = "vehicle";
  vehicle.style.left = "31%";
  vehicle.style.top = "65%";
  map.appendChild(vehicle);
}

function renderDetails() {
  const selected = state.layers.cones.find((item) => item.cone_id === state.selectedConeId) || state.layers.cones[0];
  const event = state.layers.events[0];
  if (!selected) return;
  $("#selectedTitle").textContent = `${selected.cone_id} 智能路锥`;
  $("#selectedLevel").textContent = riskText(selected.current_risk_level);
  $("#selectedLevel").className = `level-badge level-${levelClass(selected.current_risk_level)}`;
  $("#detailBody").innerHTML = [
    ["所属事件", event ? `${event.event_type} / ${event.road_name}` : "暂无事件"],
    ["当前位置", formatLocation(selected.location)],
    ["设备状态", selected.status],
    ["风险等级", riskText(selected.current_risk_level)],
    ["最近上报", selected.last_seen_at ? new Date(selected.last_seen_at).toLocaleTimeString("zh-CN", { hour12: false }) : "--"],
    ["车端影响", "附近车辆将收到减速、绕行和车道级领航建议"]
  ].map(([key, value]) => `<div class="kv"><span>${key}</span><strong>${value}</strong></div>`).join("");
}

function renderTimeline() {
  const items = [
    ["启动", "云端加载演示路锥、道路事件和风险段"],
    ["遥测", "ESP32 路锥按 telemetry.schema.json 上传数据"],
    ["规划", "树莓派车端创建导航会话并读取调度风险"],
    ["行驶", "车辆端每 1-2 秒轮询路径调整与车道级建议"]
  ];
  $("#timeline").innerHTML = items.map(([time, text]) => `
    <div class="timeline-item"><time>${time}</time><strong>${text}</strong></div>
  `).join("");
}

function renderAlerts() {
  const alerts = state.alerts.length ? state.alerts : [
    {
      alert_id: "alt-demo-001",
      level: "high",
      alert_type: "vehicle_approach",
      message: "施工区上游 300m 建议车辆提前减速并向左合流。",
      status: "pending"
    }
  ];
  $("#alertCount").textContent = alerts.length;
  $("#alertList").innerHTML = alerts.map(renderAlertItem).join("");
  $("#alertTable").innerHTML = alerts.map((alert) => `
    <div class="alert-row">
      <strong>${alert.alert_id}</strong>
      <div><strong>${alert.alert_type}</strong><span>${alert.message}</span></div>
      <span class="level-badge level-${levelClass(alert.level)}">${riskText(alert.level)}</span>
      <span>${alert.status}</span>
    </div>
  `).join("");
}

function renderAlertItem(alert) {
  return `
    <div class="alert-item">
      <span class="level-badge level-${levelClass(alert.level)}">${riskText(alert.level)}</span>
      <div><strong>${alert.alert_type}</strong><span>${alert.message}</span></div>
      <span>${alert.status}</span>
    </div>
  `;
}

function renderFusion() {
  const event = state.layers.events[0];
  const highCones = state.layers.cones.filter((item) => ["high", "critical"].includes(item.current_risk_level)).length;
  $("#fusionList").innerHTML = [
    ["遥测契约", "路锥上报字段与 contracts/telemetry.schema.json 对齐"],
    ["事件边界", event ? `${event.road_name} / ${event.affected_lanes.join(", ")}` : "暂无事件"],
    ["车辆建议", "云端输出风险段，树莓派网页负责地图展示和局部路线调整"],
    ["风险摘要", `${highCones} 个高风险路锥，${state.layers.risk_segments.length} 个风险路段`]
  ].map(([title, text]) => `<div class="fusion-item"><strong>${title}</strong><span>${text}</span></div>`).join("");

  $("#recommendations").innerHTML = [
    "保持 HTTP 轮询作为第一版实时机制",
    "车辆进入右侧施工区前提前向左合流",
    "真实地图 Key 只写入 config.local.js",
    "后续可替换为 WebSocket 或 MQTT"
  ].map((item) => `<div class="recommend-item"><strong>${item}</strong><span>已纳入当前三端接口框架</span></div>`).join("");
}

function renderEvents() {
  $("#eventList").innerHTML = state.layers.events.map((event) => `
    <div class="event-row">
      <strong>${event.event_id}</strong>
      <div><strong>${event.event_type}</strong><span>${event.road_name}</span></div>
      <span class="level-badge level-${levelClass(event.level)}">${riskText(event.level)}</span>
      <span>${event.status}</span>
    </div>
  `).join("");
}

function renderDevices() {
  $("#deviceGrid").innerHTML = state.layers.cones.map((item) => `
    <article class="device-card">
      <strong>${item.cone_id}</strong>
      <span>${item.status} / ${riskText(item.current_risk_level)}</span>
      <div class="device-meta">
        <span>经度 ${Number(item.location.longitude).toFixed(5)}</span>
        <span>纬度 ${Number(item.location.latitude).toFixed(5)}</span>
        <span>风险 ${riskText(item.current_risk_level)}</span>
        <span>来源 ${state.apiOnline ? "云端" : "本地"}</span>
      </div>
    </article>
  `).join("");
}

function renderAnalysis() {
  const values = [3, 5, 8, 12, 9, 6];
  $("#trendChart").innerHTML = values.map((value, index) => `
    <div class="bar"><div class="bar-fill" style="height:${value * 12}px"></div><span>${index + 1}0分</span></div>
  `).join("");
  const onlineRate = state.layers.cones.length ? 100 : 0;
  $("#qualityList").innerHTML = [
    ["设备在线率", onlineRate],
    ["契约对齐度", 100],
    ["车辆建议可用性", state.apiOnline ? 96 : 70],
    ["地图图层完整度", state.layers.risk_segments.length ? 92 : 60]
  ].map(([name, value]) => `
    <div class="quality-row">
      <div><strong>${name}</strong><div class="progress"><i style="width:${value}%"></i></div></div>
      <span>${value}%</span>
    </div>
  `).join("");
  $("#reviewReport").textContent = "本版本把调度中心、智能路锥和树莓派车端统一到 HTTP 契约上。调度中心展示云端图层，路锥输出标准遥测，车端以导航会话和 tick 轮询获取路径调整与车道级领航建议。";
}

function renderVehicleSync() {
  const event = state.layers.events[0];
  const segment = state.layers.risk_segments[0];
  $("#vehicleSyncSummary").innerHTML = [
    ["云端地址", CLOUD_API_BASE_URL],
    ["导航开始", "POST /api/vehicles/{vehicle_id}/navigation-sessions"],
    ["行驶轮询", "POST /api/vehicles/{vehicle_id}/navigation-sessions/{session_id}/tick"],
    ["当前事件", event ? `${event.event_id} / ${event.road_name}` : "暂无事件"],
    ["建议动作", segment ? segment.suggested_action : "keep_lane"],
    ["车道影响", event ? event.affected_lanes.join(", ") : "--"]
  ].map(([key, value]) => `<div class="kv"><span>${key}</span><strong>${value}</strong></div>`).join("");

  $("#riskSegmentList").innerHTML = state.layers.risk_segments.map((item) => `
    <div class="event-row">
      <strong>${item.segment_id}</strong>
      <div><strong>${item.road_name}</strong><span>${item.suggested_action}</span></div>
      <span class="level-badge level-${levelClass(item.level)}">${riskText(item.level)}</span>
      <span>${item.speed_limit_kph || "--"} km/h</span>
    </div>
  `).join("");
}

function loadAmap() {
  if (!AMAP_KEY || AMAP_KEY === "YOUR_AMAP_WEB_KEY") return;
  if (AMAP_SECURITY_CODE) {
    window._AMapSecurityConfig = { securityJsCode: AMAP_SECURITY_CODE };
  }
  const script = document.createElement("script");
  script.src = `https://webapi.amap.com/maps?v=2.0&key=${encodeURIComponent(AMAP_KEY)}&plugin=AMap.Scale,AMap.ToolBar`;
  script.async = true;
  script.onload = createAmap;
  document.head.appendChild(script);
}

function createAmap() {
  if (!window.AMap || state.map) return;
  const host = $("#largeMap");
  host.classList.add("using-real-map");
  const canvas = document.createElement("div");
  canvas.className = "real-map-canvas";
  host.prepend(canvas);
  state.map = new AMap.Map(canvas, {
    zoom: 16,
    center: mapCenter(),
    viewMode: "2D",
    resizeEnable: true,
    mapStyle: "amap://styles/darkblue"
  });
  state.map.addControl(new AMap.Scale());
  state.map.addControl(new AMap.ToolBar({ position: { right: "12px", top: "12px" } }));
  renderAmapLayers();
}

function renderAmapLayers() {
  if (!state.map || !window.AMap) return;
  if (state.amapLayers.length) state.map.remove(state.amapLayers);
  const layers = [];
  state.layers.events.forEach((event) => {
    if (!event.boundary?.length) return;
    layers.push(new AMap.Polygon({
      path: event.boundary.map((item) => [item.longitude, item.latitude]),
      strokeColor: "#ff7d4d",
      strokeWeight: 2,
      fillColor: "#ff7d4d",
      fillOpacity: 0.18
    }));
  });
  state.layers.risk_segments.forEach((segment) => {
    layers.push(new AMap.Polyline({
      path: [[segment.start.longitude, segment.start.latitude], [segment.end.longitude, segment.end.latitude]],
      strokeColor: "#ff4f5e",
      strokeWeight: 6,
      strokeOpacity: 0.9
    }));
  });
  state.layers.cones.forEach((item) => {
    layers.push(new AMap.Marker({
      position: [item.location.longitude, item.location.latitude],
      content: `<div class="amap-cone ${riskClass(item.current_risk_level)}"></div>`,
      offset: new AMap.Pixel(-10, -24)
    }));
  });
  state.map.add(layers);
  state.amapLayers = layers;
  if (layers.length) state.map.setFitView(layers, false, [60, 60, 60, 60], 16);
}

function eventBoundary() {
  return state.layers.events[0]?.boundary || fallbackData.events[0].boundary;
}

function boundsToPercentBox(boundary) {
  const points = boundary.map(lngLatToPercent);
  const xs = points.map((item) => item.x);
  const ys = points.map((item) => item.y);
  const minX = Math.min(...xs);
  const maxX = Math.max(...xs);
  const minY = Math.min(...ys);
  const maxY = Math.max(...ys);
  return { x: minX, y: minY, w: maxX - minX, h: maxY - minY };
}

function lngLatToPercent(location) {
  const center = mapCenter();
  const x = 50 + ((location.longitude - center[0]) / 0.0024) * 100;
  const y = 50 - ((location.latitude - center[1]) / 0.0017) * 100;
  return {
    x: Math.max(8, Math.min(92, x)),
    y: Math.max(12, Math.min(86, y))
  };
}

function mapCenter() {
  const cones = state.layers.cones;
  if (!cones.length) return [116.3978, 39.9092];
  const lng = cones.reduce((sum, item) => sum + item.location.longitude, 0) / cones.length;
  const lat = cones.reduce((sum, item) => sum + item.location.latitude, 0) / cones.length;
  return [lng, lat];
}

function placeBox(el, box) {
  el.style.left = `${box.x}%`;
  el.style.top = `${box.y}%`;
  el.style.width = `${box.w}%`;
  el.style.height = `${box.h}%`;
}

function nextDemoStep() {
  const messages = [
    "步骤 1：智能路锥上传标准遥测，云端更新设备和风险状态。",
    "步骤 2：调度中心生成 Road Event、风险边界和风险路段。",
    "步骤 3：树莓派车端创建导航会话并读取风险摘要。",
    "步骤 4：车辆行驶中轮询动态建议，获得左侧合流提示。",
    "步骤 5：事件同步为车辆预警，地图和车端保持一致。"
  ];
  showToast(messages[state.demoStep % messages.length]);
  state.demoStep += 1;
}

function playDemo() {
  state.demoStep = 0;
  let count = 0;
  const timer = setInterval(() => {
    nextDemoStep();
    count += 1;
    if (count >= 5) clearInterval(timer);
  }, 1200);
}

function riskClass(level) {
  return {
    low: "ok",
    medium: "mid",
    high: "high",
    critical: "critical"
  }[level] || "mid";
}

function levelClass(level) {
  return {
    low: "low",
    medium: "mid",
    high: "high",
    critical: "critical"
  }[level] || "mid";
}

function riskText(level) {
  return {
    low: "低风险",
    medium: "中风险",
    high: "高风险",
    critical: "紧急风险"
  }[level] || level;
}

function formatLocation(location) {
  if (location.longitude == null || location.latitude == null) return "无定位";
  return `${Number(location.longitude).toFixed(5)}, ${Number(location.latitude).toFixed(5)}`;
}

function updateClock() {
  $("#systemClock").textContent = new Date().toLocaleTimeString("zh-CN", { hour12: false });
}

function showToast(message) {
  const toast = $("#toast");
  toast.textContent = message;
  toast.classList.add("is-visible");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove("is-visible"), 2600);
}

loadOptionalConfig().then(() => {
  CONFIG = window.DISPATCH_WEB_CONFIG || {};
  CLOUD_API_BASE_URL = (CONFIG.cloudApiBaseUrl || "http://127.0.0.1:8000").replace(/\/$/, "");
  AMAP_KEY = CONFIG.amapKey || "";
  AMAP_SECURITY_CODE = CONFIG.amapSecurityCode || "";
  init();
});

function loadOptionalConfig() {
  return new Promise((resolve) => {
    const script = document.createElement("script");
    script.src = "config.local.js";
    script.onload = resolve;
    script.onerror = resolve;
    document.head.appendChild(script);
  });
}
