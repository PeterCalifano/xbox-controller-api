# Dependency-free logging

The reusable logger lives in `src/utils/logging/`. It is ordinary library
infrastructure and is available to applications that link the core library.

## Design

`xbox_controller_api::logging::CLogger` is a small dependency-free logger. Each
instance owns a component name and references caller-owned streams. It formats a
complete line before taking the process-wide output mutex, preventing partial
messages from interleaving on a shared stream.

The stable line contract is:

```text
[component][LEVEL] message
```

`Critical`, `Error`, and `Warning` use the diagnostic stream (`std::clog` by
default). `Info`, `Debug`, and `Trace` use the ordinary output stream
(`std::cout` by default). ANSI colors are explicit and disabled by default,
which keeps redirected output and CI logs deterministic.

## Levels and environment configuration

Levels are ordered from `Quiet` (`0`) through `Trace` (`6`). A configured level
includes every less-verbose severity. For example, `Info` includes critical,
error, warning, and info messages but filters debug and trace messages.

`setLevelFromEnvironment()` reads `XBOX_CONTROLLER_API_LOG_LEVEL` by default. It
accepts case-insensitive names, the aliases `off`, `fatal`, and `warn`, or a
numeric value from `0` to `6`. Missing or invalid values leave the current level
unchanged and return `false`.

```bash
XBOX_CONTROLLER_API_LOG_LEVEL=debug ./build/src/bin/xbox_controller_monitor
```

## C++ usage

```cpp
#include <utils/logging/CLogger.h>

int main()
{
    xbox_controller_api::logging::CLogger objLogger_("example_program");
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Processing ", 3, " inputs.");
    objLogger_.debug("Detailed diagnostics are enabled.");
    return 0;
}
```

With the default level, the output is:

```text
[example_program][INFO] Processing 3 inputs.
```

With `XBOX_CONTROLLER_API_LOG_LEVEL=debug`, the output is:

```text
[example_program][INFO] Processing 3 inputs.
[example_program][DEBUG] Detailed diagnostics are enabled.
```

Custom streams make output capture explicit in tests and applications:

```cpp
std::ostringstream objOutputStream_;
std::ostringstream objDiagnosticStream_;
xbox_controller_api::logging::CLogger objLogger_(
    "worker",
    xbox_controller_api::logging::ELogLevel::Info,
    xbox_controller_api::logging::ELogColorMode::Disabled,
    objOutputStream_,
    objDiagnosticStream_);
```

The caller must keep custom streams alive for the logger's lifetime. Logging
calls may come from multiple threads, but changing or destroying those streams
concurrently is outside the logger contract.

This is a local utility, not a replacement for a full logging framework.
