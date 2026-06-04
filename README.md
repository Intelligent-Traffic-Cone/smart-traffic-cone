# Smart Traffic Cone

Team workspace for the Intelligent Traffic Cone project.

The system uses an ESP32 edge node inside each smart cone to collect positioning,
four-channel ultrasonic distance, camera, and device-health data. The cloud
receives telemetry, creates road events and alerts, and publishes confirmed road
warnings for maps or vehicle-side systems.

## Repository Layout

```text
apps/
  dispatch-web/       Static dispatch-center demo.
  edge-cone-node/     PlatformIO firmware project for one cone node.
components/
  cone_device/        Reusable hardware module interfaces.
services/
  cloud-api/          FastAPI cloud API skeleton.
contracts/
  telemetry.schema.json
  vehicle-warning.md
docs/
  development/        Team development guides.
  product/            Product reports and design documents.
```

The repository root is a workspace only. It is not a PlatformIO firmware
project. Run firmware commands from `apps/edge-cone-node`.

## Quick Start

Firmware:

```powershell
cd apps/edge-cone-node
pio run -e esp32dev
```

Cloud API:

```powershell
cd services/cloud-api
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --reload
```

Dispatch web:

```powershell
cd apps/dispatch-web
start index.html
```

## Development Boundaries

- `apps/edge-cone-node` owns product orchestration and upload scheduling.
- `components/cone_device` owns reusable hardware interfaces and module state.
- `services/cloud-api` owns cloud HTTP API behavior and OpenAPI docs.
- `contracts` owns wire payloads shared across firmware, cloud, and clients.
- `docs/development` owns team process, style, and interface change rules.

Do not commit real map keys, Wi-Fi credentials, cloud tokens, or hardware lab
secrets. Use local config files ignored by git.
