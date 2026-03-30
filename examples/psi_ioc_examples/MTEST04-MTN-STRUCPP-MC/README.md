# `MTEST04-MTN-STRUCPP-MC`

This PSI IOC example is a motion-library variant of the `MTEST04` example.
Instead of writing the EtherCAT velocity setpoint directly, it drives axis `1`
through the bundled PLCopen-style motion function blocks.

The source tree is:

- [`src/main.st`](src/main.st)
- [`src/Makefile`](src/Makefile)
- [`MTEST04-MTN-STRUCPP_startup.script`](MTEST04-MTN-STRUCPP_startup.script)
- [`MTEST04-MTN-STRUCPP_parameters.yaml`](MTEST04-MTN-STRUCPP_parameters.yaml)

It shows:

- `MC_Power`, `MC_MoveAbsolute`, `MC_ReadActualPosition`, and `MC_ReadStatus`
- automatic back-and-forth moves between positions `0` and `12800`
- fixed motion parameters:
  - velocity `1000`
  - acceleration `2000`
  - deceleration `2000`
- startup, periodic, and target-change debug printouts through
  `ECMC_DebugPrint`
- generated app PVs for actual position, target position, and grouped status

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
- `MTEST04-MTN-STRUCPP_strucpp.subs`

The generic plugin panel can be opened with:

```sh
caqtdm -macro "IOC=c6025a-04,PLG_ID=0" $(ecmc_plugin_strucpp_DIR)qt/ecmc_plugin_strucpp_main.ui
```
