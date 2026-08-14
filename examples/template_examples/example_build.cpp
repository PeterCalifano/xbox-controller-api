/// @file example_build.cpp
/// @brief Demonstrates template_project logging and placeholder usage.

#include <template_src/placeholder.h>
#include <utils/logging/CLogger.h>

int main()
{
    using namespace template_project::logging;

    CLogger objLogger_("example_build", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Hello, World! This is an example file for the template.");
    placeholder::placeholder_fcn();

    // Example output:
    // [example_build][INFO] Hello, World! This is an example file for the template.

    return 0;
}
