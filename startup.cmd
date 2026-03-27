
#==============================================================================
# startup.cmd
#-------------- Information:
#- Description: ecmc_plugin_strucpp startup.cmd
#-
#- Arguments
#- PLUGIN_ID    : Plugin instantiation index, optional
#- LOGIC_LIB    : Absolute path to the STruCpp logic library
#- ASYN_PORT    : Optional dedicated asyn port name, default PLUGIN.STRUCPP0
#- MAPPING_FILE : Optional startup-linked %I/%Q mapping manifest
#- INPUT_ITEM   : ecmcDataItem used as contiguous %I image
#- OUTPUT_ITEM  : ecmcDataItem used as contiguous %Q image
#- INPUT_BINDINGS  : Optional direct %I bindings, <offset>:<item>[@bytes],...
#- OUTPUT_BINDINGS : Optional direct %Q bindings, <offset>:<item>[@bytes],...
#- MEMORY_BYTES : Optional %M image size, default 256
#- REPORT       : Print plugin report, default 1
#
#################################################################################

epicsEnvSet(ECMC_PLUGIN_FILENAME,"$(ecmc_plugin_strucpp_DIR)lib/${EPICS_HOST_ARCH=linux-x86_64}/libecmc_plugin_strucpp.so")
epicsEnvSet(ECMC_STRUCPP_PLUGIN_CONFIG,"logic_lib=${LOGIC_LIB};asyn_port=${ASYN_PORT=PLUGIN.STRUCPP0};mapping_file=${MAPPING_FILE=};input_item=${INPUT_ITEM=};output_item=${OUTPUT_ITEM=};input_bindings=${INPUT_BINDINGS=};output_bindings=${OUTPUT_BINDINGS=};memory_bytes=${MEMORY_BYTES=256}")

ecmcIf("${PLUGIN_ID=-1}=-1")
${IF_TRUE}${SCRIPTEXEC} ${ecmccfg_DIR}loadPlugin.cmd, "FILE='${ECMC_PLUGIN_FILENAME}',CONFIG='${ECMC_STRUCPP_PLUGIN_CONFIG}',REPORT=${REPORT=1}"
#else
${IF_FALSE}${SCRIPTEXEC} ${ecmccfg_DIR}loadPlugin.cmd, "PLUGIN_ID=${PLUGIN_ID}, FILE='${ECMC_PLUGIN_FILENAME}', CONFIG='${ECMC_STRUCPP_PLUGIN_CONFIG}', REPORT=${REPORT=1}"
ecmcEndIf()
