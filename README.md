# `ecmc_plugin_strucpp`

`ecmc_plugin_strucpp` is a generic `ecmc` host plugin for running
loadable `STruCpp` logic libraries inside the normal `ecmc` realtime loop.

In this repo, `ST` means **Structured Text** as defined by **IEC 61131-3**,
the PLC programming-language standard. It does not mean EPICS sequencer
State Notation Language `.st` files.

The plugin is meant to run ST code like this:

```iecst
PROGRAM MAIN
VAR
  actual_position    AT %IW0 : INT;   // @ecmc ec.s${SLAVE_ID=14}.positionActual${CH_ID}
  drive_control      AT %QW0 : WORD;  // @ecmc ec.s${SLAVE_ID=14}.driveControl${CH_ID}
  velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s${SLAVE_ID=14}.velocitySetpoint${CH_ID}
  cycle_counter      AT %MW0 : INT;   // @epics rec=Main-CycleCounterAct
END_VAR

cycle_counter := cycle_counter + 1;
drive_control := 16#0001;

IF actual_position < 0 THEN
  velocity_setpoint := 1000;
ELSIF actual_position > 1000 THEN
  velocity_setpoint := -1000;
END_IF;
END_PROGRAM
```

That example reads and writes named `ecmc` data items through `%I/%Q`, keeps
internal state in `%M`, and exposes one variable to EPICS with `@epics`.

This is not limited to one flat `PROGRAM`. Normal IEC 61131-3
`FUNCTION_BLOCK`s, `FUNCTION`s, and reusable helper code can be used as well.
The repo also ships a bundled motion library with PLCopen-style `MC_*` blocks
such as `MC_Power`, `MC_MoveAbsolute`, `MC_MoveVelocity`, and
`MC_ReadActualPosition`.

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

## Qt Panel

This repo also ships a small caQtDM panel for the built-in plugin control and
status PVs:

- [`qt/ecmc_plugin_strucpp_main.ui`](qt/ecmc_plugin_strucpp_main.ui)
- [`qt/readme.md`](qt/readme.md)

Example:

```sh
caqtdm -macro "IOC=c6025a-04,PLG_ID=0" /path/to/ecmc_plugin_strucpp/qt/ecmc_plugin_strucpp_main.ui
```

The panel targets the built-in records such as:

- `$(IOC):Plg-ST$(PLG_ID)-CtrlWord-RB`
- `$(IOC):Plg-ST$(PLG_ID)-SmpMs-RB`
- `$(IOC):Plg-ST$(PLG_ID)-ExeMsAct`

## App Build Helper

This repo now also ships a reusable IOC/app-side build helper:

- [`templates/strucpp_ioc_logic.make`](templates/strucpp_ioc_logic.make)
- [`scripts/strucpp_logic_wrappergen.py`](scripts/strucpp_logic_wrappergen.py)

The intent is to remove most per-IOC boilerplate. A small `src/Makefile` can
now usually be just:

```make
PROGRAM := machine
ECMC_PLUGIN_STRUCPP ?= ../../../ecmc_plugin_strucpp
include $(ECMC_PLUGIN_STRUCPP)/templates/strucpp_ioc_logic.make
```

For multi-file ST projects, set `ST_SOURCES` in declaration order, for example:

```make
PROGRAM := machine
ST_SOURCES := machine_types.st machine_fbs.st machine.st
ECMC_PLUGIN_STRUCPP ?= ../../../ecmc_plugin_strucpp
include $(ECMC_PLUGIN_STRUCPP)/templates/strucpp_ioc_logic.make
```

The helper bundles those files into one generated ST source before running
`strucpp`, so helper FBs, types, and the final `PROGRAM` can live in separate
files. The same `${NAME}` and `${NAME=default}` placeholder syntax can be
expanded across all bundled ST source files before code generation.

If you need handwritten C++ in the same logic library, the helper also supports
two escape hatches:

```make
PROGRAM := machine
ST_SOURCES := machine_fbs.st machine.st
WRAPPER_CPP := custom_logic_wrapper.cpp
EXTRA_CPP_SOURCES := helper.cpp adapters/custom_fb.cpp
ECMC_PLUGIN_STRUCPP ?= ../../../ecmc_plugin_strucpp
include $(ECMC_PLUGIN_STRUCPP)/templates/strucpp_ioc_logic.make
```

- `WRAPPER_CPP`
  overrides the generated wrapper with your own C++ wrapper
- `EXTRA_CPP_SOURCES`
  compiles extra handwritten C++ translation units into the same logic library

The sample IOC project in
[`examples/psi_ioc_examples/ioc_project_example`](examples/psi_ioc_examples/ioc_project_example)
now includes concrete opt-in files for this path:

- `src/Makefile.with_cpp`
- `src/custom_logic_wrapper.cpp`
- `src/machine_helper.cpp`

For a PSI-style IOC example that uses the bundled `MC_*` motion library and
exports EPICS-triggered command bits/values, see:

- [`examples/psi_ioc_examples/ioc_project_mc_motion`](examples/psi_ioc_examples/ioc_project_mc_motion)

When `WRAPPER_CPP` points at your own file, the helper stops generating the
default wrapper and compiles the provided source instead.

That common include handles:

- ordered ST source bundling
- `strucpp` code generation
- export header generation from `// @epics ...`
- mapping file generation from `// @ecmc ...`
- substitutions generation from `// @epics ...`
- logic wrapper generation
- logic library build
- staging of `bin/<logic>.so`, `.map`, and `.substitutions`

So the IOC project usually only needs:

- one ST source file
- one short `src/Makefile`
- one startup script with `require ecmc_plugin_strucpp ...`

and only drops to handwritten C++ when you explicitly opt into one of the
escape hatches above.

## IOC Scaffold

For the smoothest default path, this repo also ships a small scaffold tool:

- [`scripts/strucpp_new_ioc.py`](scripts/strucpp_new_ioc.py)

Example:

```sh
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_new_ioc.py my_ioc
```

That creates a minimal IOC project with:

- `src/main.st`
- `src/Makefile`
- `<IOC_NAME>_startup.script`
- `<IOC_NAME>_parameters.yaml`
- a top-level `Makefile`

The generated scaffold follows the shortest current convention:

- `PROGRAM := main`
- `LOGIC_NAME := main`
- startup loads `bin/main.so`
- mapping defaults to `bin/main.so.map`
- EPICS substitutions default to `bin/main.so.substitutions`

The generated ST source uses direct EL7041 item mapping as a concrete example.
Adjust the `// @ecmc ...` lines to match the real slave and PDO items for your
machine.

## Declaration Generator

To reduce the remaining manual work for direct mapping, this repo also ships a
small declaration generator:

- [`scripts/strucpp_declgen.py`](scripts/strucpp_declgen.py)
- [`templates/strucpp_declgen.manifest`](templates/strucpp_declgen.manifest)

It takes a small manifest like:

```text
I   INT   actual_position    ec.s14.positionActual01
Q   WORD  drive_control      ec.s14.driveControl01
Q   INT   velocity_setpoint  ec.s14.velocitySetpoint01
M   INT   cycle_counter
VAR INT   manual_velocity
```

and generates ST declarations with automatic `%I/%Q/%M` addresses:

```sh
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_declgen.py \
  --input my_axis.manifest \
  --output main.st \
  --program MAIN
```

That produces:

```iecst
PROGRAM MAIN
VAR
  actual_position   AT %IW0 : INT;   // @ecmc ec.s14.positionActual01
  drive_control     AT %QW0 : WORD;  // @ecmc ec.s14.driveControl01
  velocity_setpoint AT %QW2 : INT;   // @ecmc ec.s14.velocitySetpoint01
  cycle_counter     AT %MW0 : INT;
  manual_velocity   : INT;
END_VAR

// Generated from my_axis.manifest. Add logic below.
END_PROGRAM
```

Current behavior:

- `%I/%Q/%M` addresses are assigned automatically
- `BOOL` values are bit-packed as `%IXn.m` / `%QXn.m` / `%MXn.m`
- wider scalar types are byte-aligned and naturally aligned by width
- `VAR` entries generate plain non-located ST variables

## Manifest Generator

To reduce the manual work even before `declgen`, this repo also ships:

- [`scripts/strucpp_manifestgen.py`](scripts/strucpp_manifestgen.py)

It takes a short list of `ecmc` item names and creates a first-draft manifest
for `strucpp_declgen.py`.

Example input:

```text
ec.s14.positionActual01
ec.s14.driveControl01
ec.s14.velocitySetpoint01
```

Example command:

```sh
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_manifestgen.py \
  --input items.txt \
  --output axis.manifest
```

Example output:

```text
# AREA TYPE   NAME              ECMC_ITEM
I  INT   actual_position    ec.s14.positionActual01
Q  WORD  drive_control      ec.s14.driveControl01
Q  INT   velocity_setpoint  ec.s14.velocitySetpoint01
```

The generator is heuristic by design. It is meant to produce a useful first
draft that you edit as needed, not a perfect type inference engine.

## App Tool

For a single front-door entry point, this repo now also ships:

- [`scripts/strucpp_app_tool.py`](scripts/strucpp_app_tool.py)

It wraps the common workflows behind subcommands:

```sh
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_app_tool.py new-ioc my_ioc
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_app_tool.py manifest --input items.txt --output axis.manifest
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_app_tool.py declgen --input axis.manifest --output src/main.st
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_app_tool.py build --project my_ioc
python3 /path/to/ecmc_plugin_strucpp/scripts/strucpp_app_tool.py validate --project my_ioc
```

Behavior:

- `new-ioc`
  wraps [`strucpp_new_ioc.py`](scripts/strucpp_new_ioc.py)
- `declgen`
  wraps [`strucpp_declgen.py`](scripts/strucpp_declgen.py)
- `manifest`
  wraps [`strucpp_manifestgen.py`](scripts/strucpp_manifestgen.py)
- `build`
  runs `make <target>` in the chosen project directory
- `validate`
  runs the helper's `make validate`, defaulting to `<project>/src` when that
  layout exists

Useful options:

- `--dry-run`
  passes `-n` to `make`
- `--make-arg STRUCPP=/path/to/strucpp`
  forwards extra make variable assignments

The scaffold only picks the smallest default shape. The generated
`src/Makefile` still uses the shared helper, so you can later extend it with:

- `ST_SOURCES := types.st fbs.st main.st`
- `WRAPPER_CPP := custom_wrapper.cpp`
- `EXTRA_CPP_SOURCES := helper.cpp`
- `ANNOTATION_DEFINES := AXIS_INDEX=2`

The same helper now also generates:

- `${LOGIC_LIB}.summary.txt`

and supports a dry-run validation target:

```sh
make validate
```

That validation step checks the generated header, mapping file, and
substitutions file against the bundled ST source before runtime.

## Host Config

The host expects the normal `Cfg.LoadPlugin(...)` config string format:

```text
logic_lib=<path>;asyn_port=<plugin asyn port>;[mapping_file=<path>|input_item=<ecmc data item>|input_bindings=<offset:item[@bytes],...>];[output_item=<ecmc data item>|output_bindings=<offset:item[@bytes],...>];memory_bytes=<n>;sample_rate_ms=<n>
```

Startup-linked mapping example:

```text
logic_lib=/abs/path/to/el7041_velocity_logic.so;asyn_port=PLUGIN.STRUCPP0;mapping_file=/abs/path/to/el7041_velocity_logic.so.map;memory_bytes=16
```

Contiguous image example:

```text
logic_lib=/abs/path/to/machine_logic.so;input_item=ec0.s2.mm.inputDataArray01;output_item=ec0.s2.mm.outputDataArray01;memory_bytes=64
```

Direct scalar binding example:

```text
logic_lib=/abs/path/to/el7041_velocity_logic.so;input_bindings=0:ec.s14.positionActual01@2;output_bindings=0:ec.s14.driveControl01@2,2:ec.s14.velocitySetpoint01@2;memory_bytes=16
```

- `logic_lib`
  absolute path to the loadable logic library
- `mapping_file`
  path to a startup-linked manifest that maps exact STruCpp addresses like
  `%IW0` or `%QW2` to `ecmcDataItem` names
- `asyn_port`
  dedicated plain `asynPortDriver` port owned by `ecmc_plugin_strucpp`, defaults to
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
- `sample_rate_ms`
  requested ST logic sample period in milliseconds, defaults to the full
  EtherCAT/plugin rate

Use `mapping_file` when you want the plugin to inspect the current ST code at
startup, verify every used `%I/%Q` address, and link it directly to final
`ecmcDataItem` buffers once before the RT loop starts. Use `input_item` /
`output_item` when your logic naturally maps to one contiguous byte image, for
example an existing memmap. Use `input_bindings` / `output_bindings` when you
want explicit offset-to-item control without an external manifest. The EL6002
example in this repo intentionally uses contiguous images. The EL7041 example
and the IOC project examples use `mapping_file` and are the preferred default
pattern for new projects.

`sample_rate_ms` lets the host derive an integer execute divider from the
current `ecmc` sample time before realtime starts. The EtherCAT master still
runs at full rate, but the ST logic is only sampled/copied/run every Nth
cycle. For example, with a 1 ms EtherCAT period and `sample_rate_ms=10`, the
host derives `execute_divider=10` and the ST logic runs every 10 ms.

If `MAPPING_FILE` is not provided and none of `INPUT_ITEM`, `OUTPUT_ITEM`,
`INPUT_BINDINGS`, or `OUTPUT_BINDINGS` are set, the plugin defaults the
mapping file to:

```text
${LOGIC_LIB}.map
```

That is the preferred convention. App repos should generate the map next to the
logic library so the normal startup path needs no extra mapping macro, and in
the standard IOC layout can omit `LOGIC_LIB` too.

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
%IW0=ec.s14.positionActual01
%QW0=ec.s14.driveControl01
%QW2=ec.s14.velocitySetpoint01
```

The names in that file are read directly by the plugin. A leading `ec.s...`
means "use the current/default EtherCAT master index from ecmc before
realtime", so `ec.s14.positionActual01` resolves to `ec0.s14.positionActual01`
or `ec1.s14.positionActual01` depending on the configured master. Explicit
names like `ec0.s14...` and `ec1.s14...` remain valid and are not rewritten.

To avoid maintaining `%IW0` / `%QW2` addresses by hand, this repo also ships a
small generator in [`scripts/strucpp_mapgen.py`](scripts/strucpp_mapgen.py).

The generators now also perform stronger checks before runtime, including:

- malformed `@ecmc` / `@epics` annotations
- duplicate located addresses in generated forwards
- duplicate `@epics` export names
- conflicting mapping entries
- summary warnings for overlapping located addresses

`ANNOTATION_DEFINES` is applied while bundling ST source files and while
processing generated annotation metadata. For example, motion samples can use:

```iecst
actual_position AT %IL0 : LREAL; // @ecmc ax${AXIS_INDEX}.enc.actpos
```

and the build helper can supply:

```make
ANNOTATION_DEFINES := AXIS_INDEX=2
```

so the generated map resolves to `ax2.enc.actpos` without editing the ST code.
Inline defaults are also supported in annotations, for example:

```iecst
actual_position   AT %IW0 : INT;  // @ecmc ec.s${SLAVE=14}.positionActual01
drive_control     AT %QW0 : WORD; // @ecmc ec${MASTER=0}.s${SLAVE=14}.driveControl01
```

and values from `ANNOTATION_DEFINES` still override those defaults:

```make
ANNOTATION_DEFINES := SLAVE=18 MASTER=1
```

It reads the generated `STruCpp` header forwards and, preferably, inline ST
annotations of the form:

```text
actual_position    AT %IW0 : INT;   // @ecmc ec.s14.positionActual01
drive_control      AT %QW0 : WORD;  // @ecmc ec.s14.driveControl01
velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s14.velocitySetpoint01
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
counter       : INT;    // @epics
manual_target : INT;    // @epics rw
special_name  : INT;    // @epics custom.path.value
other_name    : INT;    // @epics custom.path.value rw
short_rec     : INT;    // @epics rec=Main-ShortRec
custom_pfx    : INT;    // @epics prefix=$(IOC): rec=Main-CustomPfx
named_asyn    : INT;    // @epics custom.path.value prefix=$(IOC): rec=Main-FullOverride rw
cmd_enable    : BOOL;   // @epics rw rec=Main-Cmd bit=0
cmd_execute   : BOOL;   // @epics rw rec=Main-Cmd bit=1
stat_busy     : BOOL;   // @epics rec=Main-Stat bit=0
stat_error    : BOOL;   // @epics rec=Main-Stat bit=1
```

Rules:

- the annotation lives on the ST variable declaration line
- `@epics` with no explicit name derives:
  `plugin.strucpp0.<program>.<variable>`
- the first token after `@epics`, when present, is the explicit exported asyn
  parameter name override
- `rec=<record-suffix>` optionally overrides the generated record suffix while
  still using the normal `P` prefix
- `prefix=<PV-prefix>` optionally overrides the prefix used for that one
  record
- `bit=<0..31>` on `BOOL` declarations enables packed bitfield export; use
  `group=<name>` to choose the internal/asyn group name, or just use
  `rec=<name>` and the same value will be used as the group when `group=` is
  omitted
- packed `BOOL` exports become one exported `UInt32Digital` value and one
  generated `mbbiDirect` or `mbboDirect` record
- for example:
  `rec=Main-Value`
  `prefix=$(IOC): rec=Main-Value`
- packed `BOOL` exports require explicit `bit=` numbering and keep read-only
  and writable groups separate
- optional final token `rw` makes the parameter writable from EPICS
- default is read-only
- current support is for top-level program variables:
  `BOOL`, `SINT`, `USINT`, `BYTE`, `INT`, `UINT`, `WORD`, `DINT`, `UDINT`,
  `DWORD`, `REAL`, `LREAL`; grouped export packing is available only for
  `BOOL`
- duplicate exported names are rejected at startup unless they are explicit
  members of the same grouped `BOOL` export

The application repo runs
[`scripts/strucpp_epics_exportgen.py`](scripts/strucpp_epics_exportgen.py) to
turn those annotations into a small generated export header. The logic library
then exposes that export table through the logic ABI, and the host creates
matching asyn parameters at startup on the configured `asyn_port`.

The current implementation uses a plugin-owned plain `asynPortDriver`, not the
main `ecmc` asyn driver. Exported values are updated on change, and callback
flushing is deferred out of the RT loop through a small low-priority worker
thread in the plugin.

The plugin also publishes a small built-in control/status set on the same port:

- `plugin.strucpp0.ctrl.word`
- `plugin.strucpp0.ctrl.rate_ms`
- `plugin.strucpp0.stat.rate_ms`
- `plugin.strucpp0.stat.exec_ms`
- `plugin.strucpp0.stat.div`
- `plugin.strucpp0.stat.count`

The default substitutions file maps those internal asyn parameter names to
shorter plugin-style record names:

- `Plg-ST0-CtrlWord-RB`
- `Plg-ST0-SmpMs-RB`
- `Plg-ST0-SmpMsAct`
- `Plg-ST0-ExeMsAct`
- `Plg-ST0-DivAct`
- `Plg-ST0-CntAct`

`ctrl.word` uses:

- bit 0: enable ST execution
- bit 1: enable execution-time measurement

`stat.exec_ms` is the last measured ST execution time and is only updated while
the measurement bit is enabled.

`stat.count` is intentionally rate-limited to a maximum PV update rate of
10 Hz.

This repo also ships macro-based EPICS database templates in [`db`](db) and a
generator,
[`scripts/strucpp_epics_substgen.py`](scripts/strucpp_epics_substgen.py),
that turns the same `// @epics ...` annotations into a `.substitutions` file.
The generated substitutions keep the full internal asyn name in `ASYN`, but
derive a shorter EPICS record name in `REC`, following the other plugin
templates more closely. Grouped `BOOL` exports generate packed
`mbbiDirect`/`mbboDirect` records automatically.

In addition, the repo ships a default built-in substitutions file:

- [`db/ecmcStrucppCore.substitutions`](db/ecmcStrucppCore.substitutions)

`startup.cmd` loads that built-in substitutions file by default unless
`LOAD_DEFAULT_PVS=0` is passed in the `require` macro string.

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
require ecmc_plugin_strucpp sandst_a "REPORT=1"
```

That is the standard IOC-layout case:

- `bin/main.so`
- `bin/main.so.map`
- `bin/main.so.substitutions`

When your project uses a different logic-library path or explicit contiguous
images, add the corresponding macros explicitly, for example:

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
- `SAMPLE_RATE_MS`
- `LOAD_DEFAULT_PVS`
- `EPICS_SUBST`
- `DB_PREFIX`
- `DB_MACROS`
- `REPORT`

`SAMPLE_RATE_MS` is forwarded by [`startup.cmd`](startup.cmd) as
`sample_rate_ms=<n>` in the plugin config string.

There is also a concrete IOC example in
[`examples/iocsh_examples/loadPluginExample.cmd`](examples/iocsh_examples/loadPluginExample.cmd).

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
needs database macros, or only `LOGIC_LIB` when using a non-default library
path.

`EPICS_SUBST` remains the explicit override when you want to load a different
record file.

For a minimal EL7041 velocity-only sample that binds `%IW0` to
`positionActual01`, `%QW0` to `driveControl01`, and `%QW2` to
`velocitySetpoint01` through a startup-linked mapping file, see
[`examples/iocsh_examples/loadEL7041VelocityExample.cmd`](examples/iocsh_examples/loadEL7041VelocityExample.cmd).

For a motion-data sample that binds `%IL0` to `ax1.enc.actpos` and `%QL0` to
`ax1.traj.targetpos`, see
[`examples/iocsh_examples/loadMotionActposMirrorExample.cmd`](examples/iocsh_examples/loadMotionActposMirrorExample.cmd).

For a real ST motion-library sample using `MC_Power`, `MC_MoveAbsolute`, and
`MC_ReadActualPosition` through one contiguous input/output image, see
[`examples/iocsh_examples/loadMcPowerMoveAbsoluteLibExample.cmd`](examples/iocsh_examples/loadMcPowerMoveAbsoluteLibExample.cmd).

For a velocity-oriented ST motion-library sample using `MC_Power`,
`MC_MoveVelocity`, `MC_ReadStatus`, and `MC_ReadActualVelocity`, see
[`examples/iocsh_examples/loadMcPowerMoveVelocityLibExample.cmd`](examples/iocsh_examples/loadMcPowerMoveVelocityLibExample.cmd).

For a relative-move ST motion-library sample using `MC_Power`,
`MC_MoveRelative`, `MC_ReadStatus`, and `MC_ReadActualPosition`, see
[`examples/iocsh_examples/loadMcPowerMoveRelativeLibExample.cmd`](examples/iocsh_examples/loadMcPowerMoveRelativeLibExample.cmd).

## Quick Start

For a new app repo:

1. compile your ST with `-L /path/to/ecmc_plugin_strucpp/libs`
2. include [`src/ecmcStrucppLogicWrapper.hpp`](src/ecmcStrucppLogicWrapper.hpp)
   in the tiny logic wrapper
3. link the generated logic module against the `STruCpp` runtime headers and
   `ecmcMcApi.h`
