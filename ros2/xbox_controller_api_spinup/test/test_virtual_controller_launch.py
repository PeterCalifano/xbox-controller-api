"""Prove an unchanged standalone launch activates through the SDL test fixture.

The fixture is package-local and injected only into the launch child process.
This test exercises the production SDL source, lifecycle node, and launch file
without introducing a runtime simulation parameter.
"""

import os
import time
import unittest
from pathlib import Path

from ament_index_python.packages import (
    get_package_prefix,
    get_package_share_directory,
)
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
import launch_testing
from launch_testing.actions import ReadyToTest
from lifecycle_msgs.msg import State
from lifecycle_msgs.srv import GetState
import pytest
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy

# Bounded waits keep the virtual-controller smoke test independent of hardware.
SERVICE_TIMEOUT_SEC = 3.0
STATE_TIMEOUT_SEC = 4.0
JOY_TIMEOUT_SEC = 2.0
STATE_SAMPLE_PERIOD_SEC = 0.1
REQUIRED_STABLE_STATE_SAMPLES = 3

VIRTUAL_CONTROLLER_SENTINEL = "XBOX_CONTROLLER_API_TEST_VIRTUAL_CONTROLLER"
VIRTUAL_CONTROLLER_LIBRARY = "libxbox_controller_api_ros_virtual_sdl_controller.so"


def _virtual_controller_library_path() -> Path:
    """Return the installed package-local virtual-controller preload library.

    Raises:
        FileNotFoundError: If the BUILD_TESTING-only fixture was not installed.
    """
    objLibraryPath_ = (
        Path(get_package_prefix("xbox_controller_api_ros"))
        / "lib"
        / "xbox_controller_api_ros"
        / VIRTUAL_CONTROLLER_LIBRARY
    )
    if not objLibraryPath_.is_file():
        raise FileNotFoundError(
            f"Virtual SDL controller fixture was not installed: {objLibraryPath_}"
        )

    return objLibraryPath_


def _preload_value(objLibraryPath_: Path) -> str:
    """Prepend the fixture while retaining any caller-provided preload library."""
    charExistingPreload_ = os.environ.get("LD_PRELOAD", "")
    if not charExistingPreload_:
        return str(objLibraryPath_)

    return f"{objLibraryPath_}:{charExistingPreload_}"


@pytest.mark.launch_test
def generate_test_description() -> LaunchDescription:
    """Launch the unmodified standalone asset with the virtual SDL fixture."""
    objLibraryPath_ = _virtual_controller_library_path()
    charPackageShare_ = get_package_share_directory("xbox_controller_api_spinup")
    objLaunchSource_ = PythonLaunchDescriptionSource(
        f"{charPackageShare_}/launch/xbox_controller_api.launch.py"
    )

    return LaunchDescription(
        [
            SetEnvironmentVariable(
                name=VIRTUAL_CONTROLLER_SENTINEL,
                value="1",
            ),
            SetEnvironmentVariable(
                name="LD_PRELOAD",
                value=_preload_value(objLibraryPath_),
            ),
            IncludeLaunchDescription(objLaunchSource_),
            ReadyToTest(),
        ]
    )


class TestVirtualControllerLaunch(unittest.TestCase):
    """Check the SDL fixture reaches active state through the shipped launch."""

    objNode_: Node

    @classmethod
    def setUpClass(cls) -> None:
        """Create the ROS client node used to observe the launched lifecycle node."""
        rclpy.init()
        cls.objNode_ = rclpy.create_node("xbox_controller_api_virtual_launch_test")

    @classmethod
    def tearDownClass(cls) -> None:
        """Release the ROS client node after the launch test exits."""
        cls.objNode_.destroy_node()
        rclpy.shutdown()

    def _waitForActiveState(self) -> None:
        """Require the standalone lifecycle node to stabilize in the active state."""
        objStateClient_ = self.objNode_.create_client(
            GetState,
            "/xbox_controller/get_state",
        )
        try:
            self.assertTrue(
                objStateClient_.wait_for_service(timeout_sec=SERVICE_TIMEOUT_SEC),
                "Standalone GetState service was unavailable",
            )

            dDeadline_ = time.monotonic() + STATE_TIMEOUT_SEC
            uiStableSamples_ = 0
            uiLastState_ = State.PRIMARY_STATE_UNKNOWN
            while time.monotonic() < dDeadline_:
                objFuture_ = objStateClient_.call_async(GetState.Request())
                rclpy.spin_until_future_complete(
                    self.objNode_,
                    objFuture_,
                    timeout_sec=STATE_SAMPLE_PERIOD_SEC,
                )
                if objFuture_.done() and objFuture_.exception() is None:
                    objResponse_ = objFuture_.result()
                    self.assertIsNotNone(objResponse_)
                    uiLastState_ = objResponse_.current_state.id
                    if uiLastState_ == State.PRIMARY_STATE_ACTIVE:
                        uiStableSamples_ += 1
                        if uiStableSamples_ >= REQUIRED_STABLE_STATE_SAMPLES:
                            return
                    else:
                        uiStableSamples_ = 0
                else:
                    uiStableSamples_ = 0

                time.sleep(STATE_SAMPLE_PERIOD_SEC)

            self.fail(f"Standalone node did not stabilize in active state; got {uiLastState_}")
        finally:
            self.objNode_.destroy_client(objStateClient_)

    def _waitForJoyMessage(self, listJoyMessages_: list[Joy]) -> Joy:
        """Return one Joy message published after the lifecycle node activates."""
        dDeadline_ = time.monotonic() + JOY_TIMEOUT_SEC
        while not listJoyMessages_ and time.monotonic() < dDeadline_:
            rclpy.spin_once(self.objNode_, timeout_sec=STATE_SAMPLE_PERIOD_SEC)

        self.assertTrue(listJoyMessages_, "No Joy message arrived from the active standalone node")

        return listJoyMessages_[-1]

    def testStandaloneLaunchActivatesAndPublishesJoy(self) -> None:
        """Require the unchanged standalone launch to publish through SDL."""
        listJoyMessages_: list[Joy] = []
        objJoySubscription_ = self.objNode_.create_subscription(
            Joy,
            "/xbox_controller/joy",
            listJoyMessages_.append,
            10,
        )
        try:
            self._waitForActiveState()
            objJoy_ = self._waitForJoyMessage(listJoyMessages_)

            self.assertEqual(len(objJoy_.axes), 6)
            self.assertEqual(len(objJoy_.buttons), 15)
            self.assertEqual(objJoy_.header.frame_id, "xbox_controller")
            self.assertTrue(
                objJoy_.header.stamp.sec > 0 or objJoy_.header.stamp.nanosec > 0,
                "Virtual-controller Joy stamp was not populated",
            )
        finally:
            self.objNode_.destroy_subscription(objJoySubscription_)
