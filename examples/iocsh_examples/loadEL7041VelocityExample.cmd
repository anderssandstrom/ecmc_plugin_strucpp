# Example IOC shell snippet for an EL7041-0052 velocity-only STruCpp logic
# library.
#
# This example uses the startup-linked mapping-file mode:
#   %IW0 -> ec.s14.positionActual01
#   %QW0 -> ec.s14.driveControl01
#   %QW2 -> ec.s14.velocitySetpoint01
#
# The default startup flow expects the generated mapping file next to the logic
# library as ${LOGIC_LIB}.map.

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/el7041_velocity_logic.so")

${SCRIPTEXEC} ${ecmccfg_DIR}addSlave.cmd, "SLAVE_ID=14,HW_DESC=EL7041-0052"
require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},MEMORY_BYTES=16,REPORT=1"
