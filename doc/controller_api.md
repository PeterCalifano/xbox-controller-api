# Controller Input API

`xbox_controller_api` reads xpad-class game controllers on Linux. Version 1 is
input only: sticks, triggers, buttons, and the directional pad. Rumble and LED
control are not implemented.

The backend is SDL2's GameController API, which supplies the mapping database
that turns an arbitrary joystick into named controls. A wired Xbox 360 pad is
the reference device, but nothing in the API is specific to it.

## Layers

```
Xbox pad → kernel xpad → SDL2 GameController (mapping DB)
                              ↓
              CSdlGamepadSource : CGamepadSource     (device access, pimpl)
                              ↓
                    SGamepadState snapshot           (normalized values)
                              ↓
        GamepadFilters (scalar math) + GamepadControls (snapshot operations)
                              ↓
        consumers: C++ app | CGamepadWrapper → Python | ROS node | viewer
```

Each consumer depends on the core library, not on another consumer. The backend
publishes normalized hardware values; applications apply deadzones and edge
detection when needed.

## Linux setup

The in-tree `xpad` driver handles wired Xbox controllers and is present in
stock kernels. No `xboxdrv` or `xpadneo` installation is expected.

Confirm the kernel enumerated the pad:

```bash
ls -l /dev/input/by-id/ | grep -i controller
```

A wired Xbox 360 pad appears as a pair of nodes, for example
`usb-Microsoft_Corporation_Controller_...-event-joystick` and
`...-joystick`. To watch raw events before involving this library:

```bash
sudo evtest /dev/input/by-id/usb-..._Controller_...-event-joystick
```

**Permissions.** On a normal desktop distribution, `systemd-logind` grants the
active local session access to input devices, so no extra configuration is
needed. If the device is not readable — typically over SSH, in a container, or
under a service account — grant access with a udev rule scoped to that device
rather than adding the account to a broad group that also exposes every
keyboard and mouse on the machine.

## Reading a controller from C++

```cpp
#include <xbox_controller_api/CSdlGamepadSource.h>
#include <xbox_controller_api/GamepadControls.h>

using namespace xbox_controller_api;

CSdlGamepadSource objSource;

if (!objSource.open())
{
    // Absent hardware is a normal outcome, reported rather than thrown.
    std::cerr << objSource.lastError() << "\n";
    return 0;
}

SGamepadState strPreviousState;

while (objSource.update())
{
    const SGamepadState strState = ApplyStickDeadzone(objSource.state(), 0.15);

    if (ClassifyButtonEdge(strPreviousState, strState, EGamepadButton::A) ==
        EButtonEdge::Pressed)
    {
        std::cout << "A pressed\n";
    }

    strPreviousState = strState;
}
```

`update()` returning false means no connected sample was published: either the
device is gone or nothing is open. It never blocks and never sleeps, so the
caller owns the poll cadence.

## Value conventions

| Field | Range | Meaning |
|---|---|---|
| `dLeftStickX_`, `dRightStickX_` | `[-1, 1]` | Positive to the right |
| `dLeftStickY_`, `dRightStickY_` | `[-1, 1]` | **Positive upward** |
| `dLeftTrigger_`, `dRightTrigger_` | `[0, 1]` | 0 fully released |
| button fields | `bool` | True while held |
| `bConnected_` | `bool` | True only while a device is answering polls |
| `ui64SequenceId_` | `uint64` | Increments once per consumed sample; 0 means none yet |
| `ui64TimestampNs_` | `uint64` | Steady-clock capture time; only differences are meaningful |

**The Y axis is inverted relative to SDL.** SDL reports stick Y as positive
downward; this library reports it as positive upward. The backend applies
`InvertAxis` to the two Y axes. Code ported from raw SDL must not negate them a
second time.

The negative extreme of a raw 16-bit axis is `-32768`, one count larger in
magnitude than the positive extreme `32767`. Normalization scales by the
positive full scale and clamps, so both extremes map to exactly `±1.0`.

## Control identities

`GamepadControls.h` names the controls so consumers can iterate them instead of
hard-coding a list:

```cpp
for (const EGamepadButton enumButton : AllGamepadButtons())
{
    if (GetButton(strState, enumButton))
    {
        std::cout << GetGamepadButtonName(enumButton) << " is held\n";
    }
}
```

| `EGamepadButton` | Name | SDL constant | Physical control |
|---|---|---|---|
| `A` | `"A"` | `SDL_CONTROLLER_BUTTON_A` | A (bottom face) |
| `B` | `"B"` | `SDL_CONTROLLER_BUTTON_B` | B (right face) |
| `X` | `"X"` | `SDL_CONTROLLER_BUTTON_X` | X (left face) |
| `Y` | `"Y"` | `SDL_CONTROLLER_BUTTON_Y` | Y (top face) |
| `LeftShoulder` | `"LB"` | `SDL_CONTROLLER_BUTTON_LEFTSHOULDER` | Left bumper |
| `RightShoulder` | `"RB"` | `SDL_CONTROLLER_BUTTON_RIGHTSHOULDER` | Right bumper |
| `LeftStickClick` | `"LS"` | `SDL_CONTROLLER_BUTTON_LEFTSTICK` | Left stick press |
| `RightStickClick` | `"RS"` | `SDL_CONTROLLER_BUTTON_RIGHTSTICK` | Right stick press |
| `Back` | `"Back"` | `SDL_CONTROLLER_BUTTON_BACK` | Back / View |
| `Start` | `"Start"` | `SDL_CONTROLLER_BUTTON_START` | Start / Menu |
| `Guide` | `"Guide"` | `SDL_CONTROLLER_BUTTON_GUIDE` | Xbox guide button |
| `DpadUp` | `"DpadUp"` | `SDL_CONTROLLER_BUTTON_DPAD_UP` | D-pad up |
| `DpadDown` | `"DpadDown"` | `SDL_CONTROLLER_BUTTON_DPAD_DOWN` | D-pad down |
| `DpadLeft` | `"DpadLeft"` | `SDL_CONTROLLER_BUTTON_DPAD_LEFT` | D-pad left |
| `DpadRight` | `"DpadRight"` | `SDL_CONTROLLER_BUTTON_DPAD_RIGHT` | D-pad right |

Enumerator values are contiguous from zero and their order is part of the
contract, so they are usable as indices in bindings and serialization formats.
Append new controls at the end.

The four D-pad directions are independent booleans rather than a hat value, so
diagonals stay representable without decoding.

## Deadzones

Sticks do not return exactly to zero. A resting pad commonly reports a few
percent of deflection, and a worn one reports more. `ApplyRescaledDeadzone`
suppresses that band and rescales what survives:

```
                sign(x) · (|x| − d) / (1 − d)     for |x| > d
output(x, d) =
                0                                 for |x| ≤ d
```

Rescaling keeps the response continuous at the boundary and preserves full
deflection.

For `d ≤ 0`, the helper only clamps the input. For `d ≥ 1`, it suppresses the
axis entirely.

**The default is 0.0**, which passes values through unchanged. Start with a
value between **0.10 and 0.20**, then measure your device with
`xbox_controller_monitor`, which reports unconditioned values.

Condition a whole snapshot once per sample rather than per read:

```cpp
const SGamepadState strConditioned = ApplyStickDeadzone(objSource.state(), 0.15);
```

This helper does not alter triggers. Apply any trigger threshold separately.

## Disconnect and reconnect

When a controller is unplugged:

- `update()` publishes the canonical zeroed snapshot once and returns false.
- `state()` then reports every axis at rest and `bConnected_` false.
- Subsequent `update()` calls keep returning false without republishing.

Call `open()` to reconnect. The library does not retry automatically.

`close()` is safe to call at any time, is idempotent, and publishes the same
neutral snapshot when a device was attached.

## Coexisting with an application that owns SDL

The backend never pumps or consumes the SDL event queue. It refreshes state
with `SDL_GameControllerUpdate()`, so an application running its own
`SDL_PollEvent` loop keeps every event it would otherwise have lost.

When an application also owns SDL:

- `SDL_InitSubSystem` is reference counted, and the source releases exactly the
  reference it took.
- Hints are set only when the source initializes SDL first.
- Device and controller events remain available to the application's event
  loop regardless of which component initialized SDL first.

One source instance must be used from a single thread. SDL's game controller
subsystem does not support concurrent access to one device.

## Multiple controllers

`open()` with no argument attaches to the lowest-numbered device SDL recognizes
as a game controller. Pass an explicit joystick index to select another:

```cpp
objSource.open(1);
```

An index that names no game controller fails with a message in `lastError()`.

## Reading a controller from Python

```python
import xbox_controller_api

wrapper = xbox_controller_api.CGamepadWrapper()

if wrapper.open():
    wrapper.setStickDeadzone(0.15)
    wrapper.update()
    print(wrapper.deviceName(), wrapper.leftStickX(), wrapper.buttonA())

    for index in range(wrapper.buttonCount()):
        if wrapper.buttonPressed(index):
            print(wrapper.buttonName(index), "is held")
else:
    print("no controller:", wrapper.lastError())
```

`CGamepadWrapper` provides one accessor per control and `openIndex()` instead
of an overloaded `open()`, which keeps the generated bindings simple. It caches
one conditioned snapshot per poll, so every accessor refers to the same sample.

## Builds without SDL2

`ENABLE_SDL2` defaults to ON. When SDL2 is missing, the default non-strict
configure warns and stubs the backend out; `ENABLE_SDL2_STRICT=ON` turns the
same situation into a configure error, which is what CI uses.

The class remains available in either build configuration:

- `CSdlGamepadSource::isBackendAvailable()` returns false.
- `open()` returns false and sets a fixed diagnostic in `lastError()`.
- `update()` returns false; every accessor reports its rest value.
- The pure layer — `SGamepadState`, `GamepadFilters`, `GamepadControls`, and
  `CScriptedGamepadSource` — is unaffected and fully testable.

| Option | Default | Effect |
|---|---|---|
| `ENABLE_SDL2` | ON | Build the SDL2 backend; warn and stub it out when SDL2 is absent |
| `ENABLE_SDL2_STRICT` | OFF | Fail configuration instead of warning when SDL2 is absent |

`XBOX_CONTROLLER_API_HAS_SDL2` is defined in the generated `config.h` when the
backend was compiled in.

The ROS 2 overlay is the exception: it forces `ENABLE_SDL2=ON` and
`ENABLE_SDL2_STRICT=ON`, because a node that publishes controller data has no
use for a stubbed backend.

## ROS 2

The overlay publishes the standard `sensor_msgs/msg/Joy` on `~/joy`, so
`teleop_twist_joy`, `joy_teleop`, rqt tooling and rosbag work without glue.

```bash
./build_ros2.sh
source ros2/install/setup.bash
ros2 launch xbox_controller_api_spinup xbox_controller_api.launch.py
ros2 topic echo /xbox_controller/joy
```

Axis order is `[LeftStickX, LeftStickY, RightStickX, RightStickY, LeftTrigger,
RightTrigger]`, carrying the library conventions rather than the raw SDL ones,
so stick Y is positive upward. Buttons follow `AllGamepadButtons()` order, which
is the same stable contract documented above.

| Parameter | Default | Meaning |
|---|---|---|
| `joystick_index` | `-1` | Device to open; negative selects the lowest-numbered game controller |
| `publish_rate_hz` | `50.0` | Poll and publish rate, accepted in `[1, 1000]` |
| `stick_deadzone` | `0.0` | Deadzone applied to sticks before publishing |
| `frame_id` | `xbox_controller` | Frame id placed in the message header |

The node is a lifecycle node. `on_configure` reads parameters and creates the
publisher without opening hardware. `on_activate` opens the controller and
starts the timer; it fails when no controller is available. `on_deactivate`
stops the timer and closes the device. A detach publishes one neutral message,
then stops publication until the node is deactivated and activated again.

## Testing without hardware

`CScriptedGamepadSource` replays a queued script of samples through the same
`CGamepadSource` interface, so consumer logic can be tested deterministically
with no device, no driver, and no SDL:

```cpp
CScriptedGamepadSource objSource;

SGamepadState strFrame;
strFrame.bButtonA_ = true;
objSource.pushFrame(strFrame);
objSource.pushDisconnect();

while (objSource.pendingFrameCount() > 0U)
{
    const bool bConnected = objSource.update();
    // ...
}
```

`pushFrame()` represents an attached device and `pushDisconnect()` represents a
detach. An exhausted script returns false but leaves the last snapshot intact,
which distinguishes "no new data" from "device gone".

## Running the examples

```bash
./build_lib.sh                                         # backend on by default
./build/examples/xbox_controller_api_examples/example_read_controller 10
./build/examples/xbox_controller_api_examples/example_scripted_replay
./build/src/bin/xbox_controller_monitor                # Ctrl-C to stop
```

- `example_read_controller` polls at 50 Hz for a bounded time, prints button
  transitions, and quits early on Start.
- `example_scripted_replay` needs no hardware and behaves identically in builds
  without the backend.
- `xbox_controller_monitor` is the installed diagnostic. It reports only what
  changed, applies no deadzone so drift stays visible, and exits cleanly on
  SIGINT or SIGTERM.

The Python sample mirrors the C++ one:

```bash
./build_lib.sh -p
PYTHONPATH=build/python python3 examples/python/example_read_controller.py 10
```
