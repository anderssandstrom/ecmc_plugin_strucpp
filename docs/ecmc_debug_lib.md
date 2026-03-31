# ecmc_debug.st

Detailed reference for the bundled ST debug helper library:

- [`lib/ecmc_debug.st`](../lib/ecmc_debug.st)

## Purpose

`ecmc_debug.st` provides small bring-up helpers for printing debug messages
from ST logic without adding custom EPICS records or handwritten C++ code.

The library is included automatically by the shared IOC build helper unless
you disable it with:

```make
INCLUDE_DEBUG_ST := 0
```

## Runtime Behavior

Both helpers use the plugin's built-in debug path:

- messages go to IOC stdout/stderr as plugin-tagged log lines
- the latest short message is also published on the plugin PV
  `Plg-ST0-DbgTxtAct`
- output is disabled by default and only becomes active when
  `ctrl.word` bit 2 is set
- that means debug output can be enabled or disabled at runtime from the
  plugin control PV / panel, without editing ST code

## `ECMC_DebugPrint`

Type:

- `FUNCTION_BLOCK`

Inputs:

- `Execute : BOOL`
- `Message : STRING`

Outputs:

- `Fired : BOOL`

Behavior:

- prints only on the rising edge of `Execute`
- sets `Fired := TRUE` in the scan where the print happens
- keeps internal state to remember the previous `Execute` value

Use this when:

- you want one line per event
- the trigger condition may stay true for several cycles

Example:

```iecst
VAR
  dbgMoveDone : ECMC_DebugPrint;
END_VAR

dbgMoveDone(Execute := move_done,
            Message := CONCAT('pos=', TO_STRING(actual_position)));
```

## `ECMC_DebugPrintNow`

Type:

- `FUNCTION`

Inputs:

- `Message : STRING`

Return value:

- `BOOL`

Behavior:

- prints every time the function call is executed
- returns `TRUE` after calling the debug shim
- does not perform edge detection or rate limiting

Use this when:

- you explicitly control when the call runs
- you want unconditional one-shot prints in selected branches

Example:

```iecst
IF axis_error THEN
  ECMC_DebugPrintNow(Message := 'axis error active');
END_IF;
```

## Recommended Use

- Prefer `ECMC_DebugPrint` for cyclic logic, because it avoids repeated prints
  while a condition stays true.
- Use `ECMC_DebugPrintNow` for explicit branches or temporary bring-up code
  where unconditional execution is acceptable.
- Keep messages short if you want them to fit cleanly into the built-in debug
  text PV.

## Notes

- This library is for debugging and commissioning, not for high-rate data
  streaming.
- If you need persistent operator-facing state, prefer normal `@epics` exports.
