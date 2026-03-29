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

Built-in control shortcuts:

- `Enable` writes `1` to the control word
- `Disable` writes `0`
- `Enable+Meas` writes `3`

The raw control word and requested sample period are also editable directly.
