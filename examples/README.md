# Examples

This directory is split into two groups:

- [`psi_ioc_examples`](psi_ioc_examples)
  source examples for PSI-style IOC projects and the standalone sample app
- [`iocsh_examples`](iocsh_examples)
  direct `iocsh` startup snippets for loading the plugin and sample logic

Start with [`psi_ioc_examples/ioc_project_minimal`](psi_ioc_examples/ioc_project_minimal)
for the smallest end-to-end IOC example.

For a motion-library IOC example with EPICS-triggered `MC_*` commands, see
[`psi_ioc_examples/ioc_project_mc_motion`](psi_ioc_examples/ioc_project_mc_motion).

For a self-running `MC_MoveAbsolute` bounce example derived from the MTEST
layout, see
[`psi_ioc_examples/MTEST04-MTN-STRUCPP-MC`](psi_ioc_examples/MTEST04-MTN-STRUCPP-MC).

For a self-running direct-mapped PID position example derived from the same
MTEST layout, see
[`psi_ioc_examples/MTEST04-MTN-STRUCPP-PID`](psi_ioc_examples/MTEST04-MTN-STRUCPP-PID).
