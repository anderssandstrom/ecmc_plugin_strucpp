# ecmc_utils.st

Detailed reference for the bundled ST utility helper library:

- [`lib/ecmc_utils.st`](../lib/ecmc_utils.st)

## Purpose

`ecmc_utils.st` collects small reusable helpers that complement the normal IEC
library and the higher-level motion/control helpers.

The library is included automatically by the shared IOC build helper unless
you disable it with:

```make
INCLUDE_UTILS_ST := 0
```

The bundled utilities intentionally do not redefine standard IEC blocks like:

- `TON`
- `TOF`
- `TP`
- `R_TRIG`

## Signal Conditioning

### `ECMC_DebounceBool`

Type:

- `FUNCTION_BLOCK`

Purpose:

- debounce a boolean input with separate on/off delays

Inputs:

- `In : BOOL`
- `OnDelay : TIME := T#0ms`
- `OffDelay : TIME := T#0ms`

Outputs:

- `Out : BOOL`
- `Rising : BOOL`
- `Falling : BOOL`

### `ECMC_ApplyDeadband`

Type:

- `FUNCTION`

Purpose:

- collapse a symmetric region around `Center` to `Center`
- preserve the outer slope beyond the deadband

Inputs:

- `Value : LREAL`
- `Width : LREAL := 0.0`
- `Center : LREAL := 0.0`

Returns:

- adjusted `LREAL`

### `ECMC_Clamp`

Type:

- `FUNCTION`

Purpose:

- clamp a value into `[Min, Max]`
- if bounds are reversed, the function swaps them internally

Inputs:

- `Value : LREAL`
- `Min : LREAL := 0.0`
- `Max : LREAL := 0.0`

Returns:

- clamped `LREAL`

### `ECMC_InWindow`

Type:

- `FUNCTION`

Purpose:

- test whether a value is inside a numeric window

Inputs:

- `Value : LREAL`
- `Low : LREAL`
- `High : LREAL`
- `Inclusive : BOOL := TRUE`

Returns:

- `BOOL`

### `ECMC_GetCycleTimeS`

Type:

- `FUNCTION`

Purpose:

- read the configured EtherCAT / RT base cycle time in seconds
- useful as `DT` for `ECMC_PID`, `ECMC_RateLimiter`, `ECMC_FirstOrderFilter`,
  and `ECMC_Integrator`

Returns:

- `LREAL`

### `ECMC_EpicsStarted`

Type:

- `FUNCTION`

Purpose:

- report whether the IOC has finished EPICS startup
- mirrors the `ecmc` startup state used by `epics_get_started()`
- can be used to hold back startup logic until the IOC is fully running
- remains useful when the plugin is configured with
  `run_before_epics_started=1`

Returns:

- `BOOL`

Example:

```iecst
IF ECMC_EpicsStarted() THEN
  startupReady(Enable := axis_ready_raw,
               Delay := T#500ms);
END_IF;
```

Note:

- by default the plugin already skips ST execution until EPICS startup is
  complete
- use this helper when you intentionally enable early execution and still want
  startup-aware behavior inside ST

### `ECMC_StartupDelay`

Type:

- `FUNCTION_BLOCK`

Purpose:

- delay a startup-ready signal until it has stayed true for a configured time
- useful to avoid acting on transient startup states

Inputs:

- `Enable : BOOL := TRUE`
- `Delay : TIME := T#500ms`

Outputs:

- `Ready : BOOL`
- `Rising : BOOL`

Example:

```iecst
VAR
  startupReady : ECMC_StartupDelay;
  startTrig    : R_TRIG;
END_VAR

startupReady(Enable := axis_ready_raw,
             Delay := T#500ms);
startTrig(CLK := startupReady.Ready);
```

### `ECMC_RateLimiter`

Type:

- `FUNCTION_BLOCK`

Purpose:

- limit how fast an output can move toward an input

Inputs:

- `Enable : BOOL := TRUE`
- `Reset : BOOL := FALSE`
- `Input : LREAL`
- `RisingRate : LREAL := 0.0`
- `FallingRate : LREAL := 0.0`
- `DT : LREAL := 1.0`
- `InitToInput : BOOL := TRUE`

Outputs:

- `Output : LREAL`
- `Limited : BOOL`

Notes:

- this already covers the normal “slew to target” use case

### `ECMC_FirstOrderFilter`

Type:

- `FUNCTION_BLOCK`

Purpose:

- simple first-order low-pass filter

Inputs:

- `Enable : BOOL := TRUE`
- `Reset : BOOL := FALSE`
- `Input : LREAL`
- `Tau : LREAL := 0.0`
- `DT : LREAL := 1.0`
- `InitToInput : BOOL := TRUE`

Outputs:

- `Output : LREAL`

### `ECMC_HysteresisBool`

Type:

- `FUNCTION_BLOCK`

Purpose:

- convert an analog input into a stable boolean using separate low/high
  thresholds

Inputs:

- `In : LREAL`
- `Low : LREAL := 0.0`
- `High : LREAL := 0.0`
- `Reset : BOOL := FALSE`
- `InitState : BOOL := FALSE`

Outputs:

- `Out : BOOL`
- `Rising : BOOL`
- `Falling : BOOL`

### `ECMC_Integrator`

Type:

- `FUNCTION_BLOCK`

Purpose:

- standalone bounded integrator

Inputs:

- `Enable : BOOL := TRUE`
- `Reset : BOOL := FALSE`
- `Hold : BOOL := FALSE`
- `In : LREAL`
- `K : LREAL := 1.0`
- `DT : LREAL := 1.0`
- `Min : LREAL := 0.0`
- `Max : LREAL := 0.0`
- `Init : LREAL := 0.0`

Outputs:

- `Out : LREAL`
- `Limited : BOOL`

Notes:

- integration only advances when `Enable` is true, `Hold` is false, and
  `DT > 0`

## EtherCAT Status

### `ECMC_EcMasterStatus`

Type:

- `FUNCTION_BLOCK`

Purpose:

- read generic EtherCAT master state from `ecmc`

Inputs:

- `MasterId : INT := -1`

Outputs:

- `Valid`
- `LinkUp`
- `Init`
- `PreOp`
- `SafeOp`
- `Op`
- `StateCode`
- `SlavesResponding`
- `StateWord`

### `ECMC_EcSlaveStatus`

Type:

- `FUNCTION_BLOCK`

Purpose:

- read generic EtherCAT slave state from `ecmc`

Inputs:

- `SlaveId : INT`
- `MasterId : INT := -1`

Outputs:

- `Valid`
- `Online`
- `Operational`
- `Init`
- `PreOp`
- `SafeOp`
- `Op`
- `StateCode`
- `StateWord`

## Axis Status Getters

These helpers read common axis state without requiring explicit `@ecmc`
mapping for every status point.

### Source state

- `ECMC_AxisGetTrajSource`
- `ECMC_AxisGetEncSource`

Both return a `DINT` source value. Common values:

- `0` internal
- `1` external

### Position and velocity

- `ECMC_AxisGetActualPos`
- `ECMC_AxisGetSetpointPos`
- `ECMC_AxisGetActualVel`
- `ECMC_AxisGetSetpointVel`

All return `LREAL`.

### Boolean state

- `ECMC_AxisIsEnabled`
- `ECMC_AxisIsBusy`
- `ECMC_AxisHasError`

All return `BOOL`.

### Error code

- `ECMC_AxisGetErrorId`

Returns:

- `DINT`

## Axis Source / External Data Setters

These helpers are intended mainly for synchronization and kinematics paths
where ST owns the external setpoint or encoder values of an axis.

### Explicit source setters

- `ECMC_AxisSetTrajSource(AxisId, Source)`
- `ECMC_AxisSetEncSource(AxisId, Source)`

Both return:

- `DINT` error/status code from `ecmc`

### Convenience source setters

- `ECMC_AxisUseInternalTraj`
- `ECMC_AxisUseExternalTraj`
- `ECMC_AxisUseInternalEnc`
- `ECMC_AxisUseExternalEnc`

These wrap the numeric source setters for the common internal/external cases.

### External position setters

- `ECMC_AxisSetExternalSetpointPos`
- `ECMC_AxisSetExternalEncoderPos`

Both take:

- `AxisId : DINT`
- `Position : LREAL`

Both return:

- `DINT` error/status code from `ecmc`

## Example

```iecst
VAR
  dt      : LREAL;
  filt    : ECMC_FirstOrderFilter;
  hyst    : ECMC_HysteresisBool;
  integ   : ECMC_Integrator;
  statusM : ECMC_EcMasterStatus;
  statusS : ECMC_EcSlaveStatus;
  axisPos : LREAL;
  err     : DINT;
END_VAR

dt := ECMC_GetCycleTimeS();
filt(Input := velocity_cmd, Tau := 0.02, DT := dt);
hyst(In := axis_load, Low := 20.0, High := 30.0);
integ(In := ctrl_err, K := 0.5, DT := dt, Min := -10.0, Max := 10.0);

statusM();
statusS(SlaveId := 14);

axisPos := ECMC_AxisGetActualPos(AxisId := 1);
err := ECMC_AxisUseExternalTraj(AxisId := 1);
err := ECMC_AxisSetExternalSetpointPos(AxisId := 1, Position := kin_pos_cmd);
```
