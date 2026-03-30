
#==============================================================================
# startup.cmd
#-------------- Information:
#- Description: ecmc_plugin_strucpp startup.cmd
#-
#- Arguments
#- PLUGIN_ID    : Plugin instantiation index, optional
#- LOGIC_LIB    : Path to the STruCpp logic library, default bin/main.so
#- ASYN_PORT    : Optional dedicated asyn port name, default PLUGIN.STRUCPP0
#- MAPPING_FILE : Optional startup-linked %I/%Q mapping manifest, defaults to
#-                ${LOGIC_LIB}.map when no explicit input/output image or
#-                binding mode is configured
#- INPUT_ITEM   : ecmcDataItem used as contiguous %I image
#- OUTPUT_ITEM  : ecmcDataItem used as contiguous %Q image
#- INPUT_BINDINGS  : Optional direct %I bindings, <offset>:<item>[@bytes],...
#- OUTPUT_BINDINGS : Optional direct %Q bindings, <offset>:<item>[@bytes],...
#- MEMORY_BYTES : Optional %M image size, default 256
#- SAMPLE_RATE_MS : Optional ST logic sample period in milliseconds
#- LOAD_DEFAULT_PVS : Load built-in plugin control/status PVs, default 1
#- LOAD_APP_PVS  : Load generated app PVs from substitutions, default 1
#- EPICS_SUBST  : Optional generated EPICS substitutions file to load
#- DB_PREFIX    : Optional record prefix for dbLoadTemplate, default ${IOC}
#- DB_MACROS    : Optional extra dbLoadTemplate macros, comma-separated
#- REPORT       : Print plugin report, default 1
#
#################################################################################

epicsEnvSet(ECMC_PLUGIN_FILENAME,"$(ecmc_plugin_strucpp_DIR)lib/${EPICS_HOST_ARCH=linux-x86_64}/libecmc_plugin_strucpp.so")
epicsEnvSet(ECMC_STRUCPP_LOGIC_LIB,"${LOGIC_LIB=bin/main.so}")
epicsEnvSet(ECMC_STRUCPP_DB_MACROS_BASE,"P=${DB_PREFIX=$(IOC=)},PORT=${ASYN_PORT=PLUGIN.STRUCPP0}")
epicsEnvSet(ECMC_STRUCPP_CORE_EPICS_SUBST,"$(ecmc_plugin_strucpp_DIR)db/ecmcStrucppCore.substitutions")
epicsEnvSet(ECMC_STRUCPP_DEFAULT_EPICS_SUBST,"${ECMC_STRUCPP_LOGIC_LIB}.substitutions")
epicsEnvSet(ECMC_STRUCPP_PLUGIN_CONFIG,"logic_lib=${ECMC_STRUCPP_LOGIC_LIB};asyn_port=${ASYN_PORT=PLUGIN.STRUCPP0};mapping_file=${MAPPING_FILE=};input_item=${INPUT_ITEM=};output_item=${OUTPUT_ITEM=};input_bindings=${INPUT_BINDINGS=};output_bindings=${OUTPUT_BINDINGS=};memory_bytes=${MEMORY_BYTES=256};sample_rate_ms=${SAMPLE_RATE_MS=}")

ecmcIf("'${PLUGIN_ID=-1}'='-1'")
${IF_TRUE}${SCRIPTEXEC} ${ecmccfg_DIR}loadPlugin.cmd, "FILE='${ECMC_PLUGIN_FILENAME}',CONFIG='${ECMC_STRUCPP_PLUGIN_CONFIG}',REPORT=${REPORT=1}"
#else
${IF_FALSE}${SCRIPTEXEC} ${ecmccfg_DIR}loadPlugin.cmd, "PLUGIN_ID=${PLUGIN_ID}, FILE='${ECMC_PLUGIN_FILENAME}', CONFIG='${ECMC_STRUCPP_PLUGIN_CONFIG}', REPORT=${REPORT=1}"
ecmcEndIf()

ecmcIf("'${LOAD_DEFAULT_PVS=1}'='0'",STRUCPP_CORE_SKIP_TRUE,STRUCPP_CORE_SKIP_FALSE)
# skip built-in control/status PVs
#else
  ${STRUCPP_CORE_SKIP_FALSE}ecmcIf("'${DB_MACROS=EMPTY}'='EMPTY'",STRUCPP_CORE_DB_EMPTY_TRUE,STRUCPP_CORE_DB_EMPTY_FALSE)
  ${STRUCPP_CORE_DB_EMPTY_TRUE}dbLoadTemplate("${ECMC_STRUCPP_CORE_EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE}")
  #else
  ${STRUCPP_CORE_DB_EMPTY_FALSE}dbLoadTemplate("${ECMC_STRUCPP_CORE_EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE},${DB_MACROS}")
  ecmcEndIf(STRUCPP_CORE_DB_EMPTY_TRUE,STRUCPP_CORE_DB_EMPTY_FALSE)
ecmcEndIf(STRUCPP_CORE_SKIP_TRUE,STRUCPP_CORE_SKIP_FALSE)

ecmcIf("'${LOAD_APP_PVS=1}'='0'",STRUCPP_APP_SKIP_TRUE,STRUCPP_APP_SKIP_FALSE)
# skip generated app PVs
#else
  ${STRUCPP_APP_SKIP_FALSE}ecmcIf("'${EPICS_SUBST=EMPTY}'='EMPTY'",STRUCPP_APP_SUBST_EMPTY_TRUE,STRUCPP_APP_SUBST_EMPTY_FALSE)
  ${STRUCPP_APP_SUBST_EMPTY_TRUE}ecmcIf("'${DB_PREFIX=EMPTY}'='EMPTY'",STRUCPP_DB_PREFIX_EMPTY_TRUE,STRUCPP_DB_PREFIX_EMPTY_FALSE)
    ${STRUCPP_DB_PREFIX_EMPTY_TRUE}ecmcIf("'${DB_MACROS=EMPTY}'='EMPTY'",STRUCPP_DB_MACROS_EMPTY_TRUE_1,STRUCPP_DB_MACROS_EMPTY_FALSE_1)
# no EPICS record file requested
    #else
    ${STRUCPP_DB_MACROS_EMPTY_FALSE_1}dbLoadTemplate("${ECMC_STRUCPP_DEFAULT_EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE},${DB_MACROS}")
    ecmcEndIf(STRUCPP_DB_MACROS_EMPTY_TRUE_1,STRUCPP_DB_MACROS_EMPTY_FALSE_1)
  #else
    ${STRUCPP_DB_PREFIX_EMPTY_FALSE}ecmcIf("'${DB_MACROS=EMPTY}'='EMPTY'",STRUCPP_DB_MACROS_EMPTY_TRUE_2,STRUCPP_DB_MACROS_EMPTY_FALSE_2)
    ${STRUCPP_DB_MACROS_EMPTY_TRUE_2}dbLoadTemplate("${ECMC_STRUCPP_DEFAULT_EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE}")
    #else
    ${STRUCPP_DB_MACROS_EMPTY_FALSE_2}dbLoadTemplate("${ECMC_STRUCPP_DEFAULT_EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE},${DB_MACROS}")
    ecmcEndIf(STRUCPP_DB_MACROS_EMPTY_TRUE_2,STRUCPP_DB_MACROS_EMPTY_FALSE_2)
  ecmcEndIf(STRUCPP_DB_PREFIX_EMPTY_TRUE,STRUCPP_DB_PREFIX_EMPTY_FALSE)
#else
  ${STRUCPP_APP_SUBST_EMPTY_FALSE}ecmcIf("'${DB_MACROS=EMPTY}'='EMPTY'",STRUCPP_DB_MACROS_EMPTY_TRUE_3,STRUCPP_DB_MACROS_EMPTY_FALSE_3)
  ${STRUCPP_DB_MACROS_EMPTY_TRUE_3}dbLoadTemplate("${EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE}")
  #else
  ${STRUCPP_DB_MACROS_EMPTY_FALSE_3}dbLoadTemplate("${EPICS_SUBST}", "${ECMC_STRUCPP_DB_MACROS_BASE},${DB_MACROS}")
  ecmcEndIf(STRUCPP_DB_MACROS_EMPTY_TRUE_3,STRUCPP_DB_MACROS_EMPTY_FALSE_3)
  ecmcEndIf(STRUCPP_APP_SUBST_EMPTY_TRUE,STRUCPP_APP_SUBST_EMPTY_FALSE)
ecmcEndIf(STRUCPP_APP_SKIP_TRUE,STRUCPP_APP_SKIP_FALSE)
