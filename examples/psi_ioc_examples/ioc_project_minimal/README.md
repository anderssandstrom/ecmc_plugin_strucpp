# `ioc_project_minimal`

This is the smallest practical IOC-project example for `ecmc_plugin_strucpp`.

It is meant to show the default workflow only:

- one ST source file
- one short `src/Makefile`
- no handwritten C++ files
- startup only calls `require`
- direct EtherCAT item mapping, not memmaps

The source tree is:

- [`src/main.st`](src/main.st)
- [`src/Makefile`](src/Makefile)

This minimal example also shows both annotation-placeholder styles:

- inline default in ST:
  - `ec.s${SLAVE_ID=14}...`
- required build-time define from `src/Makefile`:
  - `ANNOTATION_DEFINES := CH_ID=01`

Those placeholders are expanded across the bundled ST source itself before
`strucpp` runs, not only inside generated mapping metadata.

It also shows both EPICS record-name override forms directly in
[`src/main.st`](src/main.st):

- `rec=Main-CycleCounterAct`
  keeps the normal `P` prefix and overrides only the suffix
- `prefix=$(IOC): rec=Main-PosActMini`
  uses a custom record prefix for that one record

Here `$(IOC)` is expected to come from the IOC environment or the
`gfa-iocutils` startup naming, not from the ST build step.

The expected flow is:

1. `make`
2. `ioc install --clean -V --ioc MINIMAL-STRUCPP-IOC`
3. IOC startup loads:
   - `bin/main.so`
   - `bin/main.so.map`
   - `bin/main.so.substitutions`
   - `MINIMAL-STRUCPP-IOC_strucpp.subs`

Because the helper stages the map and substitutions next to the logic library,
the startup script can stay minimal:

```iocsh
require ecmccfg
${SCRIPTEXEC} ${ecmccfg_DIR}addSlave.cmd, "SLAVE_ID=14,HW_DESC=EL7041-0052"
require ecmc_plugin_strucpp sandst_a "REPORT=1"
# caqtdm -macro "IOC=c6025a-04,PLG_ID=0" $(ecmc_plugin_strucpp_DIR)qt/ecmc_plugin_strucpp_main.ui
```

This example uses `// @ecmc ...` annotations in the ST source, so the plugin
defaults to `bin/main.so.map` and does not need `INPUT_ITEM`,
`OUTPUT_ITEM`, or `MAPPING_FILE` macros in the normal case.

The concrete sample maps directly to EL7041 PDO items:

- `%IW0` -> `ec.s14.positionActual01`
- `%QW0` -> `ec.s14.driveControl01`
- `%QW2` -> `ec.s14.velocitySetpoint01`

So in this example:

- `SLAVE_ID` defaults to `14` inside the ST annotations
- `CH_ID` comes from `ANNOTATION_DEFINES`
- the simple logic drives the axis back and forth between positions `0` and
  `1000`

If needed, you can override both at build time, for example:

```sh
make ANNOTATION_DEFINES="SLAVE_ID=18 CH_ID=01"
```

So this example is deliberately showing the direct EtherCAT-data path rather
than the older contiguous memmap path.
