/// @file example_project.cpp
/// @brief Demonstrates consuming an installed template_project package.

#include "example_project.h"

int main()
{
    using namespace template_project::logging;

    CLogger objLogger_("example_consumer_project", ELogLevel::Info);
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Hello, World! This is an example of a project using template_project "
                    "as a library through CMake.");

    // Call the placeholder function from the template_src library
    placeholder::placeholder_fcn();

    return 0;
}
