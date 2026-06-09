# Vehicle Warning Contract

This contract defines the cloud-to-vehicle warning payload. The Raspberry Pi
vehicle simulator consumes navigation-session and dynamic-advice APIs from
`vehicle-navigation.md`, and may display these warnings as route risks.

## Publish Conditions

The cloud may publish a warning when:

- a road event is active and has a known boundary;
- the risk level is `medium`, `high`, or `critical`;
- the event has fresh telemetry or manual operator confirmation;
- the event is not closed or revoked.

## Payload Shape

```json
{
  "event_id": "evt-000001",
  "event_type": "construction",
  "road_name": "G42 eastbound 31K+200",
  "level": "high",
  "boundary": [
    { "longitude": 116.397, "latitude": 39.908 }
  ],
  "effective_at": "2026-06-04T10:30:00+08:00",
  "expires_at": "2026-06-04T11:30:00+08:00",
  "suggested_action": "slow_down_and_prepare_to_merge",
  "confidence": "operator_confirmed"
}
```

## Revocation Conditions

The cloud publishes a revocation when:

- the event is closed;
- risk is cleared by telemetry and operator confirmation;
- the road is restored;
- an operator manually cancels the warning.

Revocation payloads must carry the same `event_id` and a `status` of
`revoked`.
