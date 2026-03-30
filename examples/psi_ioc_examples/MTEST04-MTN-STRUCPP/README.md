# `MTEST04-MTN-STRUCPP`

This PSI IOC example is a small direct-mapped motion test for
`ecmc_plugin_strucpp`.

The source tree is:

- [`src/main.st`](src/main.st)
- [`src/Makefile`](src/Makefile)
- [`MTEST04-MTN-STRUCPP_startup.script`](MTEST04-MTN-STRUCPP_startup.script)
- [`MTEST04-MTN-STRUCPP_parameters.yaml`](MTEST04-MTN-STRUCPP_parameters.yaml)

It shows:

- direct `@ecmc` mapping to EtherCAT items
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
- exports the current position to EPICS as `Main-PosActMini`
- seeds `velocity_setpoint` to `+1000` if it starts at `0`
- drives between positions `0` and `12800`
- flips the velocity sign at those two limits

It also prints to the IOC shell:

- once at startup:
  `ST program started`
- periodically every 1000 cycles:
  `actual_position=...`
- on reversals:
  `reverse -> +1000 at pos=...`
  `reverse -> -1000 at pos=...`

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
