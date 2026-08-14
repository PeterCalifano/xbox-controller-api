from launch import LaunchDescription
from launch import LaunchContext
from launch.actions import OpaqueFunction
from launch.utilities import perform_substitutions
from launch_ros.actions import ComposableNodeContainer, LifecycleTransition
from launch_ros.descriptions import ComposableLifecycleNode as _RosComposableLifecycleNode
# from launch_ros.descriptions import ComposableNode
from launch_ros.utilities import (
    LifecycleEventManager,
    make_namespace_absolute,
    prefix_namespace,
)
from ament_index_python.packages import get_package_share_directory
from lifecycle_msgs.msg import Transition

import os


class _LifecycleNodeIdentity:
    def __init__(self, charFullyQualifiedName_: str) -> None:
        self.charFullyQualifiedName_ = charFullyQualifiedName_

    @property
    def node_name(self) -> str:
        return self.charFullyQualifiedName_


class ComposableLifecycleNode(_RosComposableLifecycleNode):
    def __init__(self, *, autostart: bool = False, **kwargs: object) -> None:
        self.bAutostart_ = autostart
        self.charFullyQualifiedName_ = ""

        # Jazzy joins composed autostart namespaces without a separator. The local
        # action below preserves autostart=True while replacing only that transition.
        super().__init__(autostart=False, **kwargs)

    def init_lifecycle_event_manager(self, objContext_: LaunchContext) -> None:
        charNodeName_ = perform_substitutions(objContext_, self.node_name)
        charNodeNamespace_ = ""
        if self.node_namespace is not None:
            charNodeNamespace_ = perform_substitutions(objContext_, self.node_namespace)

        charBaseNamespace_ = objContext_.launch_configurations.get("ros_namespace", None)
        charCombinedNamespace_ = make_namespace_absolute(
            prefix_namespace(charBaseNamespace_, charNodeNamespace_)
        )
        self.charFullyQualifiedName_ = (
            prefix_namespace(charCombinedNamespace_, charNodeName_) or charNodeName_
        )
        if not self.charFullyQualifiedName_.startswith("/"):
            self.charFullyQualifiedName_ = f"/{self.charFullyQualifiedName_}"

        # Jazzy launch_ros#481: composed autostart otherwise matches a relative node identity.
        self.objLifecycleEventManager_ = LifecycleEventManager(
            _LifecycleNodeIdentity(self.charFullyQualifiedName_)
        )
        self.objLifecycleEventManager_.setup_lifecycle_manager(objContext_)

    def makeAutostartAction(self) -> OpaqueFunction:
        return OpaqueFunction(function=self._autostart)

    def _autostart(self, objContext_: LaunchContext) -> list[LifecycleTransition]:
        if not self.bAutostart_:
            return []

        self.init_lifecycle_event_manager(objContext_)
        return [
            LifecycleTransition(
                lifecycle_node_names=[self.charFullyQualifiedName_],
                transition_ids=[
                    Transition.TRANSITION_CONFIGURE,
                    Transition.TRANSITION_ACTIVATE,
                ],
            )
        ]


def generate_launch_description() -> LaunchDescription:
    objPackageShare_ = get_package_share_directory("template_project_spinup")
    charParamsFile_ = os.path.join(objPackageShare_, "config", "template_project.yaml")
    objLifecycleNode_ = ComposableLifecycleNode(
        package="template_project_ros",
        plugin="template_project_ros::CTemplateLifecycleNode",
        name="template_algorithm",
        parameters=[charParamsFile_],
        autostart=True,
    )

    return LaunchDescription([
        ComposableNodeContainer(
            name="template_project_container",
            namespace="",
            package="rclcpp_components",
            executable="component_container",
            composable_node_descriptions=[
                # Autostart configures and activates the component after it is loaded.
                objLifecycleNode_
                # Template alternative: ComposableNode requires an external lifecycle manager.
                # ComposableNode(
                #     package="template_project_ros",
                #     plugin="template_project_ros::CTemplateLifecycleNode",
                #     name="template_algorithm",
                #     parameters=[charParamsFile_],
                # )
            ],
            output="screen",
        ),
        objLifecycleNode_.makeAutostartAction(),
    ])
