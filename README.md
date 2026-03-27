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

## Host Config

The host expects the normal `Cfg.LoadPlugin(...)` config string format:

```text
logic_lib=<path>;[mapping_file=<path>|input_item=<ecmc data item>|input_bindings=<offset:item[@bytes],...>];[output_item=<ecmc data item>|output_bindings=<offset:item[@bytes],...>];memory_bytes=<n>
```

Startup-linked mapping example:

```text
logic_lib=/abs/path/to/el7041_velocity_logic.so;mapping_file=/abs/path/to/el7041_velocity.map;memory_bytes=16
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

## Startup Helper

Use [`startup.cmd`](startup.cmd) to load the host plugin. It accepts:

- `PLUGIN_ID`
- `LOGIC_LIB`
- `MAPPING_FILE`
- `INPUT_ITEM`
- `OUTPUT_ITEM`
- `INPUT_BINDINGS`
- `OUTPUT_BINDINGS`
- `MEMORY_BYTES`
- `REPORT`

There is also a concrete IOC example in
[`examples/loadPluginExample.cmd`](examples/loadPluginExample.cmd).

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
