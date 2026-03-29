# Example IOC shell snippet for a direct-mapped motion velocity STruCpp logic
# library.
#
# Prerequisite: one axis must already be configured in the IOC before this
# script is loaded. The checked-in sample map is built with AXIS_INDEX=1, so it
# expects these ecmcDataItems:
#   %IL0   -> ax1.enc.actpos
#   %QL0   -> ax1.traj.targetvel
#   %QX8.0 -> ax1.drv.enable
#
# This example uses the preferred startup-linked mapping-file mode.
#
# To target another axis, rebuild the map with for example:
#   make MAPGEN_DEFINES='AXIS_INDEX=2' maps

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/motion_velocity_direct_logic.so")

require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},MEMORY_BYTES=16,REPORT=1"
