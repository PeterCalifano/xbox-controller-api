#include <template_src/placeholder.h>
#include <utils/logging/CLogger.h>

int main()
{
    template_project::logging::CLogger objLogger_("example_program");
    objLogger_.setLevelFromEnvironment();
    objLogger_.info("Running template example program.");
    objLogger_.debug("Detailed diagnostics are enabled.");

    placeholder::placeholder_fcn();
    return 0;
}
