# `ioc_project_mc_motion`

This IOC-project example shows how to use the bundled `MC_*` motion library
from ST and trigger motion directly from EPICS-exported variables.

The source tree is:

- [`src/main.st`](src/main.st)
- [`src/Makefile`](src/Makefile)

Key points:

- no `%I/%Q` image is used for the motion commands
- the ST code talks to `ecmc` motion through `MC_Power`, `MC_MoveAbsolute`,
  `MC_ReadActualPosition`, and `MC_ReadStatus`
- control and status are exposed to EPICS with `// @epics ...`
- the sample hard-wires `axis.AxisIndex := 1`, so axis `1` must already exist
  in the IOC before the plugin is loaded

The startup flow is:

1. `make`
2. `ioc install --clean -V --ioc MC-MOTION-STRUCPP-IOC`
3. configure axis `1`
4. load [`MC-MOTION-STRUCPP-IOC_startup.script`](MC-MOTION-STRUCPP-IOC_startup.script)

The exported variables let EPICS drive the sample.

- `Main-Cmd` as one packed `mbboDirect` command word
- `TargetPos`
- `Velocity`
- `Acceleration`
- `Deceleration`

and monitor:

- `Main-Stat` as one packed `mbbiDirect` status word
- `MoveErrorId`
- `ActualPos`

Because these are normal `@epics` exports, the generated records come from
`bin/main.so.substitutions` with the normal startup defaults.
