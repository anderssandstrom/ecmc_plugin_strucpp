# `ecmc_plugin_strucpp`

`ecmc_plugin_strucpp` is a generic `ecmc` host plugin for running
loadable `STruCpp` logic libraries inside the normal `ecmc` realtime loop.

The split is:

- `ecmc_plugin_strucpp`
  builds the generic host plugin loaded by `ecmc`
- your ST application
  builds a separate shared library with generated `STruCpp` code and a
  small wrapper that exposes a fixed C ABI

The host plugin uses the standard `ecmc` plugin interface. It does not add a
new plugin system. At runtime it:

1. links the current ST program's `%I/%Q/%M` addresses to configured `ecmc`
   buffers at realtime entry
2. runs one logic cycle in the normal `ecmc` loop
3. executes a precompiled pointer copy plan for the configured binding mode

The copy work is compiled once at realtime entry into a direct pointer-based
copy plan. The realtime loop only executes that plan.

## Current Scope

This host plugin is intentionally generic, but it is currently a singleton,
like `ecmc_plugin_daq`. Load it once and point it at one logic library.

If you later need several independent ST logic modules in one IOC, the next
step is to add an object-registration layer on top of this host. The installed
ABI and wrapper headers already support that extension.

## Bundled Motion Library

This repo now ships a reusable ST motion library:

- [`lib/ecmc_motion.st`](lib/ecmc_motion.st)
- [`libs/ecmc-motion.stlib`](libs/ecmc-motion.stlib)

Application repos can consume it directly with `-L $(ECMC_PLUGIN_STRUCPP)/libs`
instead of rebuilding a private copy of the motion library.

The first useful block set currently includes:

- `MC_Power`
- `MC_Reset`
- `MC_MoveAbsolute`
- `MC_MoveRelative`
- `MC_MoveVelocity`
- `MC_Home`
- `MC_Halt`
- `MC_ReadStatus`
- `MC_ReadActualPosition`
- `MC_ReadActualVelocity`

If the ST source changes, rebuild the bundled `.stlib` with:

```sh
./scripts/build_motion_stlib_container.sh
```

That recompiles the library with `strucpp` in a container and reapplies the
required `ecmcMcApi.h` header metadata.

## Host Config

The host expects the normal `Cfg.LoadPlugin(...)` config string format:

```text
logic_lib=<path>;asyn_port=<plugin asyn port>;[mapping_file=<path>|input_item=<ecmc data item>|input_bindings=<offset:item[@bytes],...>];[output_item=<ecmc data item>|output_bindings=<offset:item[@bytes],...>];memory_bytes=<n>
```

Startup-linked mapping example:

```text
logic_lib=/abs/path/to/el7041_velocity_logic.so;asyn_port=PLUGIN.STRUCPP0;mapping_file=/abs/path/to/el7041_velocity.map;memory_bytes=16
```

Contiguous image example:

```text
logic_lib=/abs/path/to/machine_logic.so;input_item=ec0.s2.mm.inputDataArray01;output_item=ec0.s2.mm.outputDataArray01;memory_bytes=64
```

Direct scalar binding example:

```text
logic_lib=/abs/path/to/el7041_velocity_logic.so;input_bindings=0:ec0.s14.positionActual01@2;output_bindings=0:ec0.s14.driveControl01@2,2:ec0.s14.velocitySetpoint01@2;memory_bytes=16
```

- `logic_lib`
  absolute path to the loadable logic library
- `mapping_file`
  path to a startup-linked manifest that maps exact STruCpp addresses like
  `%IW0` or `%QW2` to literal `ecmcDataItem` names
- `asyn_port`
  dedicated asyn port owned by `ecmc_plugin_strucpp`, defaults to
  `PLUGIN.STRUCPP0`
- `input_item`
  `ecmcDataItem` used as one contiguous `%I*` byte image
- `output_item`
  `ecmcDataItem` used as one contiguous `%Q*` byte image
- `input_bindings`
  direct `%I*` bindings in the form `<offset>:<item>[@bytes],...`
- `output_bindings`
  direct `%Q*` bindings in the form `<offset>:<item>[@bytes],...`
- `memory_bytes`
  optional `%M*` backing store size, defaults to `256`

Use `mapping_file` when you want the plugin to inspect the current ST code at
startup, verify every used `%I/%Q` address, and link it directly to final
`ecmcDataItem` buffers once before the RT loop starts. Use `input_item` /
`output_item` when your logic naturally maps to one contiguous byte image, for
example an existing memmap. Use `input_bindings` / `output_bindings` when you
want explicit offset-to-item control without an external manifest. The EL6002
example in this repo uses contiguous images. The EL7041 example uses
`mapping_file`.

In `mapping_file` mode the host now checks, at startup:

- `%I` vs `%Q` direction against `ecmcDataItemInfo.dataDirection`
- exact byte width for direct scalar mappings
- compatible `ecmcEcDataType` family for the located width

The current logic ABI still only exposes located width, not the full IEC scalar
type, so the host can validate "64-bit compatible" but not distinguish `LREAL`
from `LWORD` when both use `%IL`.

The mapping-file format is intentionally small:

```text
# comments are allowed
%IW0=ec0.s14.positionActual01
%QW0=ec0.s14.driveControl01
%QW2=ec0.s14.velocitySetpoint01
```

The names in that file are literal `ecmcDataItem` names. The plugin reads the
file directly, so IOC macro expansion does not happen inside the manifest.

To avoid maintaining `%IW0` / `%QW2` addresses by hand, this repo also ships a
small generator in [`scripts/strucpp_mapgen.py`](scripts/strucpp_mapgen.py).
It reads the generated `STruCpp` header forwards and, preferably, inline ST
annotations of the form:

```text
actual_position    AT %IW0 : INT;   // @ecmc ec0.s14.positionActual01
drive_control      AT %QW0 : WORD;  // @ecmc ec0.s14.driveControl01
velocity_setpoint  AT %QW2 : INT;   // @ecmc ec0.s14.velocitySetpoint01
```

and emits the final startup-linked mapping file automatically. It also still
accepts an external `VAR_NAME=ecmcDataItem` bindings file when you do not want
to keep the metadata in the ST source.

## PLC Functions

The plugin also exports numeric-only exprtk PLC helper functions through the
normal `ecmcPluginData.funcs[]` interface. These helpers operate directly on
the plugin's linked `%I`, `%Q`, and `%M` byte images.

Area selector values:

- `0` = `%I`
- `1` = `%Q`
- `2` = `%M`

Available functions:

- `strucpp_get_bit(area, byte_offset, bit_index)`
- `strucpp_set_bit(area, byte_offset, bit_index, value)`
- `strucpp_get_u8(area, byte_offset)`
- `strucpp_set_u8(area, byte_offset, value)`
- `strucpp_get_s8(area, byte_offset)`
- `strucpp_set_s8(area, byte_offset, value)`
- `strucpp_get_u16(area, byte_offset)`
- `strucpp_set_u16(area, byte_offset, value)`
- `strucpp_get_s16(area, byte_offset)`
- `strucpp_set_s16(area, byte_offset, value)`
- `strucpp_get_u32(area, byte_offset)`
- `strucpp_set_u32(area, byte_offset, value)`
- `strucpp_get_s32(area, byte_offset)`
- `strucpp_set_s32(area, byte_offset, value)`
- `strucpp_get_f32(area, byte_offset)`
- `strucpp_set_f32(area, byte_offset, value)`
- `strucpp_get_f64(area, byte_offset)`
- `strucpp_set_f64(area, byte_offset, value)`

Example:

```text
var status;
status := strucpp_get_u16(0, 0);   // read %IW0
strucpp_set_u16(1, 0, 16);         // write %QW0
strucpp_set_bit(2, 4, 0, 1);       // set %MX4.0
```

Out-of-range access returns `NaN`.

## ST Variable Exports

The host can also publish selected non-located ST variables as normal EPICS
asyn parameters on the plugin's own dedicated asyn port. The intent is that
the declaration lives in the ST source, not in a second C++ config list.

Annotation shape:

```text
counter       : INT;    // @epics plugin.strucpp.machine.counter
manual_target : INT;    // @epics plugin.strucpp.machine.manual_target rw
```

Rules:

- the annotation lives on the ST variable declaration line
- the first token after `@epics` is the exported asyn parameter name
- optional final token `rw` makes the parameter writable from EPICS
- default is read-only
- current support is for top-level scalar program variables:
  `BOOL`, `SINT`, `USINT`, `BYTE`, `INT`, `UINT`, `WORD`, `DINT`, `UDINT`,
  `DWORD`, `REAL`, `LREAL`

The application repo runs
[`scripts/strucpp_epics_exportgen.py`](scripts/strucpp_epics_exportgen.py) to
turn those annotations into a small generated export header. The logic library
then exposes that export table through the logic ABI, and the host creates
matching asyn parameters at startup on the configured `asyn_port`.

This repo also ships macro-based EPICS database templates in [`db`](db) and a
generator,
[`scripts/strucpp_epics_substgen.py`](scripts/strucpp_epics_substgen.py),
that turns the same `// @epics ...` annotations into a `.substitutions` file.

That lets an application repo keep the ST source as the single source of truth
for both:

- exported plugin-owned asyn parameters
- EPICS records connected to those parameters

Typical generated output is loaded like:

```iocsh
dbLoadTemplate("/absolute/path/to/machine_logic.so.substitutions",
               "P=IOC:,PORT=PLUGIN.STRUCPP0")
```

The generated substitutions reference these generic templates:

- [`db/ecmcStrucppBi.template`](db/ecmcStrucppBi.template)
- [`db/ecmcStrucppBo.template`](db/ecmcStrucppBo.template)
- [`db/ecmcStrucppLongIn.template`](db/ecmcStrucppLongIn.template)
- [`db/ecmcStrucppLongOut.template`](db/ecmcStrucppLongOut.template)
- [`db/ecmcStrucppAi.template`](db/ecmcStrucppAi.template)
- [`db/ecmcStrucppAo.template`](db/ecmcStrucppAo.template)

## Installed Public Headers

The plugin installs these public headers from [`src`](src):

- [`ecmcStrucppLogicIface.hpp`](src/ecmcStrucppLogicIface.hpp)
- [`ecmcStrucppLogicWrapper.hpp`](src/ecmcStrucppLogicWrapper.hpp)
- [`ecmcStrucppMcWrapper.hpp`](src/ecmcStrucppMcWrapper.hpp)

Those are the only pieces an application-side logic library needs from this
repo.

`ecmcStrucppMcWrapper.hpp` is the first plugin-side bridge for the new
PLCopen-style runtime now being added to `ecmc`. It exposes simple C++ wrapper
objects like `ecmcStrucpp::mc::MC_Power`, `MC_MoveAbsolute`, and
`MC_ReadStatus`, while the actual motion semantics remain in `ecmc` through
`ecmcMcApi.h`.

Example shape:

```cpp
#include "ecmcStrucppMcWrapper.hpp"

ecmcStrucpp::mc::AxisRef axis1{0};
ecmcStrucpp::mc::MC_Power power;
ecmcStrucpp::mc::MC_MoveVelocity move_vel;

power.run(axis1, true);
move_vel.run(axis1, true, 1000.0, 10000.0, 10000.0);
```

## Starter Templates

For a new motion app, start from:

- [`templates/mc_move_absolute_template.st`](templates/mc_move_absolute_template.st)
- [`templates/motion_logic_wrapper_template.cpp`](templates/motion_logic_wrapper_template.cpp)

That gives you the minimum ST program plus logic wrapper pattern. In most cases
you only need to:

1. set `axis.AxisIndex`
2. adjust the `%I/%Q` located layout
3. rename the program and wrapper identifiers

If a generated `generated/<program>_epics_exports.hpp` file exists, the wrapper
template now picks it up automatically and switches to the export-aware logic
ABI without any extra hand edit in the wrapper.

## Logic Library Shape

Each logic library exports one symbol:

```cpp
extern "C" const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api();
```

Most applications should use the wrapper macro:

```cpp
#include "ecmcStrucppLogicWrapper.hpp"
#include "my_program.hpp"

ECMC_STRUCPP_DECLARE_LOGIC_API("my_logic",
                               strucpp::Program_MYPROGRAM,
                               strucpp::locatedVars);
```

That wrapper is designed to sit next to generated `STruCpp` output in your
application repo, not in this plugin repo.

## Build

This repo follows the same packaging style as the other `ecmc_plugin_*`
modules and uses `/ioc/tools/driver.makefile`.

The build needs access to the `STruCpp` runtime headers:

```sh
make STRUCPP=/path/to/strucpp
```

If `STRUCPP` is not set, the makefile defaults to `../strucpp`.

The bundled motion library is distributed with the repo, so normal users do not
need Node or `strucpp` just to consume `MC_*` blocks from an application repo.

## Startup Helper

Load the plugin through `require`, like the other `ecmc_plugin_*` modules.
That makes `$(ecmc_plugin_strucpp_DIR)` available and auto-executes
[`startup.cmd`](startup.cmd) with the macro string from the `require` line.

Example:

```iocsh
require ecmc_plugin_strucpp sandst_a "PLUGIN_ID=0,LOGIC_LIB=/absolute/path/to/machine_logic.so,INPUT_ITEM=ec0.s2.mm.inputDataArray01,OUTPUT_ITEM=ec0.s2.mm.outputDataArray01,MEMORY_BYTES=64,REPORT=1"
```

The startup helper accepts:

- `PLUGIN_ID`
- `LOGIC_LIB`
- `ASYN_PORT`
- `MAPPING_FILE`
- `INPUT_ITEM`
- `OUTPUT_ITEM`
- `INPUT_BINDINGS`
- `OUTPUT_BINDINGS`
- `MEMORY_BYTES`
- `EPICS_SUBST`
- `DB_PREFIX`
- `DB_MACROS`
- `REPORT`

There is also a concrete IOC example in
[`examples/loadPluginExample.cmd`](examples/loadPluginExample.cmd).

If `EPICS_SUBST` is provided, `startup.cmd` also calls `dbLoadTemplate(...)`
automatically after the plugin is loaded. The standard macro set passed by the
helper is:

- `P=$(DB_PREFIX)`
- `PORT=$(ASYN_PORT)`

and `DB_MACROS` is appended unchanged if you need extra template macros.

If `DB_PREFIX` is omitted, it defaults to `$(IOC)`.

If `EPICS_SUBST` is not provided, but `DB_PREFIX` or `DB_MACROS` is set,
`startup.cmd` defaults to:

```text
${LOGIC_LIB}.substitutions
```

That is the preferred convention. Application repos should generate the
substitutions file next to the logic library so the normal startup path only
needs `LOGIC_LIB` plus the desired database macros.

`EPICS_SUBST` remains the explicit override when you want to load a different
record file.

For a minimal EL7041 velocity-only sample that binds `%IW0` to
`positionActual01`, `%QW0` to `driveControl01`, and `%QW2` to
`velocitySetpoint01` through a startup-linked mapping file, see
[`examples/loadEL7041VelocityExample.cmd`](examples/loadEL7041VelocityExample.cmd).

For a motion-data sample that binds `%IL0` to `ax1.enc.actpos` and `%QL0` to
`ax1.traj.targetpos`, see
[`examples/loadMotionActposMirrorExample.cmd`](examples/loadMotionActposMirrorExample.cmd).

For a real ST motion-library sample using `MC_Power`, `MC_MoveAbsolute`, and
`MC_ReadActualPosition` through one contiguous input/output image, see
[`examples/loadMcPowerMoveAbsoluteLibExample.cmd`](examples/loadMcPowerMoveAbsoluteLibExample.cmd).

For a velocity-oriented ST motion-library sample using `MC_Power`,
`MC_MoveVelocity`, `MC_ReadStatus`, and `MC_ReadActualVelocity`, see
[`examples/loadMcPowerMoveVelocityLibExample.cmd`](examples/loadMcPowerMoveVelocityLibExample.cmd).

For a relative-move ST motion-library sample using `MC_Power`,
`MC_MoveRelative`, `MC_ReadStatus`, and `MC_ReadActualPosition`, see
[`examples/loadMcPowerMoveRelativeLibExample.cmd`](examples/loadMcPowerMoveRelativeLibExample.cmd).

## Quick Start

For a new app repo:

1. compile your ST with `-L /path/to/ecmc_plugin_strucpp/libs`
2. include [`src/ecmcStrucppLogicWrapper.hpp`](src/ecmcStrucppLogicWrapper.hpp)
   in the tiny logic wrapper
3. link the generated logic module against the `STruCpp` runtime headers and
   `ecmcMcApi.h`
