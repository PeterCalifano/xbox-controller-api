"""Launch-level checks that the overlay starts and publishes controller data.

Nothing here asserts that a controller is attached. Activation legitimately
fails on a machine with no device, so each launch path is required to settle in
a defined lifecycle state, and the Joy contract is checked only when the node
actually reached the active state.
"""

import time
import unittest

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import GroupAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
import launch_testing
from launch_testing.actions import ReadyToTest
from launch_ros.actions import PushRosNamespace
from lifecycle_msgs.msg import State
from lifecycle_msgs.srv import GetState
import pytest
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy

# Axis and button counts published by the node, mirroring the C++ contract.
EXPECTED_AXIS_COUNT = 6
EXPECTED_BUTTON_COUNT = 15


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

    return LaunchDescription(
        [
            objLaunchAction_,
            ReadyToTest(),
        ]
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

    def _waitForSettledState(
        self,
        charNodePath_: str,
        charCase_: str,
        dTimeoutSec_: float = 10.0,
    ) -> int:
        """Return the lifecycle state the node settles in.

        Autostart drives configure and then activate. Activation fails when no
        controller is present, which leaves the node inactive rather than
        active, so both outcomes are accepted and returned to the caller.
        """
        objStateClient_ = self.objNode_.create_client(
            GetState,
            f"{charNodePath_}/get_state",
        )
        self.assertTrue(
            objStateClient_.wait_for_service(timeout_sec=dTimeoutSec_),
            f"Lifecycle state service was unavailable for {charCase_}",
        )

        dDeadline_ = time.monotonic() + dTimeoutSec_
        uiLastState_ = State.PRIMARY_STATE_UNKNOWN
        while time.monotonic() < dDeadline_:
            objFuture_ = objStateClient_.call_async(GetState.Request())
            rclpy.spin_until_future_complete(self.objNode_, objFuture_, timeout_sec=1.0)
            if objFuture_.done() and objFuture_.exception() is None:
                objResponse_ = objFuture_.result()
                self.assertIsNotNone(objResponse_)
                uiLastState_ = objResponse_.current_state.id
                if uiLastState_ == State.PRIMARY_STATE_ACTIVE:
                    return uiLastState_
            time.sleep(0.1)

        self.assertEqual(
            uiLastState_,
            State.PRIMARY_STATE_INACTIVE,
            f"Node settled in an unexpected state for {charCase_}; got {uiLastState_}",
        )

        return uiLastState_

    def testLaunchPathSettlesAndPublishesJoyWhenActive(
        self,
        charLaunchFile_: str,
        charNamespace_: str,
    ) -> None:
        charNamespacePrefix_ = f"/{charNamespace_}" if charNamespace_ else ""
        charNodePath_ = f"{charNamespacePrefix_}/xbox_controller"
        charCase_ = f"launch={charLaunchFile_}, namespace={charNamespace_ or '<root>'}"

        uiState_ = self._waitForSettledState(charNodePath_, charCase_)

        if uiState_ != State.PRIMARY_STATE_ACTIVE:
            # No controller on this machine: the launch path is still proven to
            # come up and configure, which is what this test can guarantee.
            self.skipTest(f"Node is inactive, so no controller is attached for {charCase_}")

        listJoyMessages_: list[Joy] = []
        objJoySubscription_ = self.objNode_.create_subscription(
            Joy,
            f"{charNodePath_}/joy",
            listJoyMessages_.append,
            10,
        )
        try:
            dDeadline_ = time.monotonic() + 5.0
            while not listJoyMessages_ and time.monotonic() < dDeadline_:
                rclpy.spin_once(self.objNode_, timeout_sec=0.1)

            self.assertTrue(listJoyMessages_, f"No Joy message arrived for {charCase_}")

            objJoy_ = listJoyMessages_[-1]
            self.assertEqual(len(objJoy_.axes), EXPECTED_AXIS_COUNT, charCase_)
            self.assertEqual(len(objJoy_.buttons), EXPECTED_BUTTON_COUNT, charCase_)
            self.assertEqual(objJoy_.header.frame_id, "xbox_controller", charCase_)
            self.assertTrue(
                objJoy_.header.stamp.sec > 0 or objJoy_.header.stamp.nanosec > 0,
                f"Joy stamp was not populated for {charCase_}",
            )

            # Values must satisfy the documented normalized ranges whatever the
            # sticks happen to be doing while the test runs.
            for dAxis_ in objJoy_.axes[:4]:
                self.assertGreaterEqual(dAxis_, -1.0, charCase_)
                self.assertLessEqual(dAxis_, 1.0, charCase_)
            for dTrigger_ in objJoy_.axes[4:]:
                self.assertGreaterEqual(dTrigger_, 0.0, charCase_)
                self.assertLessEqual(dTrigger_, 1.0, charCase_)
            for iButton_ in objJoy_.buttons:
                self.assertIn(iButton_, (0, 1), charCase_)
        finally:
            self.objNode_.destroy_subscription(objJoySubscription_)
