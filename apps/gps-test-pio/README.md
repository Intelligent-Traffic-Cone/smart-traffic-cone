# GPS Test PIO

Standalone PlatformIO Arduino bench app for validating GPS UART wiring and raw
NMEA parsing before integrating the module into `apps/edge-cone-node`.

## Build

```powershell
pio run -e esp32-s3-devkitc-1
```

## Defaults

- UART: `1`
- TX pin: `17`
- RX pin: `18`
- Baud: `115200`

Override these defaults in `platformio.ini` with `build_flags` when the target
board wiring differs.
