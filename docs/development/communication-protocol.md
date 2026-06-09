# Communication Protocol

## First Runnable Path

The first runnable cloud path is HTTP:

```http
POST /api/cones/{cone_id}/telemetry
```

Payloads follow `contracts/telemetry.schema.json`.

## Retry and Offline Policy

Firmware should treat upload as app-level behavior:

- keep the latest telemetry snapshot in memory;
- retry failed uploads with bounded backoff;
- count consecutive upload failures in Device Health;
- preserve the sensor Module interfaces regardless of transport choice.

Persistent offline buffering is planned but not implemented in the first
skeleton.

## MQTT Reserved Topics

Reserved MQTT topics are documented in `contracts/mqtt-topics.md`. Do not add
firmware MQTT behavior until cloud credentials, QoS, and offline buffering are
specified.

## Vehicle Warning Flow

Cloud publishes vehicle warnings only after a Road Event has an active boundary
and enough freshness or operator confirmation. The contract is documented in
`contracts/vehicle-warning.md`.

## Vehicle Navigation Flow

The Raspberry Pi vehicle simulator uses HTTP polling in the first version:

```http
POST /api/vehicles/{vehicle_id}/navigation-sessions
POST /api/vehicles/{vehicle_id}/navigation-sessions/{session_id}/tick
```

The start request creates a navigation session and returns dispatch risk
segments. During simulated driving, the vehicle sends its current location,
speed, and lane; the cloud returns route adjustment, lane guidance, nearby cone
summaries, and active risk segments.

Payloads are documented in `contracts/vehicle-navigation.md`.
