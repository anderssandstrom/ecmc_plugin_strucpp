# ecmc_control.st

Detailed reference for the bundled ST control helper library:

- [`lib/ecmc_control.st`](../lib/ecmc_control.st)

## Purpose

`ecmc_control.st` currently provides one reusable closed-loop helper:

- `ECMC_PID`

The library is included automatically by the shared IOC build helper unless
you disable it with:

```make
INCLUDE_CONTROL_ST := 0
```

## `ECMC_PID`

Type:

- `FUNCTION_BLOCK`

Purpose:

- general PID controller with feed-forward and built-in limiting

### Inputs

- `Enable : BOOL := TRUE`
- `Reset : BOOL := FALSE`
- `Setpoint : LREAL`
- `Actual : LREAL`
- `FF : LREAL := 0.0`
- `Kp : LREAL := 1.0`
- `Ki : LREAL := 0.0`
- `Kd : LREAL := 0.0`
- `Kff : LREAL := 1.0`
- `DT : LREAL := 1.0`
- `DFilterTau : LREAL := 0.0`
- `OutMin : LREAL := 0.0`
- `OutMax : LREAL := 0.0`
- `IMin : LREAL := 0.0`
- `IMax : LREAL := 0.0`

### Outputs

- `Error : LREAL`
- `Output : LREAL`
- `PPart : LREAL`
- `IPart : LREAL`
- `DPart : LREAL`
- `FFPart : LREAL`
- `Limited : BOOL`

## Behavior

The controller computes:

- `Error = Setpoint - Actual`
- `PPart = Error * Kp`
- `FFPart = FF * Kff`
- `IPart` by discrete integration of error
- `DPart` from discrete error derivative

### Output limiting

`Output` is limited only when:

- `OutMax > OutMin`

If that condition is not true, output limiting is disabled.

### Integrator limiting

`IPart` is limited only when:

- `IMax > IMin`

If that condition is not true, integrator limiting is disabled.

### Anti-windup

When the unsaturated output exceeds the output limits, the block only accepts
new integral growth when the current error would drive the output back toward
the allowed range.

### Derivative filtering

If:

- `DFilterTau > 0.0`

the raw derivative term is filtered with a first-order filter.

If:

- `DFilterTau <= 0.0`

the derivative term is unfiltered.

### Enable and reset behavior

- `Reset = TRUE` clears internal dynamic state
- `Enable = FALSE` forces `Output := 0.0` and resets derivative state
- on the first enabled cycle, derivative kick is avoided and internal state is
  reinitialized cleanly

## Recommended Use

Typical minimal call:

```iecst
VAR
  pid : ECMC_PID;
END_VAR

pid(Setpoint := 12800.0,
    Actual := actual_pos,
    Kp := 1.0,
    Ki := 0.01,
    DT := 0.001);
```

Typical bounded call with feed-forward:

```iecst
pid(Setpoint := pos_cmd,
    Actual := pos_act,
    FF := vel_ff,
    Kp := 1.0,
    Ki := 0.02,
    Kd := 0.001,
    Kff := 1.0,
    DT := 0.001,
    DFilterTau := 0.01,
    OutMin := -2000.0,
    OutMax := 2000.0,
    IMin := -500.0,
    IMax := 500.0);
```

## Tuning Notes

- Start with `Kp` only.
- Add a small `Ki` after proportional response looks reasonable.
- Use `Kd` only when needed, and prefer a nonzero `DFilterTau`.
- Set `DT` to your actual ST execution period in seconds.
- If using output limiting, consider also setting `IMin` / `IMax`.

## Notes

- The block is intentionally simple and cyclic. It does not implement
  automatic setpoint ramping or multi-rate scheduling.
- For simple clamping, filtering, or discrete integration outside a PID loop,
  see [`docs/ecmc_utils_lib.md`](ecmc_utils_lib.md).
