# Vehicle Dispatch Contract

This contract defines the LAN HTTP flow between dispatch-web, cloud-api, and
the Linux/Raspberry Pi vehicle desktop simulator.

## Candidate Routes

```http
GET /api/routes/candidates
```

The response contains exactly two configured routes. Each route includes its
polyline points, distance, current risk score, recommendation reasons, and a
`recommended` flag. Lower scores are preferred.

Route points are loaded from `services/cloud-api/app/routes.json`. Replace the
sample coordinates with the two real AMap route polylines before the final
demonstration.

## Dispatch a Route

```http
POST /api/vehicles/{vehicle_id}/navigation-tasks
Content-Type: application/json

{"route_id": "route-b"}
```

Creating a task replaces any unfinished task for the same vehicle. The task
stores a snapshot of the selected route.

## Vehicle Polling

```http
GET /api/vehicles/{vehicle_id}/navigation-tasks/current
```

The first successful read changes a pending task to `accepted`. Repeated reads
return the same task and do not create duplicates.

## Position Upload

```http
POST /api/vehicles/{vehicle_id}/position
```

Example:

```json
{
  "task_id": "task-000001",
  "location": {
    "longitude": 116.39744,
    "latitude": 39.90931,
    "accuracy_m": 3,
    "has_fix": true
  },
  "speed_kph": 28,
  "heading_deg": 110,
  "progress_percent": 42.5,
  "status": "running"
}
```

The cloud keeps the latest 500 trace points per vehicle. A vehicle is online
when its latest upload is no older than five seconds.

## Vehicle Monitoring

```http
GET /api/vehicles
```

The dispatch Web polls this endpoint once per second to display connection
state, task state, current position, progress, and trace.
