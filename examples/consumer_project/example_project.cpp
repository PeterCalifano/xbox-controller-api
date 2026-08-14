/// @file example_project.cpp
/// @brief Demonstrates consuming an installed xbox_controller_api package.

#include "example_project.h"

int main()
{
    using namespace xbox_controller_api::logging;

    CLogger objLogger_("example_consumer_project", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Hello, World! This is an example of a project using xbox_controller_api "
                    "as a library through CMake.");

    // Call the placeholder function from the xbox_controller_api library
    placeholder::placeholder_fcn();

    return 0;
}
