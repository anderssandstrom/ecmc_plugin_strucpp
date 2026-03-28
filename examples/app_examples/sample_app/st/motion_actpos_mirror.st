PROGRAM MOTION_ACTPOS_MIRROR
VAR
  source_actpos   AT %IL0 : LREAL;  // @ecmc ax${AXIS_INDEX}.enc.actpos
  target_position AT %QL0 : LREAL;  // @ecmc ax${AXIS_INDEX}.traj.targetpos
END_VAR

target_position := source_actpos;
END_PROGRAM
