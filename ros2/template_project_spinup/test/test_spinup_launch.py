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
from template_project_interfaces.msg import AlgorithmStatus
from template_project_interfaces.srv import RunAlgorithm


@pytest.mark.launch_test
@launch_testing.parametrize(
    "charLaunchFile_, charNamespace_",
    [
        ("template_project.launch.py", ""),
        ("template_project_composition.launch.py", ""),
        ("template_project.launch.py", "integration"),
        ("template_project_composition.launch.py", "integration"),
    ],
)
def generate_test_description(
    charLaunchFile_: str,
    charNamespace_: str,
) -> LaunchDescription:
    charPackageShare_ = get_package_share_directory("template_project_spinup")
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
        cls.objNode_ = rclpy.create_node("template_project_spinup_launch_test")

    @classmethod
    def tearDownClass(cls) -> None:
        cls.objNode_.destroy_node()
        rclpy.shutdown()

    def _waitForActive(
        self,
        charNodePath_: str,
        charCase_: str,
        dTimeoutSec_: float = 10.0,
    ) -> None:
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
                    return
            time.sleep(0.1)

        self.fail(
            f"Lifecycle node did not become active for {charCase_}; "
            f"last state was {uiLastState_}"
        )

    def testLaunchPathIsActiveAndServesAlgorithm(
        self,
        charLaunchFile_: str,
        charNamespace_: str,
    ) -> None:
        charNamespacePrefix_ = f"/{charNamespace_}" if charNamespace_ else ""
        charNodePath_ = f"{charNamespacePrefix_}/template_algorithm"
        charCase_ = f"launch={charLaunchFile_}, namespace={charNamespace_ or '<root>'}"
        self._waitForActive(charNodePath_, charCase_)

        objAlgorithmClient_ = self.objNode_.create_client(
            RunAlgorithm,
            f"{charNodePath_}/run_algorithm",
        )
        self.assertTrue(
            objAlgorithmClient_.wait_for_service(timeout_sec=5.0),
            f"Algorithm service was unavailable for {charCase_}",
        )

        listStatusMessages_: list[AlgorithmStatus] = []
        objStatusSubscription_ = self.objNode_.create_subscription(
            AlgorithmStatus,
            f"{charNodePath_}/status",
            listStatusMessages_.append,
            10,
        )
        try:
            dDiscoveryDeadline_ = time.monotonic() + 5.0
            while (
                objStatusSubscription_.get_publisher_count() == 0
                and time.monotonic() < dDiscoveryDeadline_
            ):
                rclpy.spin_once(self.objNode_, timeout_sec=0.1)
            self.assertGreater(
                objStatusSubscription_.get_publisher_count(),
                0,
                f"Status publisher was undiscovered for {charCase_}",
            )

            # Subscriber-side graph visibility can precede publisher-side endpoint matching.
            dDiscoverySettleDeadline_ = time.monotonic() + 0.5
            while time.monotonic() < dDiscoverySettleDeadline_:
                rclpy.spin_once(self.objNode_, timeout_sec=0.05)

            objRequest_ = RunAlgorithm.Request()
            objRequest_.input = 3.0
            objFuture_ = objAlgorithmClient_.call_async(objRequest_)
            dResponseDeadline_ = time.monotonic() + 5.0
            while (
                (not objFuture_.done() or not listStatusMessages_)
                and time.monotonic() < dResponseDeadline_
            ):
                rclpy.spin_once(self.objNode_, timeout_sec=0.1)

            self.assertTrue(
                objFuture_.done(),
                f"Algorithm response timed out for {charCase_}",
            )
            self.assertTrue(
                listStatusMessages_,
                f"Status publication timed out for {charCase_}",
            )
            self.assertIsNone(objFuture_.exception(), charCase_)
            objResponse_ = objFuture_.result()
            self.assertIsNotNone(objResponse_, charCase_)
            self.assertEqual(objResponse_.output, 14.0, charCase_)
            self.assertEqual(objResponse_.status, "ok", charCase_)

            objStatus_ = listStatusMessages_[-1]
            self.assertEqual(objStatus_.last_input, 3.0, charCase_)
            self.assertEqual(objStatus_.last_output, 14.0, charCase_)
            self.assertEqual(objStatus_.evaluation_count, 1, charCase_)
            self.assertEqual(objStatus_.state, "ok", charCase_)
            self.assertTrue(
                objStatus_.stamp.sec > 0 or objStatus_.stamp.nanosec > 0,
                f"Status timestamp was not populated for {charCase_}",
            )
        finally:
            self.objNode_.destroy_subscription(objStatusSubscription_)
