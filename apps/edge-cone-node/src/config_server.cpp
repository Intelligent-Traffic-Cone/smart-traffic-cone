#include "config_server.h"

#include <WebServer.h>

namespace {
WebServer g_server(80);
ConfigServer* g_instance = nullptr;

bool arg_enabled(const char* name) {
  return g_server.hasArg(name) && g_server.arg(name) == "1";
}

bool parse_u16_arg(const char* name, uint16_t min_value, uint16_t max_value, uint16_t& out) {
  if (!g_server.hasArg(name)) {
    return false;
  }
  const String value = g_server.arg(name);
  if (value.length() == 0) {
    return false;
  }
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isDigit(value[i])) {
      return false;
    }
  }
  const long parsed = value.toInt();
  if (parsed < min_value || parsed > max_value) {
    return false;
  }
  out = static_cast<uint16_t>(parsed);
  return true;
}

void send_json(int code, const std::string& payload) {
  g_server.sendHeader("Access-Control-Allow-Origin", "*");
  g_server.send(code, "application/json", payload.c_str());
}

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>智能路锥本地配置</title>
  <style>
    body{margin:0;background:#10151d;color:#eef3f8;font-family:Arial,"Microsoft YaHei",sans-serif}
    main{max-width:760px;margin:0 auto;padding:24px}
    h1{font-size:24px;margin:0 0 18px}
    section{border:1px solid #303947;background:#171c24;padding:16px;margin-bottom:14px}
    label{display:flex;align-items:center;gap:10px;margin:10px 0}
    select,button{font:inherit;min-height:36px;background:#1f2630;color:#eef3f8;border:1px solid #303947;padding:0 10px}
    button{background:#256bd8;border-color:#4289f5;cursor:pointer}
    button.off{background:#303947;border-color:#4b5665}
    button.flash{background:#116c68;border-color:#23a8a1}
    input[type=range]{width:100%}
    .row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}
    .value{color:#8fd3ff;font-weight:700}
    .matrix{display:grid;grid-template-columns:120px repeat(3,minmax(0,1fr));gap:8px;align-items:center;margin-top:12px}
    .matrix strong{color:#bac7d6}
    .message{min-height:24px;color:#8fd3ff;margin-top:10px}
    pre{white-space:pre-wrap;color:#bac7d6}
  </style>
</head>
<body>
<main>
  <h1>智能路锥本地配置</h1>
  <section>
    <label>路况模板
      <select id="scene">
        <option value="one_way">单行道</option>
        <option value="work_zone">双向/施工路段</option>
        <option value="intersection">十字路口</option>
        <option value="custom">自定义</option>
      </select>
    </label>
    <label><input type="checkbox" id="front"> 前向 front</label>
    <label><input type="checkbox" id="rear"> 后向 rear</label>
    <label><input type="checkbox" id="left"> 左向 left</label>
    <label><input type="checkbox" id="right"> 右向 right</label>
    <button id="save">保存并上报</button>
  </section>
  <section>
    <strong>方位控制</strong>
    <div class="row" style="margin-top:12px">
      <button id="panManual">进入人工控制</button>
      <button id="panRoam" class="off">恢复漫游</button>
      <span>模式 <span class="value" id="panMode">loading</span></span>
      <span>方位 <span class="value" id="panHeading">0</span>°</span>
    </div>
    <label>目标方位
      <input type="range" id="panSlider" min="0" max="360" step="1" value="0" disabled>
    </label>
    <div class="message" id="panMessage"></div>
  </section>
  <section>
    <strong>警示灯调试</strong>
    <div class="row" style="margin-top:12px">
      <button id="warningAutoOn">开启自动联动</button>
      <button id="warningAutoOff" class="off">关闭自动联动</button>
      <span>等级 <span class="value" id="warningLevel">loading</span></span>
      <span>最近 <span class="value" id="warningNearest">--</span></span>
      <span>暂停 <span class="value" id="warningPause">0</span>ms</span>
    </div>
    <div class="matrix" id="warningMatrix"></div>
    <div class="message" id="warningMessage"></div>
  </section>
  <section>
    <strong>状态</strong>
    <pre id="status">loading...</pre>
  </section>
</main>
<script>
const ids = ["front","rear","left","right"];
const warningTargets = [
  ["yellow","黄灯"],
  ["green","绿灯"],
  ["red","红灯"],
  ["buzzer","蜂鸣"],
  ["yellow_buzzer","黄灯+蜂鸣"],
  ["green_buzzer","绿灯+蜂鸣"],
  ["red_buzzer","红灯+蜂鸣"],
  ["all","全部"]
];
const warningActions = [
  ["off","关闭","off"],
  ["on","打开","on"],
  ["flash","闪烁","flash"]
];
async function load(){
  const cfg = await fetch("/api/config").then(r=>r.json());
  document.querySelector("#scene").value = cfg.scene;
  ids.forEach(id=>document.querySelector("#"+id).checked = !!cfg.directions[id]);
  refreshStatus();
}
async function refreshStatus(){
  const status = await fetch("/api/status").then(r=>r.json());
  document.querySelector("#status").textContent = JSON.stringify(status,null,2);
  renderPanStatus(status.pan);
  renderWarningAutomation(status.warning_automation);
}
function renderPanStatus(pan){
  if(!pan) return;
  const isManual = pan.mode === "manual";
  document.querySelector("#panMode").textContent = isManual ? "人工控制" : "漫游";
  document.querySelector("#panHeading").textContent = pan.heading_deg;
  const slider = document.querySelector("#panSlider");
  slider.value = pan.heading_deg;
  slider.disabled = !isManual;
}
async function setPanMode(mode){
  const body = new URLSearchParams();
  body.set("mode", mode);
  const response = await fetch("/api/pan/mode", {method:"POST", body});
  const payload = await response.json();
  document.querySelector("#panMessage").textContent = payload.ok ? "方位模式已更新" : `模式切换失败: ${payload.error || response.status}`;
  refreshStatus();
}
let panSliderTimer = 0;
function queuePanHeading(value){
  document.querySelector("#panHeading").textContent = value;
  clearTimeout(panSliderTimer);
  panSliderTimer = setTimeout(() => setPanHeading(value), 120);
}
async function setPanHeading(value){
  const body = new URLSearchParams();
  body.set("heading_deg", value);
  const response = await fetch("/api/pan/heading", {method:"POST", body});
  const payload = await response.json();
  document.querySelector("#panMessage").textContent = payload.ok ? `已设置 ${value}°` : `设置失败: ${payload.error || response.status}`;
  refreshStatus();
}
function renderWarningAutomation(automation){
  if(!automation) return;
  document.querySelector("#warningLevel").textContent = automation.enabled ? automation.level : "off";
  const nearest = automation.nearest_direction ? `${automation.nearest_direction} ${Number(automation.nearest_distance_m).toFixed(2)}m` : "--";
  document.querySelector("#warningNearest").textContent = nearest;
  document.querySelector("#warningPause").textContent = automation.manual_pause_remaining_ms || 0;
}
async function setWarningAutomation(enabled){
  const body = new URLSearchParams();
  body.set("enabled", enabled ? "1" : "0");
  const response = await fetch("/api/warning-automation", {method:"POST", body});
  const payload = await response.json();
  document.querySelector("#warningMessage").textContent = payload.ok ? "自动联动已更新" : `自动联动设置失败: ${payload.error || response.status}`;
  refreshStatus();
}
function renderWarningMatrix(){
  const host = document.querySelector("#warningMatrix");
  host.innerHTML = "<strong>目标</strong>" + warningActions.map(a=>`<strong>${a[1]}</strong>`).join("");
  warningTargets.forEach(([target,label]) => {
    host.insertAdjacentHTML("beforeend", `<strong>${label}</strong>`);
    warningActions.forEach(([action,label,klass]) => {
      host.insertAdjacentHTML("beforeend", `<button class="${klass}" data-target="${target}" data-action="${action}">${label}</button>`);
    });
  });
  host.querySelectorAll("button").forEach(button => {
    button.addEventListener("click", () => sendWarningCommand(button.dataset.target, button.dataset.action));
  });
}
async function sendWarningCommand(target, action){
  const body = new URLSearchParams();
  body.set("target", target);
  body.set("action", action);
  const response = await fetch("/api/warning-light", {method:"POST", body});
  const payload = await response.json();
  document.querySelector("#warningMessage").textContent = payload.ok ? `已发送 ${payload.frame}，自动联动暂停30秒` : `发送失败: ${payload.error || response.status}`;
  refreshStatus();
}
document.querySelector("#scene").addEventListener("change", e => {
  const scene = e.target.value;
  if(scene === "one_way") ids.forEach((id,i)=>document.querySelector("#"+id).checked = i===0);
  if(scene === "work_zone") ids.forEach((id,i)=>document.querySelector("#"+id).checked = i<2);
  if(scene === "intersection") ids.forEach(id=>document.querySelector("#"+id).checked = true);
});
document.querySelector("#save").addEventListener("click", async () => {
  const body = new URLSearchParams();
  body.set("scene", document.querySelector("#scene").value);
  ids.forEach(id=>body.set(id, document.querySelector("#"+id).checked ? "1" : "0"));
  await fetch("/api/config", {method:"POST", body});
  await load();
});
document.querySelector("#panManual").addEventListener("click", () => setPanMode("manual"));
document.querySelector("#panRoam").addEventListener("click", () => setPanMode("roam"));
document.querySelector("#panSlider").addEventListener("input", e => queuePanHeading(e.target.value));
document.querySelector("#warningAutoOn").addEventListener("click", () => setWarningAutomation(true));
document.querySelector("#warningAutoOff").addEventListener("click", () => setWarningAutomation(false));
setInterval(refreshStatus, 3000);
renderWarningMatrix();
load();
</script>
</body>
</html>
)HTML";
}  // namespace

void ConfigServer::setup(ConeNodeSettings* settings,
                         SettingsChangedCallback on_settings_changed,
                         StatusProvider status_provider,
                         WarningLightCommandHandler warning_light_handler,
                         PanStatusProvider pan_status_provider,
                         PanModeCommandHandler pan_mode_handler,
                         PanHeadingCommandHandler pan_heading_handler,
                         WarningAutomationCommandHandler warning_automation_handler) {
  settings_ = settings;
  on_settings_changed_ = on_settings_changed;
  status_provider_ = status_provider;
  warning_light_handler_ = warning_light_handler;
  pan_status_provider_ = pan_status_provider;
  pan_mode_handler_ = pan_mode_handler;
  pan_heading_handler_ = pan_heading_handler;
  warning_automation_handler_ = warning_automation_handler;
  g_instance = this;

  g_server.on("/", HTTP_GET, []() {
    g_server.send_P(200, "text/html; charset=utf-8", kIndexHtml);
  });

  g_server.on("/api/config", HTTP_GET, []() {
    if (g_instance == nullptr || g_instance->settings_ == nullptr) {
      send_json(500, "{\"error\":\"settings_unavailable\"}");
      return;
    }
    send_json(200, settings_json(*g_instance->settings_));
  });

  g_server.on("/api/status", HTTP_GET, []() {
    if (g_instance == nullptr || !g_instance->status_provider_) {
      send_json(500, "{\"error\":\"status_unavailable\"}");
      return;
    }
    send_json(200, g_instance->status_provider_());
  });

  g_server.on("/api/config", HTTP_POST, []() {
    if (g_instance == nullptr || g_instance->settings_ == nullptr) {
      send_json(500, "{\"error\":\"settings_unavailable\"}");
      return;
    }

    ConeNodeSettings next = *g_instance->settings_;
    next.scene = scene_from_string(g_server.arg("scene").c_str());
    next.ultrasonic_enabled[0] = arg_enabled("front");
    next.ultrasonic_enabled[1] = arg_enabled("rear");
    next.ultrasonic_enabled[2] = arg_enabled("left");
    next.ultrasonic_enabled[3] = arg_enabled("right");
    next.revision += 1;
    next.config_changed = true;
    save_settings(next);
    *g_instance->settings_ = next;
    if (g_instance->on_settings_changed_) {
      g_instance->on_settings_changed_(next);
    }
    send_json(200, settings_json(next));
  });

  g_server.on("/api/warning-light", HTTP_POST, []() {
    if (g_instance == nullptr || !g_instance->warning_light_handler_) {
      send_json(500, "{\"ok\":false,\"error\":\"warning_light_unavailable\"}");
      return;
    }

    cone_device::WarningLightTarget target;
    cone_device::WarningLightAction action;
    if (!cone_device::parse_warning_light_target(g_server.arg("target").c_str(), target)) {
      send_json(400, "{\"ok\":false,\"error\":\"invalid_target\"}");
      return;
    }
    if (!cone_device::parse_warning_light_action(g_server.arg("action").c_str(), action)) {
      send_json(400, "{\"ok\":false,\"error\":\"invalid_action\"}");
      return;
    }
    send_json(200, g_instance->warning_light_handler_(target, action));
  });

  g_server.on("/api/pan", HTTP_GET, []() {
    if (g_instance == nullptr || !g_instance->pan_status_provider_) {
      send_json(500, "{\"error\":\"pan_unavailable\"}");
      return;
    }
    send_json(200, g_instance->pan_status_provider_());
  });

  g_server.on("/api/pan/mode", HTTP_POST, []() {
    if (g_instance == nullptr || !g_instance->pan_mode_handler_) {
      send_json(500, "{\"ok\":false,\"error\":\"pan_unavailable\"}");
      return;
    }

    PanMode mode;
    if (!pan_mode_from_string(g_server.arg("mode").c_str(), mode)) {
      send_json(400, "{\"ok\":false,\"error\":\"invalid_mode\"}");
      return;
    }
    send_json(200, g_instance->pan_mode_handler_(mode));
  });

  g_server.on("/api/pan/heading", HTTP_POST, []() {
    if (g_instance == nullptr || !g_instance->pan_heading_handler_) {
      send_json(500, "{\"ok\":false,\"error\":\"pan_unavailable\"}");
      return;
    }

    uint16_t heading_deg = 0;
    if (!parse_u16_arg("heading_deg", 0, 360, heading_deg)) {
      send_json(400, "{\"ok\":false,\"error\":\"invalid_heading\"}");
      return;
    }
    const std::string payload = g_instance->pan_heading_handler_(heading_deg);
    const int code = payload.find("\"ok\":true") == std::string::npos ? 409 : 200;
    send_json(code, payload);
  });

  g_server.on("/api/warning-automation", HTTP_POST, []() {
    if (g_instance == nullptr || !g_instance->warning_automation_handler_) {
      send_json(500, "{\"ok\":false,\"error\":\"warning_automation_unavailable\"}");
      return;
    }

    if (!g_server.hasArg("enabled")) {
      send_json(400, "{\"ok\":false,\"error\":\"missing_enabled\"}");
      return;
    }
    const String enabled_arg = g_server.arg("enabled");
    if (enabled_arg != "1" && enabled_arg != "0") {
      send_json(400, "{\"ok\":false,\"error\":\"invalid_enabled\"}");
      return;
    }
    send_json(200, g_instance->warning_automation_handler_(enabled_arg == "1"));
  });

  g_server.begin();
}

void ConfigServer::tick() {
  g_server.handleClient();
}
