# Verification

Run repository-level skeleton checks:

```powershell
python .\tools\check_project.py
```

Check dispatch-web JavaScript:

```powershell
node --check .\apps\dispatch-web\script.js
```

Check cloud API syntax:

```powershell
python -m py_compile .\services\cloud-api\app\main.py .\services\cloud-api\app\models.py .\services\cloud-api\app\store.py
```

Build firmware after PlatformIO is installed:

```powershell
cd apps\edge-cone-node
pio run -e esp32dev
```

Build the standalone GPS bench app after GPS changes:

```powershell
cd apps\gps-test-pio
pio run -e esp32-s3-devkitc-1
```

Upload firmware to a connected board:

```powershell
cd apps\edge-cone-node
pio run -e esp32dev -t upload
```

The current workspace root is not a PlatformIO project; run firmware commands
from `apps/edge-cone-node`.
