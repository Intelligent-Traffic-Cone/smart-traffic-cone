from __future__ import annotations

from datetime import datetime, timedelta, timezone
from itertools import count
from math import cos, radians, sqrt

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
    NavigationSessionIn,
    NavigationSessionRecord,
    NearbyConeSummary,
    RiskLevel,
    RiskSegment,
    RoadEventIn,
    RoadEventRecord,
    VehicleDynamicAdvice,
    VehiclePositionTickIn,
)


class InMemoryStore:
    def __init__(self) -> None:
        self._telemetry_counter = count(1)
        self._event_counter = count(1)
        self._alert_counter = count(1)
        self._sync_counter = count(1)
        self._session_counter = count(1)
        self.cones: dict[str, ConeRecord] = {}
        self.telemetry: list[ConeTelemetryRecord] = []
        self.events: dict[str, RoadEventRecord] = {}
        self.alerts: dict[str, AlertRecord] = {}
        self.syncs: dict[str, ExternalSyncRecord] = {}
        self.risk_segments: dict[str, RiskSegment] = {}
        self.navigation_sessions: dict[str, NavigationSessionRecord] = {}
        self.reset_demo_data()

    def reset_demo_data(self) -> MapLayersResponse:
        self.cones.clear()
        self.telemetry.clear()
        self.events.clear()
        self.alerts.clear()
        self.syncs.clear()
        self.risk_segments.clear()
        self.navigation_sessions.clear()
        self._telemetry_counter = count(1)
        self._event_counter = count(1)
        self._alert_counter = count(1)
        self._sync_counter = count(1)
        self._session_counter = count(1)

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

    def _highest_risk(self, levels: list[RiskLevel]) -> RiskLevel:
        rank = {
            RiskLevel.low: 0,
            RiskLevel.medium: 1,
            RiskLevel.high: 2,
            RiskLevel.critical: 3,
        }
        return max(levels, key=lambda level: rank[level])


store = InMemoryStore()
