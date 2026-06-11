# Edge Cone Node Firmware

PlatformIO Arduino firmware project for one smart traffic cone edge node. The
app owns product orchestration: initialize hardware modules, collect snapshots,
encode telemetry, and upload to the cloud.

Project shape:

```text
platformio.ini       PlatformIO environments, board selection, and build flags.
src/                 Arduino setup/loop entry and orchestration.
../../components/    Reusable cone hardware modules loaded as PIO libraries.
```

Build from this directory:

```powershell
pio run
```

Local WiFi and cloud settings are read from `src/cone_node.local.h`. Create it
from the committed example:

```powershell
copy src\cone_node_config.example.h src\cone_node.local.h
```

Fill in:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `CONE_CLOUD_BASE_URL`, using the computer LAN address, for example
  `http://192.168.1.23:8000`
- `CONE_NODE_ID`
- `CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS`, default `4000`

Do not commit `cone_node.local.h`.

Upload to a connected board:

```powershell
pio run -t upload
```

Open the serial monitor:

```powershell
pio device monitor
```

The default environment targets `4d_gen4_esp32s3`.

Current V1 default wiring is centralized in `src/cone_node_bsp.h`:

- GPS: ESP TX `GPIO47` to GPS RX, ESP RX `GPIO48` from GPS TX.
- Ultrasonic front: TRIG `GPIO1`, ECHO `GPIO2`.
- Ultrasonic rear: TRIG `GPIO14`, ECHO `GPIO21`.
- Ultrasonic left: TRIG `GPIO38`, ECHO `GPIO39`.
- Ultrasonic right: TRIG `GPIO40`, ECHO `GPIO41`.
- Warning light: ESP TX `GPIO19` to module RX, ESP RX `GPIO20` from module TX,
  `115200 8N1`.
- Pan servos: bottom SG90 signal on `GPIO42`, top SG90 signal on `GPIO43`.
  Power the servos from an external 5V supply and connect grounds together.

After WiFi connects, open the ESP32 IP address in a browser to configure the
ultrasonic scene template, direction switches, pan controller, and warning light
debug panel. The settings are saved in NVS/Preferences and are included in telemetry
`raw_payload.ultrasonic_config`.

The warning light local API is:

```http
POST /api/warning-light
POST /api/warning-automation
```

With form fields:

- `target`: `all`, `yellow`, `green`, `red`, `buzzer`, `yellow_buzzer`,
  `green_buzzer`, or `red_buzzer`
- `action`: `off`, `on`, or `flash`
- `enabled`: `1` or `0` for `/api/warning-automation`

Automatic warning-light linkage is enabled by default. It uses only the
ultrasonic directions currently enabled in the local web page, picks the nearest
valid distance, and maps it to green/yellow/red/red+buzzer levels. Manual
`/api/warning-light` commands pause automatic linkage for 30 seconds.

The pan local API is:

```http
GET /api/pan
POST /api/pan/mode
POST /api/pan/heading
```

`/api/pan/mode` accepts `mode=roam|manual`. `/api/pan/heading` accepts one
`heading_deg=0..360` value while in manual mode; firmware maps that heading to
the bottom and top SG90 angles internally.

The repository root is a workspace, not a PlatformIO project. Run firmware
commands from this app directory.
