# Example IOC shell snippet for an EL7041-0052 velocity-only STruCpp logic
# library.
#
# This example uses the startup-linked mapping-file mode:
#   %IW0 -> ec0.s14.positionActual01
#   %QW0 -> ec0.s14.driveControl01
#   %QW2 -> ec0.s14.velocitySetpoint01
#
# The mapping file is read directly by the plugin, so the item names inside it
# are literal. Point STRUCPP_MAPPING_FILE at the app-generated mapping file from
# ecmc_strucpp_app_example/build or another generated map for your application.

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/el7041_velocity_logic.so")
epicsEnvSet(STRUCPP_MAPPING_FILE,"/absolute/path/to/el7041_velocity.map")

${SCRIPTEXEC} ${ecmccfg_DIR}addSlave.cmd, "SLAVE_ID=14,HW_DESC=EL7041-0052"
require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},MAPPING_FILE=${STRUCPP_MAPPING_FILE},MEMORY_BYTES=16,REPORT=1"
