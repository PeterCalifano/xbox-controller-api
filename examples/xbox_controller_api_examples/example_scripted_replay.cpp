/// @file example_build.cpp
/// @brief Demonstrates xbox_controller_api logging and placeholder usage.

#include <xbox_controller_api/placeholder.h>
#include <utils/logging/CLogger.h>

int main()
{
    using namespace xbox_controller_api::logging;

    CLogger objLogger_("example_build", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Hello, World! This is an example file for the template.");
    placeholder::placeholder_fcn();

    // Example output:
    // [example_build][INFO] Hello, World! This is an example file for the template.

    return 0;
}
