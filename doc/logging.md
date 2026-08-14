# Dependency-free logging

The reusable logger lives in `src/utils/logging/`. It is ordinary library
infrastructure: project tailoring keeps the implementation and this guide so a
derived project normally configures and uses it rather than replacing it.

## Design

`template_project::logging::CLogger` combines the useful parts of two existing
local patterns:

- the component and severity format, ordered threshold, stream routing, and
  concise stream-based value conversion used by `future-onboard-sw`;
- the dependency-free ANSI sequences, diagnostic-stream capture, and
  concurrency requirements exercised by `spectral_raytracer_dev`.

The result deliberately stays small. It has no singleton, registry, formatting
library, file sink, timestamp policy, or asynchronous queue. Each logger owns a
component name and references caller-owned streams. A complete line is assembled
before a process-wide output mutex is acquired, so multiple logger instances can
write to a shared stream without interleaving partial messages.

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

`setLevelFromEnvironment()` reads `TEMPLATE_PROJECT_LOG_LEVEL` by default. It
accepts case-insensitive names, the aliases `off`, `fatal`, and `warn`, or a
numeric value from `0` to `6`. Missing or invalid values leave the current level
unchanged and return `false`.

```bash
TEMPLATE_PROJECT_LOG_LEVEL=debug ./build/src/bin/example_program
```

## C++ usage

```cpp
#include <utils/logging/CLogger.h>

int main()
{
    template_project::logging::CLogger objLogger_("example_program");
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

With `TEMPLATE_PROJECT_LOG_LEVEL=debug`, the output is:

```text
[example_program][INFO] Processing 3 inputs.
[example_program][DEBUG] Detailed diagnostics are enabled.
```

Custom streams make output capture explicit in tests and applications:

```cpp
std::ostringstream objOutputStream_;
std::ostringstream objDiagnosticStream_;
template_project::logging::CLogger objLogger_(
    "worker",
    template_project::logging::ELogLevel::Info,
    template_project::logging::ELogColorMode::Disabled,
    objOutputStream_,
    objDiagnosticStream_);
```

The caller must keep custom streams alive for the logger's lifetime. Logging
calls may come from multiple threads, but changing or destroying those streams
concurrently is outside the logger contract.

This logger is introduced as part of the `v1.11.0` release line. It is a local
utility implementation, not a replacement third-party logging framework.
