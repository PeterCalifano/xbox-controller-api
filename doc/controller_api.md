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

Each layer depends only inward. Conditioning is never applied by the backend:
a snapshot carries what the hardware reported, and any deadzone or edge
detection is an explicit consumer decision.

## Linux setup

The in-tree `xpad` driver handles wired Xbox controllers and is present in
stock kernels. No `xboxdrv` or `xpadneo` installation is expected.

Confirm the kernel enumerated the pad:

```bash
ls -l /dev/input/by-id/ | grep -i controller
```

A wired Xbox 360 pad appears as a pair of nodes, for example
`usb-©Microsoft_Corporation_Controller_...-event-joystick` and
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
downward. This library reports positive upward, so a forward push yields a
positive value and the sign matches the usual convention for a velocity or
attitude command. The backend applies `InvertAxis` to the two Y axes and to
nothing else. Code ported from raw SDL must drop its own negation.

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

Rescaling matters. A bare cut-off leaves the axis unable to reach full
deflection and makes the response jump as the stick crosses the threshold; this
form is continuous at the boundary and still reaches `±1.0`.

Edge cases are defined rather than rejected: `d ≤ 0` clamps only, and `d ≥ 1`
suppresses the axis entirely.

**The default is 0.0 everywhere** — a pass-through — so the library never
reshapes input that was not asked for. A useful starting value is **0.10 to
0.20**; measure your own device with `xbox_controller_monitor`, which
deliberately reports unconditioned values so drift stays visible.

Condition a whole snapshot once per sample rather than per read:

```cpp
const SGamepadState strConditioned = ApplyStickDeadzone(objSource.state(), 0.15);
```

Triggers are never deadzoned by this helper. They are unidirectional, so
suppressing a light pull is a separate decision belonging to the caller.

## Disconnect and reconnect

A hot unplug is reported, never healed:

- `update()` publishes the canonical zeroed snapshot once and returns false.
- `state()` then reports every axis at rest and `bConnected_` false, so a
  consumer that ignores the connection flag still receives a neutral command
  instead of the last value read before the unplug.
- Subsequent `update()` calls keep returning false without republishing.

Recovery is an explicit `open()`. The library does not reconnect on its own,
because retry timing is a policy only the application can choose.

`close()` is safe to call at any time, is idempotent, and publishes the same
neutral snapshot when a device was attached.

## Coexisting with an application that owns SDL

The backend never pumps or consumes the SDL event queue. It refreshes state
with `SDL_GameControllerUpdate()`, so an application running its own
`SDL_PollEvent` loop keeps every event it would otherwise have lost.

Supporting details:

- `SDL_InitSubSystem` is reference counted, and the source releases exactly the
  reference it took.
- Hints are set only when the source is the first initializer, so a host
  application's deliberate configuration is never overridden.
- Device-added events are flushed only when the source started the subsystem.

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

`CGamepadWrapper` is a flat facade: no overloads, one accessor per control, and
`openIndex()` in place of an overloaded `open()`, because the binding generator
handles neither overloads nor aggregate references. It caches one conditioned
snapshot per poll, so every accessor reports a consistent view of the same
sample and the deadzone is applied once rather than per read.

## Builds without SDL2

`ENABLE_SDL2` defaults to ON. When SDL2 is missing, the default non-strict
configure warns and stubs the backend out; `ENABLE_SDL2_STRICT=ON` turns the
same situation into a configure error, which is what CI uses.

The class and its full API survive either way, so consumers branch on a value
rather than on a preprocessor symbol:

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

The ROS 2 overlay forces `ENABLE_SDL2=OFF`, so the overlay never depends on
SDL2.

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

`pushFrame()` always represents an attached device and `pushDisconnect()` is the
only way to express a detach, so the disconnect contract has exactly one entry
point. An exhausted script returns false but leaves the last snapshot standing,
which is how "no new data" stays distinguishable from "device gone".

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
