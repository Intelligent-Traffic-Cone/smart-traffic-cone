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

Upload to a connected board:

```powershell
pio run -t upload
```

Open the serial monitor:

```powershell
pio device monitor
```

The default environment targets the generic `esp32dev` board. Update the board
in `platformio.ini` when the exact controller is finalized.

The repository root is a workspace, not a PlatformIO project. Run firmware
commands from this app directory.
