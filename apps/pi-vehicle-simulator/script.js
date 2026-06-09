const DEFAULT_CONFIG = {
  cloudApiBaseUrl: "http://127.0.0.1:8000",
  vehicleId: "pi-car-001",
  amapKey: "",
  amapSecurityCode: ""
};

const route = [
  { longitude: 116.39658, latitude: 39.90974, lane: "right_lane", x: 18, y: 66 },
  { longitude: 116.39696, latitude: 39.90958, lane: "right_lane", x: 29, y: 61 },
  { longitude: 116.39735, latitude: 39.90940, lane: "right_lane", x: 41, y: 56 },
  { longitude: 116.39774, latitude: 39.90922, lane: "right_lane", x: 53, y: 50 },
  { longitude: 116.39815, latitude: 39.90902, lane: "left_lane", x: 65, y: 44 },
  { longitude: 116.39862, latitude: 39.90882, lane: "left_lane", x: 78, y: 38 },
  { longitude: 116.39908, latitude: 39.90866, lane: "left_lane", x: 88, y: 33 }
];

const state = {
  config: DEFAULT_CONFIG,
  session: null,
  advice: null,
  mapLayers: null,
  routeIndex: 0,
  timer: null,
  traces: [],
  amap: null,
  amapLayers: []
};

const $ = (selector, root = document) => root.querySelector(selector);

init();

async function init() {
  state.config = await loadConfig();
  bindEvents();
  renderAll();
  loadAmap();
  await refreshMapLayers();
}

function bindEvents() {
  $("#startNavBtn").addEventListener("click", startNavigation);
  $("#driveBtn").addEventListener("click", startDriving);
  $("#stopBtn").addEventListener("click", stopDriving);
}

async function loadConfig() {
  try {
    await loadScript("config.local.js");
  } catch (error) {
    // Local config is optional.
  }
  return { ...DEFAULT_CONFIG, ...(window.PI_VEHICLE_CONFIG || {}) };
}

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = src;
    script.onload = resolve;
    script.onerror = reject;
    document.head.appendChild(script);
  });
}

async function refreshMapLayers() {
  try {
    state.mapLayers = await api("/api/map/layers");
    setCloudStatus(true);
    addTrace("GET /api/map/layers", "ok");
  } catch (error) {
    setCloudStatus(false);
    addTrace("GET /api/map/layers", "failed");
  }
  renderAll();
}

async function startNavigation() {
  const payload = {
    vehicle_id: state.config.vehicleId,
    origin: toLocation(route[0]),
    destination: toLocation(route[route.length - 1]),
    route_preference: "avoid_risk"
  };

  try {
    state.session = await api(`/api/vehicles/${state.config.vehicleId}/navigation-sessions`, {
      method: "POST",
      body: JSON.stringify(payload)
    });
    state.routeIndex = 0;
    $("#driveBtn").disabled = false;
    $("#stopBtn").disabled = false;
    setCloudStatus(true);
    addTrace("POST navigation-sessions", "ok");
    showToast("导航会话已创建，车辆端已读取调度中心风险路段。");
    await tickNavigation();
  } catch (error) {
    setCloudStatus(false);
    addTrace("POST navigation-sessions", "failed");
    showToast("无法创建导航会话，请先启动 cloud-api。");
  }
  renderAll();
}

function startDriving() {
  if (!state.session || state.timer) return;
  state.timer = setInterval(async () => {
    state.routeIndex = Math.min(state.routeIndex + 1, route.length - 1);
    await tickNavigation();
    if (state.routeIndex >= route.length - 1) {
      stopDriving();
      showToast("模拟车辆已到达终点。");
    }
  }, 1400);
  showToast("车辆开始模拟行驶，正在按 1.4 秒间隔轮询动态建议。");
}

function stopDriving() {
  if (state.timer) clearInterval(state.timer);
  state.timer = null;
  renderAll();
}

async function tickNavigation() {
  if (!state.session) return;
  const current = route[state.routeIndex];
  const payload = {
    session_id: state.session.session_id,
    location: toLocation(current),
    speed_kph: state.routeIndex >= 3 ? 24 : 32,
    current_lane: current.lane,
    heading_deg: 110
  };

  try {
    state.advice = await api(`/api/vehicles/${state.config.vehicleId}/navigation-sessions/${state.session.session_id}/tick`, {
      method: "POST",
      body: JSON.stringify(payload)
    });
    setCloudStatus(true);
    addTrace("POST navigation tick", state.advice.risk_level);
  } catch (error) {
    setCloudStatus(false);
    addTrace("POST navigation tick", "failed");
  }
  renderAll();
}

async function api(path, options = {}) {
  const base = state.config.cloudApiBaseUrl.replace(/\/$/, "");
  const response = await fetch(`${base}${path}`, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options
  });
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json();
}

function renderAll() {
  renderVehicle();
  renderCones();
  renderAmapLayers();
  renderSession();
  renderAdvice();
  renderNearbyCones();
  renderTrace();
}

function renderVehicle() {
  const current = route[state.routeIndex];
  const marker = $("#vehicleMarker");
  marker.style.left = `${current.x}%`;
  marker.style.top = `${current.y}%`;
}

function renderCones() {
  const layer = $("#coneLayer");
  const cones = state.mapLayers?.cones || [];
  layer.innerHTML = cones.map((item) => {
    const position = lngLatToCanvas(item.location);
    return `<span class="cone-pin ${item.current_risk_level}" style="left:${position.x}%;top:${position.y}%" title="${item.cone_id}"></span>`;
  }).join("");
}

function renderSession() {
  const session = state.session;
  $("#sessionPanel").innerHTML = [
    ["车辆编号", state.config.vehicleId],
    ["云端地址", state.config.cloudApiBaseUrl],
    ["会话编号", session?.session_id || "未开始"],
    ["路线偏好", session?.route_preference || "avoid_risk"],
    ["风险摘要", session?.risk_summary || "等待导航开始"],
    ["轮询状态", state.timer ? "行驶中" : "待命"]
  ].map(renderKv).join("");
}

function renderAdvice() {
  if (!state.advice) {
    $("#advicePanel").innerHTML = "<strong>等待动态建议</strong><span>点击“开始导航”后，车辆端会向调度中心创建导航会话。</span>";
    return;
  }
  $("#advicePanel").innerHTML = `
    <strong class="risk">${riskText(state.advice.risk_level)}</strong>
    <p>${state.advice.message}</p>
    <span>路径调整</span>
    <strong>${state.advice.route_adjustment}</strong>
    <span>车道级领航</span>
    <strong>${laneActionText(state.advice.lane_guidance.action)} / 目标：${state.advice.lane_guidance.target_lane}</strong>
    <span>原因</span>
    <strong>${state.advice.lane_guidance.reason}</strong>
  `;
}

function renderNearbyCones() {
  const cones = state.advice?.nearby_cones || [];
  $("#nearbyConeList").innerHTML = cones.length
    ? cones.map((item) => `
      <div class="cone-item">
        <div><strong>${item.cone_id}</strong><span>${item.lane_hint} / ${riskText(item.risk_level)}</span></div>
        <strong>${item.distance_m} m</strong>
      </div>
    `).join("")
    : "<div class=\"cone-item\"><div><strong>暂无附近路锥</strong><span>等待 tick 接口返回</span></div><strong>--</strong></div>";
}

function renderTrace() {
  $("#traceList").innerHTML = state.traces.map((item) => `
    <div class="trace-item">
      <strong>${item.label}</strong>
      <span>${item.status} / ${item.time}</span>
    </div>
  `).join("");
}

function loadAmap() {
  if (!state.config.amapKey || state.config.amapKey === "YOUR_AMAP_WEB_KEY") return;
  if (state.config.amapSecurityCode) {
    window._AMapSecurityConfig = { securityJsCode: state.config.amapSecurityCode };
  }
  const script = document.createElement("script");
  script.src = `https://webapi.amap.com/maps?v=2.0&key=${encodeURIComponent(state.config.amapKey)}&plugin=AMap.Scale,AMap.ToolBar`;
  script.async = true;
  script.onload = createAmap;
  script.onerror = () => addTrace("AMap load", "failed");
  document.head.appendChild(script);
}

function createAmap() {
  if (!window.AMap || state.amap) return;
  const host = $("#mapCanvas");
  host.classList.add("using-real-map");
  const canvas = document.createElement("div");
  canvas.className = "real-map-canvas";
  host.prepend(canvas);
  state.amap = new AMap.Map(canvas, {
    zoom: 16,
    center: [route[2].longitude, route[2].latitude],
    viewMode: "2D",
    resizeEnable: true,
    mapStyle: "amap://styles/darkblue"
  });
  state.amap.addControl(new AMap.Scale());
  state.amap.addControl(new AMap.ToolBar({ position: { right: "12px", top: "12px" } }));
  addTrace("AMap load", "ok");
  renderAmapLayers();
}

function renderAmapLayers() {
  if (!state.amap || !window.AMap) return;
  if (state.amapLayers.length) state.amap.remove(state.amapLayers);
  const layers = [];

  layers.push(new AMap.Polyline({
    path: route.map((item) => [item.longitude, item.latitude]),
    strokeColor: "#4aa8ff",
    strokeWeight: 7,
    strokeOpacity: 0.86
  }));

  (state.mapLayers?.risk_segments || []).forEach((segment) => {
    layers.push(new AMap.Polyline({
      path: [[segment.start.longitude, segment.start.latitude], [segment.end.longitude, segment.end.latitude]],
      strokeColor: "#ff5364",
      strokeWeight: 8,
      strokeOpacity: 0.92
    }));
  });

  (state.mapLayers?.events || []).forEach((event) => {
    if (!event.boundary?.length) return;
    layers.push(new AMap.Polygon({
      path: event.boundary.map((item) => [item.longitude, item.latitude]),
      strokeColor: "#ff854f",
      strokeWeight: 2,
      fillColor: "#ff854f",
      fillOpacity: 0.15
    }));
  });

  (state.mapLayers?.cones || []).forEach((item) => {
    layers.push(new AMap.Marker({
      position: [item.location.longitude, item.location.latitude],
      content: `<div class="amap-cone ${item.current_risk_level}"></div>`,
      offset: new AMap.Pixel(-10, -24),
      zIndex: 20
    }));
  });

  const current = route[state.routeIndex];
  layers.push(new AMap.Marker({
    position: [current.longitude, current.latitude],
    content: "<div class=\"amap-vehicle\"></div>",
    offset: new AMap.Pixel(-24, -13),
    zIndex: 30
  }));

  state.amap.add(layers);
  state.amapLayers = layers;
  if (layers.length) state.amap.setFitView(layers, false, [60, 60, 60, 60], 16);
}

function renderKv([key, value]) {
  return `<div class="kv"><span>${key}</span><strong>${value}</strong></div>`;
}

function toLocation(item) {
  return {
    longitude: item.longitude,
    latitude: item.latitude,
    accuracy_m: 3,
    has_fix: true
  };
}

function lngLatToCanvas(location) {
  const minLng = 116.3963;
  const maxLng = 116.3993;
  const minLat = 39.9085;
  const maxLat = 39.9099;
  return {
    x: 12 + ((location.longitude - minLng) / (maxLng - minLng)) * 76,
    y: 74 - ((location.latitude - minLat) / (maxLat - minLat)) * 48
  };
}

function setCloudStatus(online) {
  const pill = $("#cloudStatus");
  pill.textContent = online ? "云端在线" : "云端离线";
  pill.className = `status-pill ${online ? "online" : "offline"}`;
}

function addTrace(label, status) {
  state.traces.unshift({
    label,
    status,
    time: new Date().toLocaleTimeString("zh-CN", { hour12: false })
  });
  state.traces = state.traces.slice(0, 5);
}

function riskText(level) {
  return {
    low: "低风险",
    medium: "中风险",
    high: "高风险",
    critical: "紧急风险"
  }[level] || level;
}

function laneActionText(action) {
  return {
    keep_lane: "保持车道",
    prepare_left_merge: "准备向左合流",
    merge_left_now: "立即向左合流",
    prepare_right_merge: "准备向右合流",
    merge_right_now: "立即向右合流",
    stop_if_unsafe: "条件不安全时停车"
  }[action] || action;
}

function showToast(message) {
  const toast = $("#toast");
  toast.textContent = message;
  toast.classList.add("is-visible");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.remove("is-visible"), 2600);
}
