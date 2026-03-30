#include <cstdio>

void ecmcStrucppDebugPrint(const char* message) {
  std::fprintf(stdout,
               "[ecmc_plugin_strucpp] ST: %s\n",
               message ? message : "");
  std::fflush(stdout);
}
