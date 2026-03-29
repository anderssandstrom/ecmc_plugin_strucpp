# `sample_app`

This directory is the former `ecmc_strucpp_app_example` sample app, moved into
[`ecmc_plugin_strucpp`](../../..).

It contains:

- direct-mapped ST samples under [`st`](st)
- tiny logic wrappers under [`src`](src)
- checked-in generated `STruCpp` sources under [`src/generated`](src/generated)
- a standalone sample build in [GNUmakefile](GNUmakefile)

The sample build outputs loadable logic libraries plus matching `.map` and
`.substitutions` files into `build/`.

Build:

```sh
make STRUCPP=/path/to/strucpp
```

Defaults:

- `STRUCPP=../../../../strucpp`
- `ECMC_PLUGIN_STRUCPP=../../..`

The IOC-style examples that use these patterns live next to this sample app in:

- [`../ioc_project_minimal`](../ioc_project_minimal)
- [`../ioc_project_example`](../ioc_project_example)

The corresponding plugin-side startup examples remain in:

- [`../../iocsh_examples/loadEL7041VelocityExample.cmd`](../../iocsh_examples/loadEL7041VelocityExample.cmd)
- [`../../iocsh_examples/loadMotionActposMirrorExample.cmd`](../../iocsh_examples/loadMotionActposMirrorExample.cmd)
- [`../../iocsh_examples/loadMotionVelocityDirectExample.cmd`](../../iocsh_examples/loadMotionVelocityDirectExample.cmd)
