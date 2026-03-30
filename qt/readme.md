# caQtDM panel

Minimal panel for the built-in `ecmc_plugin_strucpp` control and status PVs.

Example:

```sh
caqtdm -macro "IOC=c6025a-04,PLG_ID=0" /path/to/ecmc_plugin_strucpp/qt/ecmc_plugin_strucpp_main.ui
```

The panel expects `IOC` without a trailing `:` and builds PV names like:

- `$(IOC):Plg-ST$(PLG_ID)-CtrlWord-RB`
- `$(IOC):Plg-ST$(PLG_ID)-SmpMs-RB`
- `$(IOC):Plg-ST$(PLG_ID)-ExeMsAct`
- `$(IOC):Plg-ST$(PLG_ID)-TotMsAct`
- `$(IOC):Plg-ST$(PLG_ID)-DbgTxtAct`

The control word is edited through a `caByteController` with:

- bit 0: enable ST execution
- bit 1: measure ST logic time
- bit 2: enable ST debug prints
- bit 3: measure total plugin cycle time

The requested sample period is also editable directly.

The latest short ST debug message is shown in `Dbg text` when control-word
bit 2 is enabled and ST code calls `ECMC_DebugPrint`.
