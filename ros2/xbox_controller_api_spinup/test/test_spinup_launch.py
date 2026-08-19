"""Check that each launch path reaches its explicitly requested lifecycle state.

The normal CI contract is controller-independent: autostart configures the
node, activation fails without a controller, and the node remains inactive
without publishing a Joy message. ``XBOX_CONTROLLER_API_TEST_EXPECTED_STATE``
is a test-only switch for the virtual-controller fixture; it accepts
``inactive`` (the default) or ``active``.
"""

import os
import time
import unittest

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import GroupAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
import launch_testing
from launch_testing.actions import ReadyToTest
from launch_ros.actions import PushRosNamespace
from launch.events.process import ProcessStarted
from lifecycle_msgs.msg import State
from lifecycle_msgs.srv import ChangeState, GetState
import pytest
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy

# Axis and button counts defined by the Joy bridge.
EXPECTED_AXIS_COUNT = 6
EXPECTED_BUTTON_COUNT = 15

# Keep each parametrized launch case short while leaving time for discovery in
# a loaded ROS overlay.
SERVICE_TIMEOUT_SEC = 3.0
STATE_TIMEOUT_SEC = 4.0
JOY_TIMEOUT_SEC = 2.0
QUIET_PERIOD_SEC = 0.5
STATE_SAMPLE_PERIOD_SEC = 0.1
REQUIRED_STABLE_STATE_SAMPLES = 3

EXPECTED_STATES = {
    "inactive": State.PRIMARY_STATE_INACTIVE,
    "active": State.PRIMARY_STATE_ACTIVE,
}
EXPECTED_STATE_NAME = os.environ.get(
    "XBOX_CONTROLLER_API_TEST_EXPECTED_STATE", "inactive"
).strip().lower()
if EXPECTED_STATE_NAME not in EXPECTED_STATES:
    raise RuntimeError(
        "XBOX_CONTROLLER_API_TEST_EXPECTED_STATE must be 'inactive' or "
        f"'active', got {EXPECTED_STATE_NAME!r}"
    )

EXPECTED_STATE = EXPECTED_STATES[EXPECTED_STATE_NAME]


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
) -> LaunchDescription:
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

    # The standalone node and composition container are the processes whose
    # liveness proves a failed controller activation does not crash the launch.
    charProcessName_ = (
        "component_container"
        if charLaunchFile_ == "xbox_controller_api_composition.launch.py"
        else "xbox_controller_api_node"
    )

    return (
        LaunchDescription(
            [
                objLaunchAction_,
                ReadyToTest(),
            ]
        ),
        {"charProcessName_": charProcessName_},
    )


class TestSpinupLaunch(unittest.TestCase):
    objNode_: Node

    @classmethod
    def setUpClass(cls) -> None:
        rclpy.init()
        cls.objNode_ = rclpy.create_node("xbox_controller_api_spinup_launch_test")

    @classmethod
    def tearDownClass(cls) -> None:
        cls.objNode_.destroy_node()
        rclpy.shutdown()

    def _waitForExpectedTerminalState(
        self,
        charNodePath_: str,
        charCase_: str,
    ) -> None:
        """Require lifecycle services and a stable requested terminal state.

        Args:
            charNodePath_: Fully-qualified path of the lifecycle node.
            charCase_: Human-readable parametrized launch case.
        """
        objStateClient_ = self.objNode_.create_client(
            GetState,
            f"{charNodePath_}/get_state",
        )
        objChangeStateClient_ = self.objNode_.create_client(
            ChangeState,
            f"{charNodePath_}/change_state",
        )
        try:
            self.assertTrue(
                objStateClient_.wait_for_service(timeout_sec=SERVICE_TIMEOUT_SEC),
                f"GetState service was unavailable for {charCase_}",
            )
            self.assertTrue(
                objChangeStateClient_.wait_for_service(timeout_sec=SERVICE_TIMEOUT_SEC),
                f"ChangeState service was unavailable for {charCase_}",
            )

            dDeadline_ = time.monotonic() + STATE_TIMEOUT_SEC
            uiLastState_ = State.PRIMARY_STATE_UNKNOWN
            uiStableSamples_ = 0
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
                    if uiLastState_ == EXPECTED_STATE:
                        uiStableSamples_ += 1
                        if uiStableSamples_ >= REQUIRED_STABLE_STATE_SAMPLES:
                            return
                    else:
                        uiStableSamples_ = 0
                else:
                    uiStableSamples_ = 0

                time.sleep(STATE_SAMPLE_PERIOD_SEC)

            self.fail(
                f"Lifecycle node did not settle in {EXPECTED_STATE_NAME} for {charCase_}; "
                f"last state was {uiLastState_}"
            )
        finally:
            self.objNode_.destroy_client(objChangeStateClient_)
            self.objNode_.destroy_client(objStateClient_)

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
            f"Process exited before the lifecycle contract completed for {charCase_}",
        )

    def _waitForJoyMessage(self, listJoyMessages_: list[Joy], charCase_: str) -> Joy:
        """Return one published Joy message before the active-state deadline."""
        dDeadline_ = time.monotonic() + JOY_TIMEOUT_SEC
        while not listJoyMessages_ and time.monotonic() < dDeadline_:
            rclpy.spin_once(self.objNode_, timeout_sec=STATE_SAMPLE_PERIOD_SEC)

        self.assertTrue(listJoyMessages_, f"No Joy message arrived for {charCase_}")

        return listJoyMessages_[-1]

    def _assertNoJoyMessages(self, listJoyMessages_: list[Joy], charCase_: str) -> None:
        """Require that an inactive lifecycle publisher emits no Joy messages."""
        dDeadline_ = time.monotonic() + QUIET_PERIOD_SEC
        while time.monotonic() < dDeadline_:
            rclpy.spin_once(self.objNode_, timeout_sec=STATE_SAMPLE_PERIOD_SEC)

        self.assertFalse(
            listJoyMessages_,
            f"Inactive node unexpectedly published Joy for {charCase_}",
        )

    def _assertJoyContract(self, objJoy_: Joy, charCase_: str) -> None:
        """Check the controller-independent portion of the published Joy contract."""
        self.assertEqual(len(objJoy_.axes), EXPECTED_AXIS_COUNT, charCase_)
        self.assertEqual(len(objJoy_.buttons), EXPECTED_BUTTON_COUNT, charCase_)
        self.assertEqual(objJoy_.header.frame_id, "xbox_controller", charCase_)
        self.assertTrue(
            objJoy_.header.stamp.sec > 0 or objJoy_.header.stamp.nanosec > 0,
            f"Joy stamp was not populated for {charCase_}",
        )

        # Joy values must remain within the normalized controller ranges.
        for dAxis_ in objJoy_.axes[:4]:
            self.assertGreaterEqual(dAxis_, -1.0, charCase_)
            self.assertLessEqual(dAxis_, 1.0, charCase_)
        for dTrigger_ in objJoy_.axes[4:]:
            self.assertGreaterEqual(dTrigger_, 0.0, charCase_)
            self.assertLessEqual(dTrigger_, 1.0, charCase_)
        for iButton_ in objJoy_.buttons:
            self.assertIn(iButton_, (0, 1), charCase_)

    def testLaunchPathReachesExpectedState(
        self,
        charLaunchFile_: str,
        charNamespace_: str,
        charProcessName_: str,
        proc_info: launch_testing.ActiveProcInfoHandler,
    ) -> None:
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
            self._waitForExpectedTerminalState(charNodePath_, charCase_)
            self._assertProcessIsAlive(proc_info, charProcessName_, charCase_)

            if EXPECTED_STATE == State.PRIMARY_STATE_INACTIVE:
                self._assertNoJoyMessages(listJoyMessages_, charCase_)
            else:
                objJoy_ = self._waitForJoyMessage(listJoyMessages_, charCase_)
                self._assertJoyContract(objJoy_, charCase_)

            self._assertProcessIsAlive(proc_info, charProcessName_, charCase_)
        finally:
            self.objNode_.destroy_subscription(objJoySubscription_)
