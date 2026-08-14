#include "placeholder.h"
#include <utils/logging/CLogger.h>

namespace placeholder
{
    void placeholder_fcn()
    {
        template_project::logging::CLogger objLogger_("placeholder");
        objLogger_.setLevelFromEnvironment();
        objLogger_.info("Hello, World! I'm a placeholder function, yuppy.");
    }
} // namespace placeholder
