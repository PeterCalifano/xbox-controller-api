"""Pytest coverage for the CGamepadWrapper Python binding.

The bindings are an optional build artifact, so every test here skips cleanly
when they were not generated. Nothing asserts that a controller is attached:
each check states a contract that holds in any environment, which is what makes
the file safe to run in CI and on a developer machine alike.
"""

from __future__ import annotations

import pytest

import xbox_controller_api

pytestmark = pytest.mark.skipif(
    not xbox_controller_api.HAS_WRAPPER,
    reason=f"Python wrapper is not built: {xbox_controller_api.WRAPPER_IMPORT_ERROR}",
)

# An index far beyond any plausible device count, used to force a failed open.
IMPLAUSIBLE_JOYSTICK_INDEX: int = 9999

# Every axis accessor paired with the lower bound of its documented range.
AXIS_ACCESSORS: tuple[tuple[str, float], ...] = (
    ("leftStickX", -1.0),
    ("leftStickY", -1.0),
    ("rightStickX", -1.0),
    ("rightStickY", -1.0),
    ("leftTrigger", 0.0),
    ("rightTrigger", 0.0),
)

# Control names the library is expected to expose, in index order. Restated here
# rather than read back from the library, so a reordering is a test failure.
EXPECTED_BUTTON_NAMES: tuple[str, ...] = (
    "A",
    "B",
    "X",
    "Y",
    "LB",
    "RB",
    "LS",
    "RS",
    "Back",
    "Start",
    "Guide",
    "DpadUp",
    "DpadDown",
    "DpadLeft",
    "DpadRight",
)

BUTTON_ACCESSORS: tuple[str, ...] = (
    "buttonA",
    "buttonB",
    "buttonX",
    "buttonY",
    "leftShoulder",
    "rightShoulder",
    "leftStickClick",
    "rightStickClick",
    "back",
    "start",
    "guide",
    "dpadUp",
    "dpadDown",
    "dpadLeft",
    "dpadRight",
)


class TestGamepadWrapper:
    def test_backend_availability_is_reported_as_a_bool(self) -> None:
        assert isinstance(xbox_controller_api.CGamepadWrapper.isBackendAvailable(), bool)

    def test_fresh_wrapper_reports_rest_values(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        assert wrapper_.connected() is False
        assert wrapper_.deviceName() == ""
        assert wrapper_.lastError() == ""
        assert wrapper_.sequenceId() == 0

        for accessor_, _ in AXIS_ACCESSORS:
            assert getattr(wrapper_, accessor_)() == 0.0

        for accessor_ in BUTTON_ACCESSORS:
            assert getattr(wrapper_, accessor_)() is False

    def test_polling_without_a_device_reports_no_data(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        assert wrapper_.update() is False
        assert wrapper_.sequenceId() == 0

    def test_failed_open_reports_an_error_without_raising(self) -> None:
        """A failed attach must be a returned bool, never an exception."""
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        assert wrapper_.openIndex(IMPLAUSIBLE_JOYSTICK_INDEX) is False
        assert wrapper_.connected() is False
        assert wrapper_.lastError() != ""

        # The failure must leave every control at rest rather than at whatever
        # the previous state happened to be.
        for accessor_, _ in AXIS_ACCESSORS:
            assert getattr(wrapper_, accessor_)() == 0.0

    def test_stick_deadzone_round_trips(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        # The documented default is a pass-through, so the wrapper never
        # reshapes input the caller did not ask it to reshape.
        assert wrapper_.stickDeadzone() == 0.0

        wrapper_.setStickDeadzone(0.25)
        assert wrapper_.stickDeadzone() == 0.25

        # Out-of-range values are meaningful rather than rejected.
        wrapper_.setStickDeadzone(-1.0)
        assert wrapper_.stickDeadzone() == -1.0

    def test_full_deadzone_suppresses_the_sticks(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()
        wrapper_.setStickDeadzone(1.0)

        for accessor_ in ("leftStickX", "leftStickY", "rightStickX", "rightStickY"):
            assert getattr(wrapper_, accessor_)() == 0.0

    def test_accessor_return_types(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        assert isinstance(wrapper_.deviceName(), str)
        assert isinstance(wrapper_.lastError(), str)
        assert isinstance(wrapper_.sequenceId(), int)
        assert isinstance(wrapper_.stickDeadzone(), float)
        assert isinstance(wrapper_.connected(), bool)

        for accessor_, _ in AXIS_ACCESSORS:
            assert isinstance(getattr(wrapper_, accessor_)(), float)

        for accessor_ in BUTTON_ACCESSORS:
            assert isinstance(getattr(wrapper_, accessor_)(), bool)

    def test_indexed_buttons_match_the_expected_control_list(self) -> None:
        """The indexed API is what lets a script iterate controls generically."""
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        assert wrapper_.buttonCount() == len(EXPECTED_BUTTON_NAMES)
        assert wrapper_.buttonCount() == len(BUTTON_ACCESSORS)

        actual_names_ = tuple(
            wrapper_.buttonName(index_) for index_ in range(wrapper_.buttonCount())
        )
        assert actual_names_ == EXPECTED_BUTTON_NAMES

    def test_indexed_buttons_agree_with_the_named_accessors(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        for index_, accessor_ in enumerate(BUTTON_ACCESSORS):
            assert wrapper_.buttonPressed(index_) == getattr(wrapper_, accessor_)()

    def test_out_of_range_button_index_is_handled(self) -> None:
        """A bad index from a script must not read out of bounds."""
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        for index_ in (-1, wrapper_.buttonCount(), 9999):
            assert wrapper_.buttonName(index_) == "Unknown"
            assert wrapper_.buttonPressed(index_) is False

    def test_close_is_safe_before_any_open(self) -> None:
        wrapper_ = xbox_controller_api.CGamepadWrapper()

        wrapper_.close()
        wrapper_.close()

        assert wrapper_.connected() is False
        assert wrapper_.sequenceId() == 0
