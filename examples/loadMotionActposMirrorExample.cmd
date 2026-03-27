# Example IOC shell snippet for a motion-data STruCpp logic library.
#
# Prerequisite: axis 1 must already be configured in the IOC before this script
# is loaded, so that ax1.enc.actpos and ax1.traj.targetpos exist as ecmcDataItems.
#
# This example uses the startup-linked mapping-file mode:
#   %IL0 -> ax1.enc.actpos
#   %QL0 -> ax1.traj.targetpos

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/motion_actpos_mirror_logic.so")
epicsEnvSet(STRUCPP_MAPPING_FILE,"/absolute/path/to/motion_actpos_mirror.map")

${SCRIPTEXEC} $(ecmc_plugin_strucpp_DIR)startup.cmd, "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},MAPPING_FILE=${STRUCPP_MAPPING_FILE},MEMORY_BYTES=16,REPORT=1"
