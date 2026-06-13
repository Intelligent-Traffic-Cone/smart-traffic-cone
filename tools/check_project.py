from __future__ import annotations

import json
import py_compile
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(path: str) -> Path:
    target = ROOT / path
    if not target.exists():
        raise SystemExit(f"missing required path: {path}")
    return target


def main() -> None:
    for path in [
        "README.md",
        "CONTEXT.md",
        "apps/edge-cone-node/platformio.ini",
        "apps/dispatch-web/index.html",
        "apps/pi-vehicle-simulator/index.html",
        "apps/pi-vehicle-simulator/server.py",
        "apps/vehicle-desktop-simulator/app.py",
        "components/cone_device/library.json",
        "services/cloud-api/app/main.py",
        "contracts/telemetry.schema.json",
        "contracts/vehicle-navigation.md",
        "contracts/vehicle-dispatch.md",
        "contracts/examples/telemetry.sample.json",
        "contracts/examples/navigation-session.sample.json",
        "contracts/examples/navigation-tick.sample.json",
    ]:
        require(path)

    web_index = require("apps/dispatch-web/index.html").read_text(encoding="utf-8")
    if "智能路锥" not in web_index:
        raise SystemExit("dispatch web index is not readable as UTF-8 Chinese")

    product_doc = require("docs/product/智能路锥调度中心网站功能设计文档.md").read_text(encoding="utf-8")
    if "智能路锥" not in product_doc:
        raise SystemExit("product document is not readable as UTF-8 Chinese")

    script = require("apps/dispatch-web/script.js").read_text(encoding="utf-8")
    unsafe_key_pattern = re.compile(r'const\s+AMAP_(?:KEY|SECURITY_CODE)\s*=\s*"[^"]{8,}"')
    if unsafe_key_pattern.search(script):
        raise SystemExit("dispatch web still contains an inline map key")

    json.loads(require("contracts/telemetry.schema.json").read_text(encoding="utf-8"))
    json.loads(require("contracts/examples/telemetry.sample.json").read_text(encoding="utf-8"))
    nav_session = json.loads(require("contracts/examples/navigation-session.sample.json").read_text(encoding="utf-8"))
    nav_tick = json.loads(require("contracts/examples/navigation-tick.sample.json").read_text(encoding="utf-8"))

    for path in [
        "services/cloud-api/app/main.py",
        "services/cloud-api/app/models.py",
        "services/cloud-api/app/store.py",
        "apps/pi-vehicle-simulator/server.py",
        "apps/vehicle-desktop-simulator/app.py",
    ]:
        py_compile.compile(str(require(path)), doraise=True)

    sys.path.insert(0, str(ROOT / "services" / "cloud-api"))
    from app.models import NavigationSessionIn, VehiclePositionTickIn

    NavigationSessionIn.model_validate(nav_session)
    VehiclePositionTickIn.model_validate(nav_tick)

    routes = json.loads(require("services/cloud-api/app/routes.json").read_text(encoding="utf-8"))
    if len(routes) != 2:
        raise SystemExit("routes.json must contain exactly two route candidates")

    print("project skeleton checks passed")


if __name__ == "__main__":
    main()
