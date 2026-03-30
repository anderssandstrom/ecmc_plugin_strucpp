# `MTEST04-MTN-STRUCPP-PID`

This PSI IOC example is a small direct-mapped motion test for
`ecmc_plugin_strucpp` using the bundled `ECMC_PID` function block.

The source tree is:

- [`src/main.st`](src/main.st)
- [`src/Makefile`](src/Makefile)
- [`MTEST04-MTN-STRUCPP_startup.script`](MTEST04-MTN-STRUCPP_startup.script)
- [`MTEST04-MTN-STRUCPP_parameters.yaml`](MTEST04-MTN-STRUCPP_parameters.yaml)

It shows:

- direct `@ecmc` mapping to EtherCAT items
- use of the bundled `ECMC_PID` helper
- `@epics` export of selected ST variables
- bundled `ECMC_DebugPrint` usage from ST
- the standard IOC build helper flow with generated `bin/main.so*`
- generation of the dedicated IOC substitutions file:
  `MTEST04-MTN-STRUCPP_strucpp.subs`

The EtherCAT mappings in [`src/main.st`](src/main.st) are:

- `%IW0` -> `ec.s14.positionActual01`
- `%QW0` -> `ec.s14.driveControl01`
- `%QW2` -> `ec.s14.velocitySetpoint01`

The current ST logic does this:

- enables the drive with `drive_control := 16#0001`
- flips the target position between `0` and `12800`
- uses `ECMC_PID` to generate a velocity command
- limits the velocity command to `-1000 .. +1000`
- limits the I part to `-500 .. +500`

It also exports:

- actual position
- active target position
- current velocity command
- PID error
- PID limited status

It prints to the IOC shell:

- once at startup:
  `ST PID program started`
- periodically every 1000 cycles:
  `actual_position=...`
- when the target changes:
  `PID target -> 12800`
  `PID target -> 0`

The expected flow is:

1. `make`
2. `ioc install --clean -V --ioc MTEST04-MTN-STRUCPP`
3. start the IOC with
   [`MTEST04-MTN-STRUCPP_startup.script`](MTEST04-MTN-STRUCPP_startup.script)

The build produces and stages:

- `bin/main.so`
- `bin/main.so.map`
- `bin/main.so.substitutions`
- `bin/main.so.summary.txt`

and the project-root `Makefile` also regenerates:

- `MTEST04-MTN-STRUCPP_strucpp.subs`

The generic plugin panel can be opened with:

```sh
caqtdm -macro "IOC=c6025a-04,PLG_ID=0" $(ecmc_plugin_strucpp_DIR)qt/ecmc_plugin_strucpp_main.ui
```
