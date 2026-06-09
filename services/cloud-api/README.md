# Cloud API

FastAPI skeleton for receiving smart cone telemetry, maintaining device state,
creating road events, handling alerts, publishing road warnings, and serving
Raspberry Pi vehicle-side navigation advice.

Run locally:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --reload
```

OpenAPI docs are available at `http://127.0.0.1:8000/docs`.

Useful demo endpoints:

```http
POST /api/demo/reset
GET /api/map/layers
POST /api/vehicles/{vehicle_id}/navigation-sessions
POST /api/vehicles/{vehicle_id}/navigation-sessions/{session_id}/tick
```
