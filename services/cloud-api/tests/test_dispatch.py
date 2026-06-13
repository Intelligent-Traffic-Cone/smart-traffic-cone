from __future__ import annotations

import unittest

from app.models import (
    ConeTelemetryIn,
    LocationPayload,
    NavigationTaskIn,
    UltrasonicChannelPayload,
    VehiclePositionIn,
)
from app.store import InMemoryStore


class DispatchFlowTests(unittest.TestCase):
    def setUp(self) -> None:
        self.store = InMemoryStore()

    def test_route_a_risk_recommends_route_b(self) -> None:
        assessment = self.store.assess_routes()
        self.assertEqual("route-b", assessment.recommended_route_id)
        self.assertEqual(2, len(assessment.routes))
        self.assertEqual(
            min(route.risk_score for route in assessment.routes),
            next(route.risk_score for route in assessment.routes if route.recommended),
        )

    def test_new_cone_telemetry_can_change_recommendation(self) -> None:
        route_b_points = [
            (116.39692, 39.91002),
            (116.39745, 39.91013),
            (116.39808, 39.91004),
            (116.39858, 39.90973),
        ]
        for index, (longitude, latitude) in enumerate(route_b_points):
            cone_id = f"route-b-risk-{index}"
            self.store.ingest_telemetry(
                cone_id,
                ConeTelemetryIn(
                    cone_id=cone_id,
                    location=LocationPayload(
                        longitude=longitude,
                        latitude=latitude,
                        has_fix=True,
                    ),
                    ultrasonic=[
                        UltrasonicChannelPayload(channel=0, distance_m=5)
                    ],
                ),
            )
        self.assertEqual("route-a", self.store.assess_routes().recommended_route_id)

    def test_task_polling_and_position_lifecycle(self) -> None:
        task = self.store.create_navigation_task(
            "pi-car-001",
            NavigationTaskIn(route_id="route-a"),
        )
        self.assertIsNotNone(task)
        first_poll = self.store.current_navigation_task("pi-car-001")
        second_poll = self.store.current_navigation_task("pi-car-001")
        self.assertEqual(first_poll.task_id, second_poll.task_id)
        self.assertEqual("accepted", first_poll.status)

        vehicle = self.store.update_vehicle_position(
            "pi-car-001",
            VehiclePositionIn(
                task_id=task.task_id,
                location=LocationPayload(
                    longitude=116.397,
                    latitude=39.9095,
                    accuracy_m=3,
                    has_fix=True,
                ),
                speed_kph=25,
                heading_deg=110,
                progress_percent=25,
                status="running",
            ),
        )
        self.assertEqual(1, len(vehicle.trace))
        self.assertTrue(self.store.list_vehicles()[0].online)

        self.store.update_vehicle_position(
            "pi-car-001",
            VehiclePositionIn(
                task_id=task.task_id,
                location=LocationPayload(
                    longitude=116.39908,
                    latitude=39.90866,
                    accuracy_m=3,
                    has_fix=True,
                ),
                progress_percent=100,
                status="completed",
            ),
        )
        self.assertEqual("completed", self.store.navigation_tasks[task.task_id].status)
        self.assertIsNone(self.store.current_navigation_task("pi-car-001"))

    def test_invalid_route_and_task_are_rejected(self) -> None:
        self.assertIsNone(
            self.store.create_navigation_task(
                "pi-car-001",
                NavigationTaskIn(route_id="missing"),
            )
        )
        self.assertIsNone(
            self.store.update_vehicle_position(
                "pi-car-001",
                VehiclePositionIn(
                    task_id="missing",
                    location=LocationPayload(has_fix=False),
                ),
            )
        )


if __name__ == "__main__":
    unittest.main()
