# Example IOC shell snippet for a direct-mapped motion velocity STruCpp logic
# library.
#
# Prerequisite: axis 1 must already be configured in the IOC before this script
# is loaded, so that these ecmcDataItems exist:
#   %IL0   -> ax1.enc.actpos
#   %QL0   -> ax1.traj.targetvel
#   %QX8.0 -> ax1.drv.enable
#
# This example uses the preferred startup-linked mapping-file mode.

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/motion_velocity_direct_logic.so")

require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},MEMORY_BYTES=16,REPORT=1"
