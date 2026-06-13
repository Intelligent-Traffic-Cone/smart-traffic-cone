from __future__ import annotations

from datetime import datetime, timedelta, timezone
from itertools import count
import json
from math import cos, radians, sqrt
from pathlib import Path

from .models import (
    AlertHandleIn,
    AlertRecord,
    ConeRecord,
    ConeStatus,
    ConeTelemetryIn,
    ConeTelemetryRecord,
    ExternalSyncRecord,
    LaneAction,
    LaneGuidance,
    LocationPayload,
    MapConeLayer,
    MapEventLayer,
    MapLayersResponse,
    NavigationTaskIn,
    NavigationTaskRecord,
    NavigationSessionIn,
    NavigationSessionRecord,
    NearbyConeSummary,
    RiskLevel,
    RouteAssessment,
    RouteCandidate,
    RouteCandidatesResponse,
    RiskSegment,
    RoadEventIn,
    RoadEventRecord,
    VehicleDynamicAdvice,
    VehiclePositionIn,
    VehiclePositionPoint,
    VehiclePositionTickIn,
    VehicleRecord,
)


class InMemoryStore:
    def __init__(self) -> None:
        self._telemetry_counter = count(1)
        self._event_counter = count(1)
        self._alert_counter = count(1)
        self._sync_counter = count(1)
        self._session_counter = count(1)
        self._task_counter = count(1)
        self.cones: dict[str, ConeRecord] = {}
        self.telemetry: list[ConeTelemetryRecord] = []
        self.events: dict[str, RoadEventRecord] = {}
        self.alerts: dict[str, AlertRecord] = {}
        self.syncs: dict[str, ExternalSyncRecord] = {}
        self.risk_segments: dict[str, RiskSegment] = {}
        self.navigation_sessions: dict[str, NavigationSessionRecord] = {}
        self.routes = self._load_routes()
        self.navigation_tasks: dict[str, NavigationTaskRecord] = {}
        self.vehicles: dict[str, VehicleRecord] = {}
        self.reset_demo_data()

    def reset_demo_data(self) -> MapLayersResponse:
        self.cones.clear()
        self.telemetry.clear()
        self.events.clear()
        self.alerts.clear()
        self.syncs.clear()
        self.risk_segments.clear()
        self.navigation_sessions.clear()
        self.navigation_tasks.clear()
        self.vehicles.clear()
        self._telemetry_counter = count(1)
        self._event_counter = count(1)
        self._alert_counter = count(1)
        self._sync_counter = count(1)
        self._session_counter = count(1)
        self._task_counter = count(1)

        now = datetime.now(timezone.utc)
        cone_points = [
            ("cone-demo-001", 116.39725, 39.90940, RiskLevel.medium),
            ("cone-demo-002", 116.39755, 39.90932, RiskLevel.high),
            ("cone-demo-003", 116.39786, 39.90920, RiskLevel.high),
            ("cone-demo-004", 116.39820, 39.90905, RiskLevel.medium),
            ("cone-demo-005", 116.39852, 39.90894, RiskLevel.low),
        ]
        for cone_id, lng, lat, risk in cone_points:
            self.cones[cone_id] = ConeRecord(
                cone_id=cone_id,
                status=ConeStatus.deployed,
                last_seen_at=now,
                location=LocationPayload(
                    longitude=lng,
                    latitude=lat,
                    accuracy_m=2.5,
                    has_fix=True,
                ),
                current_risk_level=risk,
            )

        event = self.create_event(
            RoadEventIn(
                event_type="construction",
                road_name="人民路北向主路",
                level=RiskLevel.high,
                boundary=[
                    LocationPayload(longitude=116.39718, latitude=39.90950, has_fix=True),
                    LocationPayload(longitude=116.39858, latitude=39.90905, has_fix=True),
                    LocationPayload(longitude=116.39845, latitude=39.90872, has_fix=True),
                    LocationPayload(longitude=116.39706, latitude=39.90912, has_fix=True),
                ],
                related_cone_ids=[item[0] for item in cone_points],
                affected_lanes=["right_lane", "shoulder"],
                description="右侧施工占道，建议车辆提前向左合流。",
            )
        )
        event.status = "active"

        segment = RiskSegment(
            segment_id="seg-demo-001",
            event_id=event.event_id,
            road_name=event.road_name,
            level=RiskLevel.high,
            start=LocationPayload(longitude=116.39688, latitude=39.90956, has_fix=True),
            end=LocationPayload(longitude=116.39880, latitude=39.90886, has_fix=True),
            affected_lanes=["right_lane", "shoulder"],
            suggested_action="prepare_left_merge_and_slow_down",
            speed_limit_kph=30,
        )
        self.risk_segments[segment.segment_id] = segment

        self.create_alert(
            alert_type="vehicle_approach",
            level=RiskLevel.high,
            message="施工区上游 300m 建议车辆提前减速并向左合流。",
            event_id=event.event_id,
            cone_id="cone-demo-002",
        )
        return self.map_layers()

    def ingest_telemetry(self, cone_id: str, payload: ConeTelemetryIn) -> ConeTelemetryRecord:
        now = datetime.now(timezone.utc)
        payload_data = payload.model_dump()
        payload_data["cone_id"] = cone_id
        record = ConeTelemetryRecord(
            **payload_data,
            telemetry_id=f"tel-{next(self._telemetry_counter):06d}",
            received_at=now,
        )
        self.telemetry.append(record)
        self.cones[cone_id] = ConeRecord(
            cone_id=cone_id,
            last_seen_at=payload.reported_at,
            location=payload.location,
            current_risk_level=self._estimate_risk(payload),
        )
        return record

    def create_event(self, payload: RoadEventIn) -> RoadEventRecord:
        now = datetime.now(timezone.utc)
        event = RoadEventRecord(
            **payload.model_dump(),
            event_id=f"evt-{next(self._event_counter):06d}",
            created_at=now,
            updated_at=now,
        )
        self.events[event.event_id] = event
        return event

    def list_events(self, status: str | None = None, level: RiskLevel | None = None) -> list[RoadEventRecord]:
        events = list(self.events.values())
        if status:
            events = [event for event in events if event.status == status]
        if level:
            events = [event for event in events if event.level == level]
        return events

    def create_alert(
        self,
        alert_type: str,
        level: RiskLevel,
        message: str,
        cone_id: str | None = None,
        event_id: str | None = None,
    ) -> AlertRecord:
        alert = AlertRecord(
            alert_id=f"alt-{next(self._alert_counter):06d}",
            cone_id=cone_id,
            event_id=event_id,
            alert_type=alert_type,
            level=level,
            message=message,
            created_at=datetime.now(timezone.utc),
        )
        self.alerts[alert.alert_id] = alert
        return alert

    def handle_alert(self, alert_id: str, payload: AlertHandleIn) -> AlertRecord | None:
        alert = self.alerts.get(alert_id)
        if not alert:
            return None
        alert.status = payload.action
        alert.handler = payload.handler
        alert.handled_at = datetime.now(timezone.utc)
        return alert

    def sync_event(self, event_id: str, target_platform: str = "vehicle-warning") -> ExternalSyncRecord | None:
        event = self.events.get(event_id)
        if not event:
            return None
        record = ExternalSyncRecord(
            sync_id=f"sync-{next(self._sync_counter):06d}",
            event_id=event_id,
            target_platform=target_platform,
            status="queued",
            payload={
                "event_id": event.event_id,
                "event_type": event.event_type,
                "road_name": event.road_name,
                "level": event.level,
                "boundary": [point.model_dump() for point in event.boundary],
                "affected_lanes": event.affected_lanes,
                "suggested_action": "slow_down_and_prepare_to_merge",
            },
            synced_at=datetime.now(timezone.utc),
        )
        self.syncs[record.sync_id] = record
        return record

    def map_layers(self, bbox: str | None = None) -> MapLayersResponse:
        return MapLayersResponse(
            generated_at=datetime.now(timezone.utc),
            cones=[
                MapConeLayer(
                    cone_id=cone.cone_id,
                    status=cone.status,
                    location=cone.location,
                    current_risk_level=cone.current_risk_level,
                    last_seen_at=cone.last_seen_at,
                )
                for cone in self.cones.values()
                if self._inside_bbox(cone.location, bbox)
            ],
            events=[
                MapEventLayer(
                    event_id=event.event_id,
                    event_type=event.event_type,
                    road_name=event.road_name,
                    level=event.level,
                    status=event.status,
                    boundary=event.boundary,
                    affected_lanes=event.affected_lanes,
                    description=event.description,
                )
                for event in self.events.values()
            ],
            risk_segments=list(self.risk_segments.values()),
            vehicle_warnings=[sync.payload for sync in self.syncs.values()],
        )

    def assess_routes(self) -> RouteCandidatesResponse:
        assessments: list[RouteAssessment] = []
        risk_weight = {
            RiskLevel.low: 1.0,
            RiskLevel.medium: 8.0,
            RiskLevel.high: 24.0,
            RiskLevel.critical: 45.0,
        }
        for route in self.routes.values():
            distance_m = sum(
                self._distance_m(route.points[index - 1], route.points[index])
                for index in range(1, len(route.points))
            )
            score = distance_m / 1000.0
            nearby: list[str] = []
            reasons: list[str] = []
            for cone in self.cones.values():
                distance = self._distance_to_route_m(cone.location, route.points)
                if distance > 120:
                    continue
                nearby.append(cone.cone_id)
                proximity = max(0.15, 1.0 - distance / 120.0)
                penalty = risk_weight[cone.current_risk_level] * proximity
                score += penalty
                if cone.current_risk_level in {RiskLevel.high, RiskLevel.critical}:
                    reasons.append(
                        f"{cone.cone_id} {cone.current_risk_level.value} 风险，距路线约 {distance:.0f} 米"
                    )
            if not reasons:
                reasons.append("路线附近未发现高风险路锥")
            assessments.append(
                RouteAssessment(
                    **route.model_dump(),
                    distance_m=round(distance_m, 1),
                    risk_score=round(score, 2),
                    nearby_cone_ids=nearby,
                    reasons=reasons[:3],
                )
            )
        assessments.sort(key=lambda item: item.risk_score)
        assessments[0].recommended = True
        return RouteCandidatesResponse(
            generated_at=datetime.now(timezone.utc),
            recommended_route_id=assessments[0].route_id,
            routes=assessments,
        )

    def create_navigation_task(
        self,
        vehicle_id: str,
        payload: NavigationTaskIn,
    ) -> NavigationTaskRecord | None:
        route = self.routes.get(payload.route_id)
        if not route:
            return None
        now = datetime.now(timezone.utc)
        for task in self.navigation_tasks.values():
            if task.vehicle_id == vehicle_id and task.status in {"pending", "accepted", "running", "paused"}:
                task.status = "replaced"
        task = NavigationTaskRecord(
            task_id=f"task-{next(self._task_counter):06d}",
            vehicle_id=vehicle_id,
            route=route.model_copy(deep=True),
            created_at=now,
        )
        self.navigation_tasks[task.task_id] = task
        vehicle = self.vehicles.get(vehicle_id) or VehicleRecord(vehicle_id=vehicle_id)
        vehicle.current_task_id = task.task_id
        vehicle.status = "task_pending"
        self.vehicles[vehicle_id] = vehicle
        return task

    def current_navigation_task(self, vehicle_id: str) -> NavigationTaskRecord | None:
        tasks = [
            task
            for task in self.navigation_tasks.values()
            if task.vehicle_id == vehicle_id and task.status in {"pending", "accepted", "running", "paused"}
        ]
        if not tasks:
            return None
        task = max(tasks, key=lambda item: item.created_at)
        if task.status == "pending":
            task.status = "accepted"
            task.accepted_at = datetime.now(timezone.utc)
        return task

    def update_vehicle_position(
        self,
        vehicle_id: str,
        payload: VehiclePositionIn,
    ) -> VehicleRecord | None:
        task = None
        if payload.task_id:
            task = self.navigation_tasks.get(payload.task_id)
            if not task or task.vehicle_id != vehicle_id:
                return None
        now = datetime.now(timezone.utc)
        vehicle = self.vehicles.get(vehicle_id) or VehicleRecord(vehicle_id=vehicle_id)
        vehicle.status = payload.status
        vehicle.online = True
        vehicle.current_task_id = payload.task_id
        vehicle.location = payload.location
        vehicle.speed_kph = payload.speed_kph
        vehicle.heading_deg = payload.heading_deg
        vehicle.progress_percent = payload.progress_percent
        vehicle.last_seen_at = now
        if payload.location.has_fix:
            vehicle.trace.append(
                VehiclePositionPoint(
                    location=payload.location,
                    speed_kph=payload.speed_kph,
                    reported_at=now,
                )
            )
        vehicle.trace = vehicle.trace[-500:]
        self.vehicles[vehicle_id] = vehicle
        if task:
            task.status = payload.status if payload.status in {"paused", "completed", "stopped"} else "running"
            if task.status == "completed":
                task.completed_at = now
        return vehicle

    def list_vehicles(self) -> list[VehicleRecord]:
        now = datetime.now(timezone.utc)
        result: list[VehicleRecord] = []
        for vehicle in self.vehicles.values():
            item = vehicle.model_copy(deep=True)
            item.online = bool(item.last_seen_at and now - item.last_seen_at <= timedelta(seconds=5))
            if not item.online and item.status not in {"completed", "stopped"}:
                item.status = "offline"
            result.append(item)
        return result

    def create_navigation_session(
        self,
        vehicle_id: str,
        payload: NavigationSessionIn,
    ) -> NavigationSessionRecord:
        now = datetime.now(timezone.utc)
        active_events = [event for event in self.events.values() if event.status != "closed"]
        blocked_boundaries = [event.boundary for event in active_events if event.boundary]
        risk_segments = list(self.risk_segments.values())
        highest = self._highest_risk([event.level for event in active_events] + [RiskLevel.low])
        payload_data = payload.model_dump()
        payload_data["vehicle_id"] = vehicle_id
        session = NavigationSessionRecord(
            **payload_data,
            session_id=f"nav-{next(self._session_counter):06d}",
            started_at=now,
            risk_summary=f"当前路线存在 {len(risk_segments)} 个风险路段，最高风险等级为 {highest.value}。",
            blocked_boundaries=blocked_boundaries,
            risk_segments=risk_segments,
            avoidance_strategy="route_around_high_risk_segments_then_merge_left_near_cones",
        )
        self.navigation_sessions[session.session_id] = session
        return session

    def navigation_tick(
        self,
        vehicle_id: str,
        session_id: str,
        payload: VehiclePositionTickIn,
    ) -> VehicleDynamicAdvice | None:
        session = self.navigation_sessions.get(session_id)
        if not session or session.vehicle_id != vehicle_id:
            return None

        nearby = self._nearby_cones(payload.location)
        active_segments = self._active_segments(payload.location)
        risk_level = self._highest_risk(
            [cone.risk_level for cone in nearby]
            + [segment.level for segment in active_segments]
            + [RiskLevel.low]
        )

        if risk_level in {RiskLevel.high, RiskLevel.critical}:
            route_adjustment = "前方施工占道，保持主路线但提前向左侧可通行车道合流。"
            lane_guidance = LaneGuidance(
                action=LaneAction.merge_left_now if payload.current_lane == "right_lane" else LaneAction.keep_lane,
                target_lane="left_lane",
                reason="右侧车道受智能路锥边界和施工区影响。",
            )
            message = "进入车道级领航区，请降低车速并关注右侧路锥边界。"
        elif risk_level == RiskLevel.medium:
            route_adjustment = "前方有中风险路锥阵列，建议保持当前路线并准备左侧合流。"
            lane_guidance = LaneGuidance(
                action=LaneAction.prepare_left_merge,
                target_lane="left_lane",
                reason="前方路锥距离接近，提前预留合流空间。",
            )
            message = "请准备减速，等待后续车道建议。"
        else:
            route_adjustment = "当前路线可继续通行，暂无绕行要求。"
            lane_guidance = LaneGuidance(
                action=LaneAction.keep_lane,
                target_lane=payload.current_lane,
                reason="附近未发现高风险路锥或封控边界。",
            )
            message = "保持当前车道。"

        generated_at = datetime.now(timezone.utc)
        return VehicleDynamicAdvice(
            session_id=session_id,
            vehicle_id=vehicle_id,
            generated_at=generated_at,
            expires_at=generated_at + timedelta(seconds=3),
            risk_level=risk_level,
            route_adjustment=route_adjustment,
            lane_guidance=lane_guidance,
            nearby_cones=nearby,
            active_risk_segments=active_segments,
            message=message,
        )

    def _estimate_risk(self, payload: ConeTelemetryIn) -> RiskLevel:
        distances = [
            item.distance_m
            for item in payload.ultrasonic
            if item.distance_m is not None and not item.timed_out
        ]
        if distances and min(distances) < 10:
            return RiskLevel.critical
        if distances and min(distances) < 20:
            return RiskLevel.high
        if distances and min(distances) < 35:
            return RiskLevel.medium
        return RiskLevel.low

    def _nearby_cones(self, location: LocationPayload) -> list[NearbyConeSummary]:
        if location.longitude is None or location.latitude is None:
            return []
        items: list[NearbyConeSummary] = []
        for cone in self.cones.values():
            if cone.location.longitude is None or cone.location.latitude is None:
                continue
            distance_m = self._distance_m(location, cone.location)
            if distance_m > 450:
                continue
            items.append(
                NearbyConeSummary(
                    cone_id=cone.cone_id,
                    distance_m=round(distance_m, 1),
                    bearing="ahead" if cone.location.longitude >= location.longitude else "behind",
                    status=cone.status,
                    risk_level=cone.current_risk_level,
                    lane_hint="right_boundary" if cone.current_risk_level != RiskLevel.low else "roadside",
                )
            )
        return sorted(items, key=lambda item: item.distance_m)[:6]

    def _active_segments(self, location: LocationPayload) -> list[RiskSegment]:
        if location.longitude is None or location.latitude is None:
            return list(self.risk_segments.values())
        active: list[RiskSegment] = []
        for segment in self.risk_segments.values():
            midpoint = LocationPayload(
                longitude=((segment.start.longitude or 0) + (segment.end.longitude or 0)) / 2,
                latitude=((segment.start.latitude or 0) + (segment.end.latitude or 0)) / 2,
                has_fix=True,
            )
            if self._distance_m(location, midpoint) <= 650:
                active.append(segment)
        return active

    def _inside_bbox(self, location: LocationPayload, bbox: str | None) -> bool:
        if not bbox or location.longitude is None or location.latitude is None:
            return True
        try:
            min_lng, min_lat, max_lng, max_lat = [float(item) for item in bbox.split(",")]
        except ValueError:
            return True
        return min_lng <= location.longitude <= max_lng and min_lat <= location.latitude <= max_lat

    def _distance_m(self, a: LocationPayload, b: LocationPayload) -> float:
        if a.longitude is None or a.latitude is None or b.longitude is None or b.latitude is None:
            return 999999.0
        lat_scale = 111_320.0
        lng_scale = 111_320.0 * cos(radians((a.latitude + b.latitude) / 2))
        dx = (a.longitude - b.longitude) * lng_scale
        dy = (a.latitude - b.latitude) * lat_scale
        return sqrt(dx * dx + dy * dy)

    def _distance_to_route_m(
        self,
        point: LocationPayload,
        route_points: list[LocationPayload],
    ) -> float:
        return min(
            self._distance_to_segment_m(point, route_points[index - 1], route_points[index])
            for index in range(1, len(route_points))
        )

    def _distance_to_segment_m(
        self,
        point: LocationPayload,
        start: LocationPayload,
        end: LocationPayload,
    ) -> float:
        if any(
            value is None
            for value in (
                point.longitude,
                point.latitude,
                start.longitude,
                start.latitude,
                end.longitude,
                end.latitude,
            )
        ):
            return 999999.0
        lat_scale = 111_320.0
        lng_scale = 111_320.0 * cos(radians(point.latitude or 0))
        px = (point.longitude or 0) * lng_scale
        py = (point.latitude or 0) * lat_scale
        ax = (start.longitude or 0) * lng_scale
        ay = (start.latitude or 0) * lat_scale
        bx = (end.longitude or 0) * lng_scale
        by = (end.latitude or 0) * lat_scale
        dx = bx - ax
        dy = by - ay
        if dx == 0 and dy == 0:
            return sqrt((px - ax) ** 2 + (py - ay) ** 2)
        ratio = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
        nearest_x = ax + ratio * dx
        nearest_y = ay + ratio * dy
        return sqrt((px - nearest_x) ** 2 + (py - nearest_y) ** 2)

    def _load_routes(self) -> dict[str, RouteCandidate]:
        route_path = Path(__file__).with_name("routes.json")
        raw_routes = json.loads(route_path.read_text(encoding="utf-8"))
        routes = [RouteCandidate.model_validate(item) for item in raw_routes]
        if len(routes) != 2:
            raise ValueError("routes.json must define exactly two routes")
        return {route.route_id: route for route in routes}

    def _highest_risk(self, levels: list[RiskLevel]) -> RiskLevel:
        rank = {
            RiskLevel.low: 0,
            RiskLevel.medium: 1,
            RiskLevel.high: 2,
            RiskLevel.critical: 3,
        }
        return max(levels, key=lambda level: rank[level])


store = InMemoryStore()
