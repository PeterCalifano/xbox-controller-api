# Controller Input API implementation record

Status: completed on 2026-08-15. This file records the implementation decisions
and verification performed during the first controller-input release. For setup,
API usage, and ROS 2 parameters, see [Controller Input API](../controller_api.md).

## Scope

Version 1 reads xpad-class controllers on Linux through SDL2's GameController
API. It publishes sticks, triggers, buttons, and the D-pad. Rumble, LEDs, and
controller enumeration beyond an explicit joystick index are deferred.

The implementation provides:

- a C++20 core with normalized `SGamepadState` snapshots;
- an SDL2 hardware source and a hardware-free scripted source;
- deadzone and button-edge helpers;
- a gtwrap Python facade and C++ examples;
- a ROS 2 lifecycle node that publishes `sensor_msgs/msg/Joy`.

## Final design

```
Controller → SDL2 GameController → CSdlGamepadSource → SGamepadState
                                                   ├→ C++ and Python consumers
                                                   └→ ROS 2 Joy publisher
```

- Stick axes are normalized to `[-1, 1]`, with positive Y pointing upward.
  Triggers are normalized to `[0, 1]`.
- The source publishes raw normalized values. Applications opt into deadzones
  and edge detection through `GamepadFilters` and `GamepadControls`.
- `open()` attaches a controller and `update()` publishes a sample. A disconnect
  publishes one neutral sample and stops updates; reconnecting requires `open()`.
- The SDL2 backend is optional for the core library. When unavailable, the API
  remains present and `open()` reports the reason through `lastError()`.
- The ROS 2 overlay requires SDL2 and publishes the standard `sensor_msgs/Joy`
  message instead of defining custom interfaces.

## Implementation batches

| Batch | Contents | Commit |
|---|---|---|
| 1 | Core state, polling interface, filters, and scripted source | `ed7c959` |
| 2 | SDL2 CMake configuration and feature plumbing | `cdfa00a` |
| 3 | SDL2 controller source and invariant tests | `943691f` |
| 4 | C++ examples, diagnostic, and installed consumer example | `26665d8`, `b098e00` |
| 5 | Python facade, bindings, sample, and tests | `c5061e7` |
| 5b | Shared control identity API | `b5aaf68` |
| 6 | Placeholder removal and public documentation | `8fbd86f` |
| 7 | ROS 2 Joy bridge and lifecycle publisher | `001dc43` |

## Implementation notes

- The SDL2 fallback links resolved pkg-config flags instead of exporting a
  `PkgConfig::` target, which would not exist in an installed consumer.
- `SDL2_ENABLED` records the resolved backend state. `ENABLE_SDL2` remains the
  configuration request.
- `GamepadControls` is the single source of truth for button names, snapshot
  members, and iteration order.
- `CGamepadWrapper` conditions one snapshot per poll, avoiding repeated
  deadzone calculations in scalar accessors.
- `sensor_msgs/Joy` uses reliable QoS for compatibility with existing joystick
  consumers such as `teleop_twist_joy`.

## Recorded verification

The following checks were completed while implementing the feature:

| Check | Result |
|---|---|
| Core build, wrapper build, and tests | 37/37 tests passed with the configured warnings enabled |
| SDL2 disabled build and wrapper | 37/37 tests passed; the backend reported its fixed unavailable diagnostic |
| Strict SDL2 configuration matrix | Available dependency enabled the backend; missing dependency failed in strict mode and selected the stub otherwise |
| Installed consumer project | Configured, built, and ran against the installed package |
| ROS 2 overlay | 4 packages, 11 tests, 0 failures |
| Live controller | SDL reported an X360 Controller; C++ and Python examples ran at 50 Hz |

## Follow-ups

- Remove the stale `EXCLUDED_LIST "example_build"` entry from
  `examples/CMakeLists.txt`.
- Source discovery uses `file(GLOB ...)` without `CONFIGURE_DEPENDS`; add or
  remove sources through a fresh configure, or use `./build_ros2.sh --clean`.
- Remove a stale install tree before checking which headers are currently
  exported.
- Rumble, LEDs, expanded multi-controller discovery, and viewer integration are
  deferred to a later version.
