#include "machine_helper.hpp"

#include "ecmcStrucppLogicWrapper.hpp"
#include "generated/main.hpp"

#if __has_include("generated/main_epics_exports.hpp")
#include "generated/main_epics_exports.hpp"
#define ECMC_STRUCPP_EXPORT_INIT_FN ecmcStrucppExports::initProgram_MAINExports
#endif

namespace {
[[maybe_unused]] const char* kLogicNameExample = machineLogicExampleName();
}

#ifdef ECMC_STRUCPP_EXPORT_INIT_FN
ECMC_STRUCPP_DECLARE_LOGIC_API_WITH_EXPORTS("main",
                                            strucpp::Program_MAIN,
                                            strucpp::locatedVars,
                                            ECMC_STRUCPP_EXPORT_INIT_FN);
#else
ECMC_STRUCPP_DECLARE_LOGIC_API("main",
                               strucpp::Program_MAIN,
                               strucpp::locatedVars);
#endif
