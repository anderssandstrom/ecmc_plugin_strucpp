# Example IOC shell snippet for one STruCpp logic library.
#
# This is the explicit contiguous-image example. It intentionally uses a real
# memmap pair created by the existing EL6002 hardware script in ecmccfg:
#   %I* -> ec0.s${ECMC_EC_SLAVE_NUM}.mm.inputDataArray01
#   %Q* -> ec0.s${ECMC_EC_SLAVE_NUM}.mm.outputDataArray01
#
# Prefer the direct mapping examples for normal new projects. Use this path
# when one packed %I/%Q image is actually what you want.

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/machine_logic.so")

${SCRIPTEXEC} ${ecmccfg_DIR}addSlave.cmd, "SLAVE_ID=2, HW_DESC=EL6002"

require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},ASYN_PORT=PLUGIN.STRUCPP0,INPUT_ITEM=ec0.s${ECMC_EC_SLAVE_NUM}.mm.inputDataArray01,OUTPUT_ITEM=ec0.s${ECMC_EC_SLAVE_NUM}.mm.outputDataArray01,MEMORY_BYTES=64,REPORT=1"
