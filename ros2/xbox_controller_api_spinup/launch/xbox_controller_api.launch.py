from launch import LaunchDescription
from launch_ros.actions import LifecycleNode
# from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

import os


def generate_launch_description() -> LaunchDescription:
    objPackageShare_ = get_package_share_directory("xbox_controller_api_spinup")
    charParamsFile_ = os.path.join(objPackageShare_, "config", "xbox_controller_api.yaml")

    return LaunchDescription([
        # LifecycleNode autostart asks launch_ros to configure and activate the node.
        LifecycleNode(
            package="xbox_controller_api_ros",
            executable="xbox_controller_api_node",
            name="xbox_controller",
            namespace="",
            output="screen",
            parameters=[charParamsFile_],
            autostart=True,
        )
        # Template alternative: Node starts unconfigured and requires an external lifecycle manager.
        # Node(
        #     package="xbox_controller_api_ros",
        #     executable="xbox_controller_api_node",
        #     name="xbox_controller",
        #     output="screen",
        #     parameters=[charParamsFile_],
        # )
    ])
