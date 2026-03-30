# ecmc_motion.st

Detailed reference for the bundled ST motion helper library:

- [`lib/ecmc_motion.st`](../lib/ecmc_motion.st)

## Purpose

`ecmc_motion.st` provides PLCopen-style motion FBs implemented on top of
`ecmc`.

This is not a formal PLCopen compliance claim. The library follows familiar
PLCopen naming and cyclic calling patterns, but the exact behavior is defined
by the `ecmc` implementation.

The source of truth is:

- [`lib/ecmc_motion.st`](../lib/ecmc_motion.st)

The bundled `.stlib` is built from that ST source.

## Axis Type

### `ECMC_AXIS_REF`

Type:

- `TYPE ... STRUCT`

Fields:

- `AxisIndex : DINT`

Purpose:

- minimal axis reference passed to all `MC_*` blocks

Example:

```iecst
VAR
  axis : ECMC_AXIS_REF;
END_VAR

axis.AxisIndex := 1;
```

## General Motion FB Notes

- all `MC_*` blocks are `FUNCTION_BLOCK`s
- they are intended to be called cyclically
- each FB instance keeps its own internal handle/state
- all blocks use `Axis : ECMC_AXIS_REF`

## Motion Function Blocks

### `MC_Power`

Inputs:

- `Axis`
- `Enable`

Outputs:

- `Status`
- `Valid`
- `Busy`
- `Error`
- `ErrorID`
- `Active`

Purpose:

- enable or disable an axis through the motion interface

### `MC_Reset`

Inputs:

- `Axis`
- `Execute`

Outputs:

- `Done`
- `Busy`
- `Error`
- `ErrorID`
- `Active`

Purpose:

- reset axis error state

### `MC_MoveAbsolute`

Inputs:

- `Axis`
- `Execute`
- `Position`
- `Velocity`
- `Acceleration`
- `Deceleration`

Outputs:

- `Done`
- `Busy`
- `Active`
- `CommandAborted`
- `Error`
- `ErrorID`

Purpose:

- command an absolute target position move

### `MC_MoveRelative`

Inputs:

- `Axis`
- `Execute`
- `Distance`
- `Velocity`
- `Acceleration`
- `Deceleration`

Outputs:

- `Done`
- `Busy`
- `Active`
- `CommandAborted`
- `Error`
- `ErrorID`

Purpose:

- command a relative move

### `MC_MoveVelocity`

Inputs:

- `Axis`
- `Execute`
- `Velocity`
- `Acceleration`
- `Deceleration`

Outputs:

- `InVelocity`
- `Busy`
- `Active`
- `CommandAborted`
- `Error`
- `ErrorID`

Purpose:

- command a continuous velocity move

### `MC_Home`

Inputs:

- `Axis`
- `Execute`
- `SeqId`
- `HomePosition`
- `VelocityTowardsCam`
- `VelocityOffCam`
- `Acceleration`
- `Deceleration`

Outputs:

- `Done`
- `Busy`
- `Active`
- `CommandAborted`
- `Error`
- `ErrorID`

Purpose:

- start a homing sequence using the configured `ecmc` homing path

### `MC_Halt`

Inputs:

- `Axis`
- `Execute`

Outputs:

- `Done`
- `Busy`
- `Active`
- `CommandAborted`
- `Error`
- `ErrorID`

Purpose:

- stop motion in a controlled way

### `MC_ReadStatus`

Inputs:

- `Axis`
- `Enable`

Outputs:

- `Valid`
- `Busy`
- `Error`
- `ErrorID`
- `ErrorStop`
- `Disabled`
- `Stopping`
- `Homing`
- `StandStill`
- `DiscreteMotion`
- `ContinuousMotion`
- `SynchronizedMotion`

Purpose:

- read high-level axis motion state

### `MC_ReadActualPosition`

Inputs:

- `Axis`
- `Enable`

Outputs:

- `Valid`
- `Busy`
- `Error`
- `ErrorID`
- `Position`

Purpose:

- read current actual position

### `MC_ReadActualVelocity`

Inputs:

- `Axis`
- `Enable`

Outputs:

- `Valid`
- `Busy`
- `Error`
- `ErrorID`
- `Velocity`

Purpose:

- read current actual velocity

## Example

```iecst
VAR
  axis    : ECMC_AXIS_REF;
  power   : MC_Power;
  moveAbs : MC_MoveAbsolute;
  readPos : MC_ReadActualPosition;
END_VAR

axis.AxisIndex := 1;

power(Axis := axis, Enable := TRUE);
moveAbs(Axis := axis,
        Execute := start_move,
        Position := 12800.0,
        Velocity := 1000.0,
        Acceleration := 2000.0,
        Deceleration := 2000.0);
readPos(Axis := axis, Enable := TRUE);
```

## Rebuilding The Bundled `.stlib`

If you change [`lib/ecmc_motion.st`](../lib/ecmc_motion.st), rebuild the
bundled motion library with:

```sh
./scripts/build_motion_stlib_container.sh
```
