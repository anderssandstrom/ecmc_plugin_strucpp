# Bundled ST Quick Guide

This file is a compact reference for the bundled ST helpers that ship with
`ecmc_plugin_strucpp`.

## `lib/ecmc_debug.st`

- `ECMC_DebugPrint`
  Edge-triggered debug print FB. Prints only on the rising edge of `Execute`.
- `ECMC_DebugPrintNow`
  Plain function version. Prints every time the call is executed.

## `lib/ecmc_control.st`

- `ECMC_PID`
  General PID FB with FF, output limiting, integrator limiting, anti-windup,
  and optional filtered D part.

## `lib/ecmc_utils.st`

- `ECMC_DebounceBool`
  Debounce a boolean with independent on/off delays.
- `ECMC_ApplyDeadband`
  Remove a symmetric deadband around a center value.
- `ECMC_Clamp`
  Clamp a value to a min/max range.
- `ECMC_InWindow`
  Check whether a value is inside a numeric window.
- `ECMC_RateLimiter`
  Limit how fast a value is allowed to rise or fall.
- `ECMC_FirstOrderFilter`
  Simple first-order low-pass filter.
- `ECMC_HysteresisBool`
  Convert an analog value to a stable boolean using low/high thresholds.
- `ECMC_Integrator`
  Standalone bounded integrator with enable/reset/hold.
- `ECMC_EcMasterStatus`
  Read generic EtherCAT master state.
- `ECMC_EcSlaveStatus`
  Read generic EtherCAT slave state.
- `ECMC_AxisGetTrajSource`
  Read whether an axis trajectory source is internal or external.
- `ECMC_AxisGetEncSource`
  Read whether an axis encoder source is internal or external.
- `ECMC_AxisGetActualPos`
  Read current actual axis position.
- `ECMC_AxisGetSetpointPos`
  Read current axis setpoint position.
- `ECMC_AxisGetActualVel`
  Read current actual axis velocity.
- `ECMC_AxisGetSetpointVel`
  Read current axis setpoint velocity.
- `ECMC_AxisIsEnabled`
  Read whether an axis is enabled.
- `ECMC_AxisIsBusy`
  Read whether an axis is busy.
- `ECMC_AxisHasError`
  Read whether an axis is in error.
- `ECMC_AxisGetErrorId`
  Read current axis error ID.
- `ECMC_AxisSetTrajSource`
  Explicitly set axis trajectory source by numeric value.
- `ECMC_AxisSetEncSource`
  Explicitly set axis encoder source by numeric value.
- `ECMC_AxisUseInternalTraj`
  Convenience wrapper to switch trajectory source to internal.
- `ECMC_AxisUseExternalTraj`
  Convenience wrapper to switch trajectory source to external.
- `ECMC_AxisUseInternalEnc`
  Convenience wrapper to switch encoder source to internal.
- `ECMC_AxisUseExternalEnc`
  Convenience wrapper to switch encoder source to external.
- `ECMC_AxisSetExternalSetpointPos`
  Write an external trajectory setpoint position.
- `ECMC_AxisSetExternalEncoderPos`
  Write an external encoder position.

Notes:

- There is intentionally no separate `ECMC_SlewToTarget`, since
  `ECMC_RateLimiter` already covers that use case.
- There is no dedicated pulse-stretch helper because the standard IEC timer
  blocks `TP` and `TOF` already cover that behavior well.

## `lib/ecmc_motion.st`

- `ECMC_AXIS_REF`
  Minimal axis reference type used by the bundled motion FBs.
- `MC_Power`
  Enable or disable an axis.
- `MC_Reset`
  Reset axis error state.
- `MC_MoveAbsolute`
  Command an absolute move.
- `MC_MoveRelative`
  Command a relative move.
- `MC_MoveVelocity`
  Command a velocity move.
- `MC_Home`
  Start a homing sequence.
- `MC_Halt`
  Stop motion in a controlled way.
- `MC_ReadStatus`
  Read high-level axis motion state.
- `MC_ReadActualPosition`
  Read current actual position.
- `MC_ReadActualVelocity`
  Read current actual velocity.
