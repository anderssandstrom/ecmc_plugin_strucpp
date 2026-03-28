# Example IOC shell snippet for the ST-based motion library sample.
#
# Prerequisite: axis 0 must already be configured in the IOC before this script
# is loaded, because the sample hard-wires ECMC_AXIS_REF.AxisIndex := 0.
#
# The sample uses the host plugin's contiguous-image mode rather than a mapping
# file. Provide one input buffer of at least 40 bytes and one output buffer of
# at least 16 bytes.
#
# Input layout:
#   %IX0.0 enableCmd
#   %IX0.1 executeCmd
#   %IL8   targetPos
#   %IL16  velocity
#   %IL24  accel
#   %IL32  decel
#
# Output layout:
#   %QX0.0 powerStatus
#   %QX0.1 powerValid
#   %QX0.2 moveBusy
#   %QX0.3 moveDone
#   %QX0.4 moveError
#   %QD4   moveErrorId
#   %QL8   actualPos

epicsEnvSet(STRUCPP_LOGIC_LIB,"/absolute/path/to/mc_power_move_absolute_lib_logic.so")
epicsEnvSet(STRUCPP_INPUT_ITEM,"<40-byte-input-item>")
epicsEnvSet(STRUCPP_OUTPUT_ITEM,"<16-byte-output-item>")

require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=${STRUCPP_LOGIC_LIB},INPUT_ITEM=${STRUCPP_INPUT_ITEM},OUTPUT_ITEM=${STRUCPP_OUTPUT_ITEM},MEMORY_BYTES=16,REPORT=1"
