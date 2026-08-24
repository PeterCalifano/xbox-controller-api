"""Live controller read-out through the xbox_controller_api Python bindings.

Mirrors ``examples/xbox_controller_api_examples/example_read_controller.cpp``.
It polls at a fixed rate, reports button transitions, and stops on disconnect.

This file is not part of any build. Run it against a built wrapper, for example:

    PYTHONPATH=build/python python3 examples/python/example_read_controller.py 5

Example:
    python3 example_read_controller.py 5

Output:
    Reading 'X360 Controller' at 50 Hz for 5 s.
    Press Start to quit early. Sticks use a 0.15 deadzone.
      [37] A pressed
      [42] A released
      LS(0.00, 0.62)  RS(0.00, 0.00)  LT=0.00  RT=0.41
    Start pressed; quitting.
    Completed 96 polls; last sequence id was 97.
"""

from __future__ import annotations

import argparse
import sys
import time

import xbox_controller_api

# Poll period matching the 50 Hz sampling rate of the demo.
POLL_PERIOD_S: float = 0.02

# Runtime applied when no argument is supplied.
DEFAULT_RUNTIME_S: int = 20

# Upper bound accepted for the runtime argument, in seconds.
MAXIMUM_RUNTIME_S: int = 3600

# Deadzone applied to the sticks, inside the suggested 0.10 to 0.20 range.
DISPLAY_DEADZONE: float = 0.15

# Print the axis summary at 5 Hz.
AXIS_PRINT_DECIMATION: int = 10

# Name of the control used to quit early, as reported by the library.
QUIT_BUTTON_NAME: str = "Start"


def button_names(wrapper_: xbox_controller_api.CGamepadWrapper) -> tuple[str, ...]:
    """List every control the library exposes, in its canonical order.

    Args:
        wrapper_: Wrapper to query.

    Returns:
        Control names in index order.
    """
    return tuple(wrapper_.buttonName(index_) for index_ in range(wrapper_.buttonCount()))


def read_button_states(wrapper_: xbox_controller_api.CGamepadWrapper) -> dict[str, bool]:
    """Sample every control from the current snapshot.

    Args:
        wrapper_: Wrapper whose most recent poll should be read.

    Returns:
        Mapping from control name to its pressed state.
    """
    return {
        wrapper_.buttonName(index_): wrapper_.buttonPressed(index_)
        for index_ in range(wrapper_.buttonCount())
    }


def print_button_edges(
    previous_states_: dict[str, bool],
    current_states_: dict[str, bool],
    sequence_id_: int,
) -> None:
    """Print only the buttons that changed between two consecutive samples.

    Args:
        previous_states_: Button states from the preceding poll.
        current_states_: Button states from the current poll.
        sequence_id_: Sequence number of the current sample.
    """
    for name_, is_pressed_ in current_states_.items():
        was_pressed_ = previous_states_.get(name_, False)

        if is_pressed_ and not was_pressed_:
            print(f"  [{sequence_id_}] {name_} pressed")
        elif was_pressed_ and not is_pressed_:
            print(f"  [{sequence_id_}] {name_} released")


def print_axes(wrapper_: xbox_controller_api.CGamepadWrapper) -> None:
    """Print the conditioned stick and trigger values on a single line.

    Args:
        wrapper_: Wrapper whose most recent poll should be read.
    """
    print(
        f"  LS({wrapper_.leftStickX():.2f}, {wrapper_.leftStickY():.2f})"
        f"  RS({wrapper_.rightStickX():.2f}, {wrapper_.rightStickY():.2f})"
        f"  LT={wrapper_.leftTrigger():.2f}  RT={wrapper_.rightTrigger():.2f}"
    )


def has_axis_activity(wrapper_: xbox_controller_api.CGamepadWrapper) -> bool:
    """Report whether any conditioned axis has left its rest position.

    Args:
        wrapper_: Wrapper whose most recent poll should be read.

    Returns:
        True when at least one stick or trigger is deflected.
    """
    deflections_ = (
        abs(wrapper_.leftStickX()),
        abs(wrapper_.leftStickY()),
        abs(wrapper_.rightStickX()),
        abs(wrapper_.rightStickY()),
        wrapper_.leftTrigger(),
        wrapper_.rightTrigger(),
    )

    return any(deflection_ > 0.0 for deflection_ in deflections_)


def parse_arguments() -> argparse.Namespace:
    """Parse the optional bounded runtime argument.

    Integer values outside ``(0, MAXIMUM_RUNTIME_S]`` are replaced with the
    default. argparse reports non-integer input as a command-line error.

    Returns:
        Parsed arguments carrying a validated ``runtime_seconds``.
    """
    parser_ = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser_.add_argument(
        "runtime_seconds",
        nargs="?",
        default=DEFAULT_RUNTIME_S,
        type=int,
        help=f"How long to poll before exiting, in seconds (default: {DEFAULT_RUNTIME_S}).",
    )

    arguments_ = parser_.parse_args()

    if not 0 < arguments_.runtime_seconds <= MAXIMUM_RUNTIME_S:
        print(
            f"Ignoring invalid runtime argument '{arguments_.runtime_seconds}'; "
            f"using {DEFAULT_RUNTIME_S} s",
            file=sys.stderr,
        )
        arguments_.runtime_seconds = DEFAULT_RUNTIME_S

    return arguments_


def main() -> int:
    """Poll an attached controller and report its activity.

    Returns:
        Process exit status. Missing bindings, backend, or hardware are normal
        outcomes for this sample.
    """
    arguments_ = parse_arguments()

    if not xbox_controller_api.HAS_WRAPPER:
        print(f"Python bindings are not available: {xbox_controller_api.WRAPPER_IMPORT_ERROR}")
        return 0

    wrapper_ = xbox_controller_api.CGamepadWrapper()

    if not wrapper_.isBackendAvailable():
        print("SDL2 backend is not compiled into this build; nothing to read.")
        print("Reconfigure with -DENABLE_SDL2=ON to enable it.")
        return 0

    if not wrapper_.open():
        print(f"No controller available: {wrapper_.lastError()}")
        return 0

    wrapper_.setStickDeadzone(DISPLAY_DEADZONE)

    print(
        f"Reading '{wrapper_.deviceName()}' at 50 Hz for {arguments_.runtime_seconds} s.\n"
        f"Press Start to quit early. Sticks use a {DISPLAY_DEADZONE} deadzone."
    )

    # Start from an all-released baseline so a button already held at startup
    # registers as a press on the first poll.
    previous_states_ = {name_: False for name_ in button_names(wrapper_)}
    deadline_ = time.monotonic() + arguments_.runtime_seconds
    poll_count_ = 0

    while time.monotonic() < deadline_:
        frame_start_ = time.monotonic()

        # A failed poll after open() means the controller was detached.
        if not wrapper_.update():
            print(f"Controller detached after {poll_count_} polls; stopping.")
            break

        current_states_ = read_button_states(wrapper_)
        print_button_edges(previous_states_, current_states_, wrapper_.sequenceId())

        if current_states_[QUIT_BUTTON_NAME] and not previous_states_[QUIT_BUTTON_NAME]:
            print("Start pressed; quitting.")
            break

        if poll_count_ % AXIS_PRINT_DECIMATION == 0 and has_axis_activity(wrapper_):
            print_axes(wrapper_)

        previous_states_ = current_states_
        poll_count_ += 1

        # Keep the poll cadence relative to the frame start.
        remaining_s_ = (frame_start_ + POLL_PERIOD_S) - time.monotonic()
        if remaining_s_ > 0.0:
            time.sleep(remaining_s_)

    print(f"Completed {poll_count_} polls; last sequence id was {wrapper_.sequenceId()}.")
    wrapper_.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
