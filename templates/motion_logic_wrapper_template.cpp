#include "ecmcStrucppLogicWrapper.hpp"
#include "generated/my_motion_app.hpp"

#if __has_include("generated/my_motion_app_epics_exports.hpp")
#include "generated/my_motion_app_epics_exports.hpp"
#define ECMC_STRUCPP_EXPORT_INIT_FN ecmcStrucppExports::initProgram_MY_MOTION_APPExports
#endif

#ifdef ECMC_STRUCPP_EXPORT_INIT_FN
ECMC_STRUCPP_DECLARE_LOGIC_API_WITH_EXPORTS("my_motion_app_logic",
                                            strucpp::Program_MY_MOTION_APP,
                                            strucpp::locatedVars,
                                            ECMC_STRUCPP_EXPORT_INIT_FN);
#else
ECMC_STRUCPP_DECLARE_LOGIC_API("my_motion_app_logic",
                               strucpp::Program_MY_MOTION_APP,
                               strucpp::locatedVars);
#endif
