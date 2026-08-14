#!/usr/bin/env python3
"""Synchronize ROS 2 package manifests from the root CMake project metadata.

Example:
    python3 ros2/tools/sync_package_metadata.py \
        --project-root . --ros2-dir ros2 --version 1.2.3
    # Output:
    # Synchronized 4 ROS 2 package manifests.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Sequence


_STRICT_VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


@dataclass(frozen=True)
class ProjectMetadata:
    """Project-owned values exported by the metadata-only CMake configure.

    Example:
        metadata_ = ProjectMetadata("demo", "1.2.3", "Demo", "https://example.test", "A", "a@example.test", "MIT")
        print(metadata_.version)
        # Output:
        # 1.2.3
    """

    project_name: str
    version: str
    description: str
    homepage_url: str
    maintainer_name: str
    maintainer_email: str
    license: str


class PackageRole(Enum):
    """Role-specific description suffixes for overlay package manifests.

    Example:
        print(PackageRole.BRIDGE.value)
        # Output:
        # ROS 2 bridge package.
    """

    SHIM = "ROS 2 colcon shim package."
    INTERFACES = "ROS 2 message and service interfaces."
    BRIDGE = "ROS 2 bridge package."
    SPINUP = "ROS 2 launch and runtime assets."
    GENERIC = "ROS 2 package."


@dataclass(frozen=True)
class ManifestDocument:
    """Parsed package manifest plus filesystem and outer-XML state.

    Example:
        print(ManifestDocument.__name__)
        # Output:
        # ManifestDocument
    """

    path: Path
    mode: int
    original_bytes: bytes
    tree: ET.ElementTree
    leading_nodes: tuple[str, ...]
    trailing_nodes: tuple[str, ...]
    package_name: str


@dataclass(frozen=True)
class ManifestUpdate:
    """Prepared atomic replacement for one package manifest.

    Example:
        update_ = ManifestUpdate(Path("package.xml"), 0o644, b"old", b"new")
        print(oct(update_.mode))
        # Output:
        # 0o644
    """

    path: Path
    mode: int
    original_bytes: bytes
    updated_bytes: bytes


def _ReadCacheValue(cacheText_: str, key_: str) -> str:
    """Read one non-empty field from CMakeCache.txt text.

    Example:
        print(_ReadCacheValue("FIELD:STRING=value\n", "FIELD"))
        # Output:
        # value
    """
    prefix_ = f"{key_}:"
    for line_ in cacheText_.splitlines():
        if not line_.startswith(prefix_):
            continue
        _, separator_, value_ = line_.partition("=")
        if separator_ and value_:
            return value_
        break
    raise ValueError(f"Missing or empty CMake metadata field: {key_}")


def _ConfigureProjectMetadata(projectRoot_: Path, version_: str) -> ProjectMetadata:
    """Run the root metadata-only CMake configure and read its cache.

    Example:
        # _ConfigureProjectMetadata(Path("."), "1.2.3")
        # Output: ProjectMetadata populated from CMakeCache.txt
    """
    cmakeExecutable_ = shutil.which("cmake")
    if cmakeExecutable_ is None:
        raise RuntimeError("cmake was not found on PATH")

    with tempfile.TemporaryDirectory(prefix="ros2_project_metadata_") as buildDirectory_:
        result_ = subprocess.run(
            [
                cmakeExecutable_,
                "-S",
                str(projectRoot_),
                "-B",
                buildDirectory_,
                "-DPROJECT_METADATA_ONLY=ON",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if result_.returncode != 0:
            raise RuntimeError(
                "Metadata-only CMake configure failed:\n"
                f"stdout:\n{result_.stdout}\n"
                f"stderr:\n{result_.stderr}"
            )
        cacheText_ = (Path(buildDirectory_) / "CMakeCache.txt").read_text(encoding="utf-8")

    cacheVersion_ = _ReadCacheValue(cacheText_, "CMAKE_PROJECT_VERSION")
    if cacheVersion_ != version_:
        raise ValueError(
            f"Resolved CMake version {cacheVersion_!r} does not match requested ROS version {version_!r}"
        )

    metadata_ = ProjectMetadata(
        project_name=_ReadCacheValue(cacheText_, "CMAKE_PROJECT_NAME"),
        version=version_,
        description=_ReadCacheValue(cacheText_, "CMAKE_PROJECT_DESCRIPTION"),
        homepage_url=_ReadCacheValue(cacheText_, "CMAKE_PROJECT_HOMEPAGE_URL"),
        maintainer_name=_ReadCacheValue(cacheText_, "PROJECT_MAINTAINER_NAME"),
        maintainer_email=_ReadCacheValue(cacheText_, "PROJECT_MAINTAINER_EMAIL"),
        license=_ReadCacheValue(cacheText_, "PROJECT_LICENSE"),
    )
    if not _STRICT_VERSION_RE.fullmatch(metadata_.version):
        raise ValueError(f"ROS package version must be strict X.Y.Z, got {metadata_.version!r}")
    if "@" not in metadata_.maintainer_email:
        raise ValueError("PROJECT_MAINTAINER_EMAIL must contain '@'")
    return metadata_


def _ReadOuterXmlNodes(path_: Path) -> tuple[tuple[str, ...], tuple[str, ...]]:
    """Capture comments and processing instructions outside the package root.

    Example:
        # leading_, trailing_ = _ReadOuterXmlNodes(Path("package.xml"))
        # Output: (("<?xml-model ...?>",), ())
    """
    leadingNodes_: list[str] = []
    trailingNodes_: list[str] = []
    depth_ = 0
    rootSeen_ = False
    rootClosed_ = False

    for event_, element_ in ET.iterparse(path_, events=("start", "end", "comment", "pi")):
        if event_ == "start":
            rootSeen_ = True
            depth_ += 1
        elif event_ == "end":
            depth_ -= 1
            if rootSeen_ and depth_ == 0:
                rootClosed_ = True
        elif depth_ == 0:
            serializedNode_ = ET.tostring(element_, encoding="unicode")
            if rootClosed_:
                trailingNodes_.append(serializedNode_)
            elif not rootSeen_:
                leadingNodes_.append(serializedNode_)

    return tuple(leadingNodes_), tuple(trailingNodes_)


def _ReadManifest(path_: Path) -> ManifestDocument:
    """Parse one package manifest while retaining comments, PIs, and file mode.

    Example:
        # document_ = _ReadManifest(Path("ros2/demo/package.xml"))
        # Output: ManifestDocument(..., package_name="demo")
    """
    leadingNodes_, trailingNodes_ = _ReadOuterXmlNodes(path_)
    parser_ = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True, insert_pis=True))
    tree_ = ET.parse(path_, parser=parser_)
    root_ = tree_.getroot()
    if root_.tag != "package":
        raise ValueError(f"Expected <package> root in {path_}")
    nameElement_ = root_.find("name")
    packageName_ = "" if nameElement_ is None or nameElement_.text is None else nameElement_.text.strip()
    if not packageName_:
        raise ValueError(f"Missing package name in {path_}")
    return ManifestDocument(
        path=path_,
        mode=stat.S_IMODE(path_.stat().st_mode),
        original_bytes=path_.read_bytes(),
        tree=tree_,
        leading_nodes=leadingNodes_,
        trailing_nodes=trailingNodes_,
        package_name=packageName_,
    )


def _PackageRoles(packageNames_: frozenset[str]) -> dict[str, PackageRole]:
    """Infer overlay roles without consulting or changing the CMake project name.

    Example:
        roles_ = _PackageRoles(frozenset({"demo", "demo_interfaces", "demo_ros", "demo_spinup"}))
        print(roles_["demo_ros"].name)
        # Output:
        # BRIDGE
    """
    prefixCandidates_ = [
        name_
        for name_ in packageNames_
        if {
            f"{name_}_interfaces",
            f"{name_}_ros",
            f"{name_}_spinup",
        }.issubset(packageNames_)
    ]
    if len(prefixCandidates_) != 1:
        raise ValueError(
            "Could not identify one ROS overlay package quartet from package names: "
            + ", ".join(sorted(packageNames_))
        )

    prefix_ = prefixCandidates_[0]
    roleNames_ = {
        prefix_: PackageRole.SHIM,
        f"{prefix_}_interfaces": PackageRole.INTERFACES,
        f"{prefix_}_ros": PackageRole.BRIDGE,
        f"{prefix_}_spinup": PackageRole.SPINUP,
    }
    return {name_: roleNames_.get(name_, PackageRole.GENERIC) for name_ in packageNames_}


def _RequireElement(root_: ET.Element, tag_: str, path_: Path) -> ET.Element:
    """Return a required direct child element.

    Example:
        element_ = _RequireElement(ET.fromstring("<package><name>demo</name></package>"), "name", Path("package.xml"))
        print(element_.text)
        # Output:
        # demo
    """
    element_ = root_.find(tag_)
    if element_ is None:
        raise ValueError(f"Missing <{tag_}> in {path_}")
    return element_


def _SetWebsiteUrl(root_: ET.Element, homepageUrl_: str, path_: Path) -> None:
    """Update website URLs or insert one without touching other URL types.

    Example:
        root_ = ET.fromstring("<package><license>MIT</license></package>")
        _SetWebsiteUrl(root_, "https://example.test", Path("package.xml"))
        print(root_.find("url").text)
        # Output:
        # https://example.test
    """
    websiteElements_ = [url_ for url_ in root_.findall("url") if url_.get("type") == "website"]
    if websiteElements_:
        for websiteElement_ in websiteElements_:
            websiteElement_.text = homepageUrl_
        return

    licenseElement_ = _RequireElement(root_, "license", path_)
    children_ = list(root_)
    insertIndex_ = children_.index(licenseElement_) + 1
    websiteElement_ = ET.Element("url", {"type": "website"})
    websiteElement_.text = homepageUrl_
    websiteElement_.tail = licenseElement_.tail
    licenseElement_.tail = "\n  "
    root_.insert(insertIndex_, websiteElement_)


def _SerializeManifest(document_: ManifestDocument) -> bytes:
    """Serialize one manifest with its outer XML nodes restored.

    Example:
        # serialized_ = _SerializeManifest(document_)
        # Output: b'<?xml version="1.0"?>\n<?xml-model ...?>\n<package ...>\n'
    """
    rootText_ = ET.tostring(document_.tree.getroot(), encoding="unicode")
    sections_ = [
        '<?xml version="1.0"?>',
        *document_.leading_nodes,
        rootText_,
        *document_.trailing_nodes,
    ]
    return ("\n".join(sections_).rstrip() + "\n").encode("utf-8")


def _BuildUpdate(
    document_: ManifestDocument,
    metadata_: ProjectMetadata,
    role_: PackageRole,
) -> ManifestUpdate:
    """Prepare one manifest update without changing package identity or dependencies.

    Example:
        # update_ = _BuildUpdate(document_, metadata_, PackageRole.SHIM)
        # Output: ManifestUpdate(...)
    """
    root_ = document_.tree.getroot()
    _RequireElement(root_, "version", document_.path).text = metadata_.version
    descriptionBase_ = metadata_.description.rstrip().removesuffix(".")
    _RequireElement(root_, "description", document_.path).text = f"{descriptionBase_}: {role_.value}"
    maintainerElement_ = _RequireElement(root_, "maintainer", document_.path)
    maintainerElement_.text = metadata_.maintainer_name
    maintainerElement_.set("email", metadata_.maintainer_email)
    _RequireElement(root_, "license", document_.path).text = metadata_.license
    _SetWebsiteUrl(root_, metadata_.homepage_url, document_.path)
    return ManifestUpdate(
        path=document_.path,
        mode=document_.mode,
        original_bytes=document_.original_bytes,
        updated_bytes=_SerializeManifest(document_),
    )


def _WriteUpdate(update_: ManifestUpdate) -> bool:
    """Atomically replace a changed manifest while preserving its mode.

    Example:
        # changed_ = _WriteUpdate(update_)
        # Output: True
    """
    if update_.updated_bytes == update_.original_bytes:
        return False

    temporaryPath_: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{update_.path.name}.",
            dir=update_.path.parent,
            delete=False,
        ) as temporaryFile_:
            temporaryFile_.write(update_.updated_bytes)
            temporaryFile_.flush()
            os.fsync(temporaryFile_.fileno())
            temporaryPath_ = Path(temporaryFile_.name)
        os.chmod(temporaryPath_, update_.mode)
        os.replace(temporaryPath_, update_.path)
        temporaryPath_ = None
    finally:
        if temporaryPath_ is not None:
            temporaryPath_.unlink(missing_ok=True)
    return True


def SynchronizePackageMetadata(projectRoot_: Path, ros2Directory_: Path, version_: str) -> int:
    """Synchronize all immediate ROS package manifests from root CMake metadata.

    Example:
        # count_ = SynchronizePackageMetadata(Path("."), Path("ros2"), "1.2.3")
        # Output: 4
    """
    metadata_ = _ConfigureProjectMetadata(projectRoot_.resolve(), version_)
    manifestPaths_ = sorted(ros2Directory_.resolve().glob("*/package.xml"))
    if not manifestPaths_:
        raise ValueError(f"No immediate package.xml files found under {ros2Directory_}")

    documents_ = [_ReadManifest(path_) for path_ in manifestPaths_]
    packageNames_ = frozenset(document_.package_name for document_ in documents_)
    if len(packageNames_) != len(documents_):
        raise ValueError("Duplicate ROS package names found in immediate package manifests")
    roles_ = _PackageRoles(packageNames_)
    updates_ = [
        _BuildUpdate(document_, metadata_, roles_[document_.package_name])
        for document_ in documents_
    ]
    for update_ in updates_:
        _WriteUpdate(update_)
    return len(updates_)


def _ParseArguments(arguments_: Sequence[str] | None) -> argparse.Namespace:
    """Parse command-line arguments for the metadata synchronizer.

    Example:
        arguments_ = _ParseArguments(["--project-root", ".", "--ros2-dir", "ros2", "--version", "1.2.3"])
        print(arguments_.version)
        # Output:
        # 1.2.3
    """
    parser_ = argparse.ArgumentParser(description=__doc__)
    parser_.add_argument("--project-root", required=True, type=Path)
    parser_.add_argument("--ros2-dir", required=True, type=Path)
    parser_.add_argument("--version", required=True)
    return parser_.parse_args(arguments_)


def Main(arguments_: Sequence[str] | None = None) -> int:
    """Run the synchronization command.

    Example:
        # exitCode_ = Main(["--project-root", ".", "--ros2-dir", "ros2", "--version", "1.2.3"])
        # Output: Synchronized 4 ROS 2 package manifests.
    """
    parsedArguments_ = _ParseArguments(arguments_)
    try:
        synchronizedCount_ = SynchronizePackageMetadata(
            parsedArguments_.project_root,
            parsedArguments_.ros2_dir,
            parsedArguments_.version,
        )
    except (ET.ParseError, OSError, RuntimeError, ValueError) as error_:
        print(f"[ERROR] {error_}", file=sys.stderr)
        return 1
    print(f"Synchronized {synchronizedCount_} ROS 2 package manifests.")
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
