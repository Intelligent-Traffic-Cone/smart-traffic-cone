from __future__ import annotations

from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parent


class VehicleSimulatorHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> None:
    host = "127.0.0.1"
    port = 8090
    server = ThreadingHTTPServer((host, port), VehicleSimulatorHandler)
    print(f"Raspberry Pi vehicle simulator: http://{host}:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
