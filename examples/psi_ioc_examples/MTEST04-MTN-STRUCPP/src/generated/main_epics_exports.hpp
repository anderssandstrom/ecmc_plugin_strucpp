#pragma once

#include <vector>

#include "ecmcStrucppLogicIface.hpp"
#include "generated/main.hpp"

namespace ecmcStrucppExports {

inline void initProgram_MAINExports(strucpp::Program_MAIN& program, std::vector<ecmcStrucppExportedVar>& out) {
  out.clear();
  out.push_back({
    "plugin.strucpp0.main.cycle_counter",
    program.CYCLE_COUNTER.raw_ptr(),
    ECMC_STRUCPP_EXPORT_S16,
    0,
    ECMC_STRUCPP_EXPORT_FLAG_NONE,
    0,
  });
  out.push_back({
    "plugin.strucpp0.main.actual_pos_export",
    program.ACTUAL_POS_EXPORT.raw_ptr(),
    ECMC_STRUCPP_EXPORT_S16,
    0,
    ECMC_STRUCPP_EXPORT_FLAG_NONE,
    0,
  });
}

}  // namespace ecmcStrucppExports
