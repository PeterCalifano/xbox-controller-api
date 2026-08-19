"""Prove every shipped launch form activates through the SDL test fixture.

The fixture is package-local and injected only into the launch child process.
This test exercises the production SDL source, lifecycle node, and launch files
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
from launch.actions import GroupAction, IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
import launch_testing
from launch_testing.actions import ReadyToTest
from launch.events.process import ProcessStarted
from launch_ros.actions import PushRosNamespace
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

# The fixture is seeded with fully deflected sticks and released triggers.
EXPECTED_STICK_AXES = (1.0, 1.0, -1.0, -1.0)
EXPECTED_TRIGGER_AXES = (0.0, 0.0)

# This follows xbox_controller_api::AllGamepadButtons(): A and D-pad-left press.
EXPECTED_BUTTONS = (1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0)


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
@launch_testing.parametrize(
    "charLaunchFile_, charNamespace_",
    [
        ("xbox_controller_api.launch.py", ""),
        ("xbox_controller_api_composition.launch.py", ""),
        ("xbox_controller_api.launch.py", "integration"),
        ("xbox_controller_api_composition.launch.py", "integration"),
    ],
)
def generate_test_description(
    charLaunchFile_: str,
    charNamespace_: str,
) -> tuple[LaunchDescription, dict[str, str]]:
    """Launch one unmodified asset with the virtual SDL fixture enabled."""
    objLibraryPath_ = _virtual_controller_library_path()
    charPackageShare_ = get_package_share_directory("xbox_controller_api_spinup")
    objLaunchSource_ = PythonLaunchDescriptionSource(
        f"{charPackageShare_}/launch/{charLaunchFile_}"
    )
    objIncludeLaunch_ = IncludeLaunchDescription(objLaunchSource_)
    if charNamespace_:
        objLaunchAction_ = GroupAction(
            [PushRosNamespace(charNamespace_), objIncludeLaunch_]
        )
    else:
        objLaunchAction_ = objIncludeLaunch_

    # Match each shipped launch form so process liveness remains observable.
    charProcessName_ = (
        "component_container"
        if charLaunchFile_ == "xbox_controller_api_composition.launch.py"
        else "xbox_controller_api_node"
    )

    return (
        LaunchDescription(
            [
                SetEnvironmentVariable(
                    name=VIRTUAL_CONTROLLER_SENTINEL,
                    value="1",
                ),
                SetEnvironmentVariable(
                    name="LD_PRELOAD",
                    value=_preload_value(objLibraryPath_),
                ),
                objLaunchAction_,
                ReadyToTest(),
            ]
        ),
        {"charProcessName_": charProcessName_},
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

    def _waitForActiveState(self, charNodePath_: str, charCase_: str) -> None:
        """Require the lifecycle node to stabilize in the active state.

        Args:
            charNodePath_: Fully-qualified path of the lifecycle node.
            charCase_: Human-readable parametrized launch case.
        """
        objStateClient_ = self.objNode_.create_client(
            GetState,
            f"{charNodePath_}/get_state",
        )
        try:
            self.assertTrue(
                objStateClient_.wait_for_service(timeout_sec=SERVICE_TIMEOUT_SEC),
                f"GetState service was unavailable for {charCase_}",
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

            self.fail(
                f"Lifecycle node did not stabilize in active state for {charCase_}; "
                f"got {uiLastState_}"
            )
        finally:
            self.objNode_.destroy_client(objStateClient_)

    def _waitForJoyMessage(self, listJoyMessages_: list[Joy], charCase_: str) -> Joy:
        """Return one Joy message published after the lifecycle node activates."""
        dDeadline_ = time.monotonic() + JOY_TIMEOUT_SEC
        while not listJoyMessages_ and time.monotonic() < dDeadline_:
            rclpy.spin_once(self.objNode_, timeout_sec=STATE_SAMPLE_PERIOD_SEC)

        self.assertTrue(listJoyMessages_, f"No Joy message arrived for {charCase_}")

        return listJoyMessages_[-1]

    def _assertJoyContract(self, objJoy_: Joy, charCase_: str) -> None:
        """Verify the full deterministic virtual-controller Joy contract."""
        self.assertEqual(len(objJoy_.axes), 6, charCase_)
        self.assertEqual(len(objJoy_.buttons), 15, charCase_)
        self.assertEqual(tuple(objJoy_.axes[:4]), EXPECTED_STICK_AXES, charCase_)
        self.assertEqual(tuple(objJoy_.axes[4:]), EXPECTED_TRIGGER_AXES, charCase_)
        self.assertEqual(tuple(objJoy_.buttons), EXPECTED_BUTTONS, charCase_)
        self.assertEqual(objJoy_.header.frame_id, "xbox_controller", charCase_)
        self.assertTrue(
            objJoy_.header.stamp.sec > 0 or objJoy_.header.stamp.nanosec > 0,
            f"Joy stamp was not populated for {charCase_}",
        )

    def _assertProcessIsAlive(
        self,
        proc_info: launch_testing.ActiveProcInfoHandler,
        charProcessName_: str,
        charCase_: str,
    ) -> None:
        """Require the standalone node or composition container to remain alive."""
        self.assertIsInstance(
            proc_info[charProcessName_],
            ProcessStarted,
            f"Process exited before active-state verification for {charCase_}",
        )

    def testLaunchPathActivatesAndPublishesJoy(
        self,
        charLaunchFile_: str,
        charNamespace_: str,
        charProcessName_: str,
        proc_info: launch_testing.ActiveProcInfoHandler,
    ) -> None:
        """Require each unchanged launch path to publish the seeded SDL state."""
        charNamespacePrefix_ = f"/{charNamespace_}" if charNamespace_ else ""
        charNodePath_ = f"{charNamespacePrefix_}/xbox_controller"
        charCase_ = f"launch={charLaunchFile_}, namespace={charNamespace_ or '<root>'}"

        listJoyMessages_: list[Joy] = []
        objJoySubscription_ = self.objNode_.create_subscription(
            Joy,
            f"{charNodePath_}/joy",
            listJoyMessages_.append,
            10,
        )
        try:
            proc_info.assertWaitForStartup(
                process=charProcessName_,
                timeout=SERVICE_TIMEOUT_SEC,
            )
            self._waitForActiveState(charNodePath_, charCase_)
            self._assertProcessIsAlive(proc_info, charProcessName_, charCase_)
            objJoy_ = self._waitForJoyMessage(listJoyMessages_, charCase_)
            self._assertJoyContract(objJoy_, charCase_)
            self._assertProcessIsAlive(proc_info, charProcessName_, charCase_)
        finally:
            self.objNode_.destroy_subscription(objJoySubscription_)
