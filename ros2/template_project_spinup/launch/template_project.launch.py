from launch import LaunchDescription
from launch_ros.actions import LifecycleNode
# from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

import os


def generate_launch_description() -> LaunchDescription:
    objPackageShare_ = get_package_share_directory("template_project_spinup")
    charParamsFile_ = os.path.join(objPackageShare_, "config", "template_project.yaml")

    return LaunchDescription([
        # LifecycleNode autostart asks launch_ros to configure and activate the node.
        LifecycleNode(
            package="template_project_ros",
            executable="template_project_node",
            name="template_algorithm",
            namespace="",
            output="screen",
            parameters=[charParamsFile_],
            autostart=True,
        )
        # Template alternative: Node starts unconfigured and requires an external lifecycle manager.
        # Node(
        #     package="template_project_ros",
        #     executable="template_project_node",
        #     name="template_algorithm",
        #     output="screen",
        #     parameters=[charParamsFile_],
        # )
    ])
