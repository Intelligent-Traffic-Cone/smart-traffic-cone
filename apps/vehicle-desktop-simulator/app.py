from __future__ import annotations

import json
import math
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parent
DEFAULT_CONFIG = {
    "cloud_api_base_url": "http://127.0.0.1:8000",
    "vehicle_id": "pi-car-001",
    "poll_interval_ms": 1000,
    "default_speed_kph": 30,
}


class VehicleSimulatorApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.config = self._load_config()
        self.task: dict | None = None
        self.cones: list[dict] = []
        self.route_points: list[dict] = []
        self.segment_lengths: list[float] = []
        self.total_distance_m = 0.0
        self.travelled_m = 0.0
        self.last_motion_at = time.monotonic()
        self.last_upload_at = 0.0
        self.connected = False
        self.running = False
        self.paused = False
        self.request_active = False
        self.task_finished = False
        self.speed_var = tk.DoubleVar(value=float(self.config["default_speed_kph"]))
        self.status_var = tk.StringVar(value="未连接")
        self.task_var = tk.StringVar(value="等待任务")
        self.position_var = tk.StringVar(value="--")
        self.progress_var = tk.StringVar(value="0.0%")
        self._build_ui()
        self.root.after(100, self._motion_tick)

    def _build_ui(self) -> None:
        self.root.title("车辆导航模拟上位机")
        self.root.geometry("1100x720")
        self.root.minsize(900, 620)

        toolbar = ttk.Frame(self.root, padding=10)
        toolbar.pack(fill=tk.X)
        ttk.Label(toolbar, text="云端地址").grid(row=0, column=0, sticky=tk.W)
        self.api_entry = ttk.Entry(toolbar, width=30)
        self.api_entry.insert(0, self.config["cloud_api_base_url"])
        self.api_entry.grid(row=1, column=0, padx=(0, 10), sticky=tk.EW)
        ttk.Label(toolbar, text="车辆编号").grid(row=0, column=1, sticky=tk.W)
        self.vehicle_entry = ttk.Entry(toolbar, width=18)
        self.vehicle_entry.insert(0, self.config["vehicle_id"])
        self.vehicle_entry.grid(row=1, column=1, padx=(0, 10), sticky=tk.EW)
        ttk.Button(toolbar, text="连接", command=self.connect).grid(row=1, column=2, padx=4)
        ttk.Button(toolbar, text="开始", command=self.start).grid(row=1, column=3, padx=4)
        ttk.Button(toolbar, text="暂停/继续", command=self.toggle_pause).grid(row=1, column=4, padx=4)
        ttk.Button(toolbar, text="停止", command=self.stop).grid(row=1, column=5, padx=4)
        ttk.Label(toolbar, text="速度 km/h").grid(row=0, column=6, sticky=tk.W)
        ttk.Scale(toolbar, from_=5, to=80, variable=self.speed_var, orient=tk.HORIZONTAL).grid(
            row=1, column=6, padx=(8, 0), sticky=tk.EW
        )
        toolbar.columnconfigure(0, weight=2)
        toolbar.columnconfigure(1, weight=1)
        toolbar.columnconfigure(6, weight=1)

        content = ttk.Panedwindow(self.root, orient=tk.HORIZONTAL)
        content.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))
        map_frame = ttk.Frame(content)
        side_frame = ttk.Frame(content, padding=(12, 4))
        content.add(map_frame, weight=4)
        content.add(side_frame, weight=2)

        self.canvas = tk.Canvas(map_frame, bg="#10151d", highlightthickness=1, highlightbackground="#303947")
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda _event: self._draw_map())

        self._info_row(side_frame, "连接状态", self.status_var)
        self._info_row(side_frame, "当前任务", self.task_var)
        self._info_row(side_frame, "当前位置", self.position_var)
        self._info_row(side_frame, "行驶进度", self.progress_var)
        ttk.Separator(side_frame).pack(fill=tk.X, pady=12)
        ttk.Label(side_frame, text="运行日志").pack(anchor=tk.W)
        self.log_text = tk.Text(side_frame, height=22, state=tk.DISABLED, bg="#151b24", fg="#dce8f7")
        self.log_text.pack(fill=tk.BOTH, expand=True, pady=(6, 0))

    def _info_row(self, parent: ttk.Frame, label: str, variable: tk.StringVar) -> None:
        frame = ttk.Frame(parent)
        frame.pack(fill=tk.X, pady=6)
        ttk.Label(frame, text=label, width=10).pack(side=tk.LEFT)
        ttk.Label(frame, textvariable=variable, wraplength=260).pack(side=tk.LEFT, fill=tk.X, expand=True)

    def connect(self) -> None:
        self.config["cloud_api_base_url"] = self.api_entry.get().strip().rstrip("/")
        self.config["vehicle_id"] = self.vehicle_entry.get().strip()
        if not self.config["cloud_api_base_url"] or not self.config["vehicle_id"]:
            messagebox.showerror("配置错误", "云端地址和车辆编号不能为空。")
            return
        self.status_var.set("连接中")
        self._log("开始连接云端")
        self._poll_cloud()

    def start(self) -> None:
        if not self.task:
            messagebox.showinfo("等待任务", "请先连接并等待管理 Web 下发路线。")
            return
        if self.travelled_m >= self.total_distance_m:
            self.travelled_m = 0.0
        self.task_finished = False
        self.running = True
        self.paused = False
        self.last_motion_at = time.monotonic()
        self._log("开始模拟行驶")

    def toggle_pause(self) -> None:
        if not self.running:
            return
        self.paused = not self.paused
        self.last_motion_at = time.monotonic()
        self._log("已暂停" if self.paused else "继续行驶")
        self._upload_position(force=True)

    def stop(self) -> None:
        if not self.task:
            return
        self.running = False
        self.paused = False
        self.task_finished = True
        self._log("车辆已停止")
        self._upload_position(force=True, status="stopped")

    def _poll_cloud(self) -> None:
        if self.request_active:
            return
        self.request_active = True

        def worker() -> None:
            try:
                task = self._request(
                    "GET",
                    f"/api/vehicles/{self.config['vehicle_id']}/navigation-tasks/current",
                )
                layers = self._request("GET", "/api/map/layers")
                self.root.after(0, lambda: self._apply_poll_result(task, layers, None))
            except (HTTPError, URLError, TimeoutError, ValueError) as error:
                self.root.after(0, lambda error=error: self._apply_poll_result(None, None, error))

        threading.Thread(target=worker, daemon=True).start()

    def _apply_poll_result(self, task: dict | None, layers: dict | None, error: Exception | None) -> None:
        self.request_active = False
        if error:
            if self.connected:
                self._log(f"连接中断：{error}")
            self.connected = False
            self.status_var.set("离线，自动重试")
        else:
            if not self.connected:
                self._log("云端连接成功")
            self.connected = True
            self.status_var.set("在线")
            self.cones = layers.get("cones", []) if layers else []
            if task and task.get("task_id") != (self.task or {}).get("task_id"):
                self._accept_task(task)
            self._draw_map()
            if self.task and not self.task_finished:
                self._upload_position()
            else:
                self._upload_idle()
        self.root.after(int(self.config["poll_interval_ms"]), self._poll_cloud)

    def _accept_task(self, task: dict) -> None:
        points = task.get("route", {}).get("points", [])
        if len(points) < 2 or any(
            point.get("longitude") is None or point.get("latitude") is None for point in points
        ):
            self._log("收到非法路线，已忽略")
            return
        self.task = task
        self.route_points = points
        self.segment_lengths = [
            self._distance_m(points[index - 1], points[index])
            for index in range(1, len(points))
        ]
        self.total_distance_m = sum(self.segment_lengths)
        self.travelled_m = 0.0
        self.running = False
        self.paused = False
        self.task_finished = False
        self.task_var.set(f"{task['task_id']} / {task['route']['name']}")
        self._log(f"已领取任务 {task['task_id']}，路线长度 {self.total_distance_m:.0f} 米")
        self._update_position_labels()
        self._draw_map()
        self._upload_position(force=True, status="accepted")

    def _motion_tick(self) -> None:
        now = time.monotonic()
        elapsed = min(0.25, now - self.last_motion_at)
        self.last_motion_at = now
        if self.running and not self.paused and self.route_points:
            self.travelled_m = min(
                self.total_distance_m,
                self.travelled_m + self.speed_var.get() / 3.6 * elapsed,
            )
            if self.travelled_m >= self.total_distance_m:
                self.running = False
                self.task_finished = True
                self._log("已到达终点")
                self._upload_position(force=True, status="completed")
            self._update_position_labels()
            self._draw_map()
            self._upload_position()
        self.root.after(100, self._motion_tick)

    def _upload_position(self, force: bool = False, status: str | None = None) -> None:
        if not self.connected or not self.route_points or not self.task:
            return
        now = time.monotonic()
        if not force and now - self.last_upload_at < 1.0:
            return
        self.last_upload_at = now
        location, heading = self._current_location()
        if status is None:
            status = "paused" if self.paused else ("running" if self.running else "accepted")
        payload = {
            "task_id": self.task["task_id"],
            "location": {**location, "accuracy_m": 3, "has_fix": True},
            "speed_kph": self.speed_var.get() if self.running and not self.paused else 0,
            "heading_deg": heading,
            "progress_percent": self._progress_percent(),
            "status": status,
        }

        def worker() -> None:
            try:
                self._request(
                    "POST",
                    f"/api/vehicles/{self.config['vehicle_id']}/position",
                    payload,
                )
            except (HTTPError, URLError, TimeoutError, ValueError) as error:
                self.root.after(0, lambda error=error: self._mark_upload_failed(error))

        threading.Thread(target=worker, daemon=True).start()

    def _upload_idle(self) -> None:
        now = time.monotonic()
        if now - self.last_upload_at < 1.0:
            return
        self.last_upload_at = now
        payload = {
            "task_id": None,
            "location": {"longitude": None, "latitude": None, "accuracy_m": None, "has_fix": False},
            "speed_kph": 0,
            "heading_deg": None,
            "progress_percent": 0,
            "status": "idle",
        }

        def worker() -> None:
            try:
                self._request(
                    "POST",
                    f"/api/vehicles/{self.config['vehicle_id']}/position",
                    payload,
                )
            except (HTTPError, URLError, TimeoutError, ValueError) as error:
                self.root.after(0, lambda error=error: self._mark_upload_failed(error))

        threading.Thread(target=worker, daemon=True).start()

    def _mark_upload_failed(self, error: Exception) -> None:
        if isinstance(error, HTTPError) and error.code == 404 and self.task:
            self._log("云端任务已失效，等待重新下发")
            self.task = None
            self.route_points = []
            self.segment_lengths = []
            self.task_finished = False
            self.running = False
            self.task_var.set("等待任务")
            self._draw_map()
            return
        self.connected = False
        self.status_var.set("上报失败，自动重试")
        self._log(f"位置上报失败：{error}")

    def _current_location(self) -> tuple[dict, float]:
        remaining = self.travelled_m
        for index, length in enumerate(self.segment_lengths):
            if remaining <= length or index == len(self.segment_lengths) - 1:
                ratio = 0 if length == 0 else min(1.0, remaining / length)
                start = self.route_points[index]
                end = self.route_points[index + 1]
                location = {
                    "longitude": start["longitude"] + (end["longitude"] - start["longitude"]) * ratio,
                    "latitude": start["latitude"] + (end["latitude"] - start["latitude"]) * ratio,
                }
                heading = (
                    math.degrees(
                        math.atan2(
                            end["longitude"] - start["longitude"],
                            end["latitude"] - start["latitude"],
                        )
                    )
                    + 360
                ) % 360
                return location, heading
            remaining -= length
        last = self.route_points[-1]
        return {"longitude": last["longitude"], "latitude": last["latitude"]}, 0

    def _draw_map(self) -> None:
        self.canvas.delete("all")
        if not self.route_points and not self.cones:
            self.canvas.create_text(
                self.canvas.winfo_width() / 2,
                self.canvas.winfo_height() / 2,
                text="连接云端并等待路线任务",
                fill="#9aa8b7",
                font=("sans", 16),
            )
            return
        points = self.route_points + [item["location"] for item in self.cones if item["location"].get("has_fix")]
        if not points:
            return
        longitudes = [point["longitude"] for point in points]
        latitudes = [point["latitude"] for point in points]
        min_lng, max_lng = min(longitudes), max(longitudes)
        min_lat, max_lat = min(latitudes), max(latitudes)
        padding = 50
        width = max(1, self.canvas.winfo_width() - padding * 2)
        height = max(1, self.canvas.winfo_height() - padding * 2)

        def project(point: dict) -> tuple[float, float]:
            lng_span = max(max_lng - min_lng, 0.0001)
            lat_span = max(max_lat - min_lat, 0.0001)
            x = padding + (point["longitude"] - min_lng) / lng_span * width
            y = padding + (max_lat - point["latitude"]) / lat_span * height
            return x, y

        if len(self.route_points) >= 2:
            coordinates = [coordinate for point in self.route_points for coordinate in project(point)]
            self.canvas.create_line(*coordinates, fill="#4aa8ff", width=5, smooth=True)
        for cone in self.cones:
            location = cone["location"]
            if not location.get("has_fix"):
                continue
            x, y = project(location)
            color = {
                "low": "#43d58d",
                "medium": "#f2b84b",
                "high": "#ff4f5e",
                "critical": "#ff4f5e",
            }.get(cone.get("current_risk_level"), "#9aa8b7")
            self.canvas.create_polygon(x, y - 11, x - 8, y + 9, x + 8, y + 9, fill=color, outline="")
        if self.route_points:
            location, _heading = self._current_location()
            x, y = project(location)
            self.canvas.create_rectangle(x - 14, y - 8, x + 14, y + 8, fill="#ffffff", outline="#4aa8ff", width=2)

    def _update_position_labels(self) -> None:
        if not self.route_points:
            return
        location, _heading = self._current_location()
        self.position_var.set(f"{location['longitude']:.6f}, {location['latitude']:.6f}")
        self.progress_var.set(f"{self._progress_percent():.1f}%")

    def _progress_percent(self) -> float:
        if self.total_distance_m <= 0:
            return 0.0
        return min(100.0, self.travelled_m / self.total_distance_m * 100)

    def _request(self, method: str, path: str, payload: dict | None = None) -> dict | None:
        body = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = Request(
            f"{self.config['cloud_api_base_url']}{path}",
            data=body,
            method=method,
            headers={"Content-Type": "application/json"},
        )
        with urlopen(request, timeout=3) as response:
            content = response.read()
        return json.loads(content.decode("utf-8")) if content else None

    def _distance_m(self, a: dict, b: dict) -> float:
        lat_scale = 111_320.0
        lng_scale = 111_320.0 * math.cos(math.radians((a["latitude"] + b["latitude"]) / 2))
        return math.hypot(
            (a["longitude"] - b["longitude"]) * lng_scale,
            (a["latitude"] - b["latitude"]) * lat_scale,
        )

    def _log(self, message: str) -> None:
        self.log_text.configure(state=tk.NORMAL)
        self.log_text.insert(tk.END, f"{time.strftime('%H:%M:%S')}  {message}\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def _load_config(self) -> dict:
        config = dict(DEFAULT_CONFIG)
        config_path = ROOT / "config.local.json"
        if config_path.exists():
            config.update(json.loads(config_path.read_text(encoding="utf-8")))
        return config


def main() -> None:
    root = tk.Tk()
    VehicleSimulatorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
