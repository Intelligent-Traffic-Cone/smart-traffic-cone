# Vehicle Navigation Contract

This contract defines the first HTTP interface between the dispatch cloud and a
Raspberry Pi vehicle-side simulator. The vehicle simulator starts a navigation
session before driving, then polls dynamic advice while moving.

## Start Navigation

```http
POST /api/vehicles/{vehicle_id}/navigation-sessions
```

Request:

```json
{
  "vehicle_id": "pi-car-001",
  "origin": {
    "longitude": 116.39658,
    "latitude": 39.90974,
    "accuracy_m": 3,
    "has_fix": true
  },
  "destination": {
    "longitude": 116.39908,
    "latitude": 39.90866,
    "accuracy_m": 3,
    "has_fix": true
  },
  "route_preference": "avoid_risk"
}
```

Response includes:

- `session_id` for later polling;
- route risk summary;
- blocked boundaries from active Road Events;
- risk segments that the vehicle map should avoid or highlight;
- an avoidance strategy string for first-version UI display.

## Dynamic Driving Advice

```http
POST /api/vehicles/{vehicle_id}/navigation-sessions/{session_id}/tick
```

Request:

```json
{
  "session_id": "nav-000001",
  "location": {
    "longitude": 116.39744,
    "latitude": 39.90931,
    "accuracy_m": 3,
    "has_fix": true
  },
  "speed_kph": 28,
  "current_lane": "right_lane",
  "heading_deg": 110
}
```

Response includes:

- `route_adjustment` for path-level decisions;
- `lane_guidance` for lane-level piloting;
- nearby Smart Traffic Cones with distance and risk;
- active risk segments;
- `expires_at`, after which the vehicle simulator should poll again.

## First-Version Transport

Use HTTP polling every 1-2 seconds. MQTT and WebSocket are reserved for later
iterations.
