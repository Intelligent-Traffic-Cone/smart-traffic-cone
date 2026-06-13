from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Any

from pydantic import BaseModel, Field


class RiskLevel(str, Enum):
    low = "low"
    medium = "medium"
    high = "high"
    critical = "critical"


class ConeStatus(str, Enum):
    inventory = "inventory"
    deployed = "deployed"
    abnormal = "abnormal"
    offline = "offline"


class RoutePreference(str, Enum):
    fastest = "fastest"
    avoid_risk = "avoid_risk"
    shortest = "shortest"


class LaneAction(str, Enum):
    keep_lane = "keep_lane"
    prepare_left_merge = "prepare_left_merge"
    merge_left_now = "merge_left_now"
    prepare_right_merge = "prepare_right_merge"
    merge_right_now = "merge_right_now"
    stop_if_unsafe = "stop_if_unsafe"


class LocationPayload(BaseModel):
    longitude: float | None = None
    latitude: float | None = None
    accuracy_m: float | None = None
    has_fix: bool = False


class UltrasonicChannelPayload(BaseModel):
    channel: int = Field(ge=0, le=3)
    distance_m: float | None = Field(default=None, ge=0)
    timed_out: bool = False
    sample_age_ms: int | None = Field(default=None, ge=0)


class CameraPayload(BaseModel):
    enabled: bool = False
    initialized: bool = False
    frame_available: bool = False
    last_frame_age_ms: int | None = Field(default=None, ge=0)
    frame_count: int = Field(default=0, ge=0)
    image_url: str | None = None


class DeviceHealthPayload(BaseModel):
    gps_status: str = "unknown"
    ultrasonic_status: str = "unknown"
    camera_status: str = "unknown"
    network_status: str = "unknown"
    battery_percent: float | None = Field(default=None, ge=0, le=100)


class ConeTelemetryIn(BaseModel):
    cone_id: str
    reported_at: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    location: LocationPayload = Field(default_factory=LocationPayload)
    ultrasonic: list[UltrasonicChannelPayload] = Field(default_factory=list)
    camera: CameraPayload = Field(default_factory=CameraPayload)
    device: DeviceHealthPayload = Field(default_factory=DeviceHealthPayload)
    raw_payload: dict[str, Any] = Field(default_factory=dict)


class ConeTelemetryRecord(ConeTelemetryIn):
    telemetry_id: str
    received_at: datetime


class ConeRecord(BaseModel):
    cone_id: str
    status: ConeStatus = ConeStatus.deployed
    last_seen_at: datetime | None = None
    location: LocationPayload = Field(default_factory=LocationPayload)
    current_risk_level: RiskLevel = RiskLevel.low


class RoadEventIn(BaseModel):
    event_type: str
    road_name: str
    level: RiskLevel = RiskLevel.medium
    boundary: list[LocationPayload] = Field(default_factory=list)
    related_cone_ids: list[str] = Field(default_factory=list)
    description: str = ""
    affected_lanes: list[str] = Field(default_factory=list)


class RoadEventRecord(RoadEventIn):
    event_id: str
    status: str = "pending_confirmation"
    created_at: datetime
    updated_at: datetime


class AlertRecord(BaseModel):
    alert_id: str
    cone_id: str | None = None
    event_id: str | None = None
    alert_type: str
    level: RiskLevel
    message: str
    status: str = "pending"
    created_at: datetime
    handled_at: datetime | None = None
    handler: str | None = None


class AlertHandleIn(BaseModel):
    handler: str
    action: str = "confirm"
    note: str = ""


class ExternalSyncRecord(BaseModel):
    sync_id: str
    event_id: str
    target_platform: str
    status: str = "queued"
    payload: dict[str, Any] = Field(default_factory=dict)
    synced_at: datetime


class RiskSegment(BaseModel):
    segment_id: str
    event_id: str
    road_name: str
    level: RiskLevel
    start: LocationPayload
    end: LocationPayload
    affected_lanes: list[str] = Field(default_factory=list)
    suggested_action: str = "slow_down"
    speed_limit_kph: int | None = Field(default=None, ge=0)


class MapConeLayer(BaseModel):
    cone_id: str
    status: ConeStatus
    location: LocationPayload
    current_risk_level: RiskLevel
    last_seen_at: datetime | None = None


class MapEventLayer(BaseModel):
    event_id: str
    event_type: str
    road_name: str
    level: RiskLevel
    status: str
    boundary: list[LocationPayload] = Field(default_factory=list)
    affected_lanes: list[str] = Field(default_factory=list)
    description: str = ""


class MapLayersResponse(BaseModel):
    generated_at: datetime
    cones: list[MapConeLayer] = Field(default_factory=list)
    events: list[MapEventLayer] = Field(default_factory=list)
    risk_segments: list[RiskSegment] = Field(default_factory=list)
    vehicle_warnings: list[dict[str, Any]] = Field(default_factory=list)


class RouteCandidate(BaseModel):
    route_id: str
    name: str
    description: str = ""
    points: list[LocationPayload] = Field(min_length=2)


class RouteAssessment(RouteCandidate):
    distance_m: float
    risk_score: float
    nearby_cone_ids: list[str] = Field(default_factory=list)
    reasons: list[str] = Field(default_factory=list)
    recommended: bool = False


class RouteCandidatesResponse(BaseModel):
    generated_at: datetime
    recommended_route_id: str
    routes: list[RouteAssessment] = Field(default_factory=list)


class NavigationTaskIn(BaseModel):
    route_id: str


class NavigationTaskRecord(BaseModel):
    task_id: str
    vehicle_id: str
    route: RouteCandidate
    status: str = "pending"
    created_at: datetime
    accepted_at: datetime | None = None
    completed_at: datetime | None = None


class VehiclePositionIn(BaseModel):
    task_id: str | None = None
    location: LocationPayload
    speed_kph: float = Field(default=0, ge=0)
    heading_deg: float | None = Field(default=None, ge=0, le=360)
    progress_percent: float = Field(default=0, ge=0, le=100)
    status: str = "online"


class VehiclePositionPoint(BaseModel):
    location: LocationPayload
    speed_kph: float = 0
    reported_at: datetime


class VehicleRecord(BaseModel):
    vehicle_id: str
    status: str = "offline"
    online: bool = False
    current_task_id: str | None = None
    location: LocationPayload = Field(default_factory=LocationPayload)
    speed_kph: float = 0
    heading_deg: float | None = None
    progress_percent: float = 0
    last_seen_at: datetime | None = None
    trace: list[VehiclePositionPoint] = Field(default_factory=list)


class NavigationSessionIn(BaseModel):
    vehicle_id: str
    origin: LocationPayload
    destination: LocationPayload
    route_preference: RoutePreference = RoutePreference.avoid_risk


class NavigationSessionRecord(NavigationSessionIn):
    session_id: str
    status: str = "active"
    started_at: datetime
    risk_summary: str = ""
    blocked_boundaries: list[list[LocationPayload]] = Field(default_factory=list)
    risk_segments: list[RiskSegment] = Field(default_factory=list)
    avoidance_strategy: str = "follow_dispatch_risk_segments"


class VehiclePositionTickIn(BaseModel):
    session_id: str
    location: LocationPayload
    speed_kph: float = Field(default=0, ge=0)
    current_lane: str = "unknown"
    heading_deg: float | None = Field(default=None, ge=0, le=360)


class NearbyConeSummary(BaseModel):
    cone_id: str
    distance_m: float
    bearing: str = "ahead"
    status: ConeStatus
    risk_level: RiskLevel
    lane_hint: str = ""


class LaneGuidance(BaseModel):
    action: LaneAction
    target_lane: str
    reason: str
    confidence: str = "demo_rule"


class VehicleDynamicAdvice(BaseModel):
    session_id: str
    vehicle_id: str
    generated_at: datetime
    expires_at: datetime
    risk_level: RiskLevel
    route_adjustment: str
    lane_guidance: LaneGuidance
    nearby_cones: list[NearbyConeSummary] = Field(default_factory=list)
    active_risk_segments: list[RiskSegment] = Field(default_factory=list)
    message: str = ""
