PROGRAM MC_POWER_MOVE_VELOCITY_LIB
VAR
  axis        : ECMC_AXIS_REF;
  power       : MC_Power;
  moveVel     : MC_MoveVelocity;
  readStatus  : MC_ReadStatus;
  readVel     : MC_ReadActualVelocity;

  enableCmd   AT %IX0.0 : BOOL;
  executeCmd  AT %IX0.1 : BOOL;

  velocityCmd AT %IL8  : LREAL;
  accelCmd    AT %IL16 : LREAL;
  decelCmd    AT %IL24 : LREAL;

  powerStatus AT %QX0.0 : BOOL;
  powerValid  AT %QX0.1 : BOOL;
  inVelocity  AT %QX0.2 : BOOL;
  moveBusy    AT %QX0.3 : BOOL;
  moveError   AT %QX0.4 : BOOL;
  standStill  AT %QX0.5 : BOOL;
  contMotion  AT %QX0.6 : BOOL;
  errorStop   AT %QX0.7 : BOOL;
  moveErrorId AT %QD4   : UDINT;
  actualVel   AT %QL8   : LREAL;
END_VAR

axis.AxisIndex := 0;

power(Axis := axis, Enable := enableCmd);
moveVel(Axis := axis,
        Execute := executeCmd,
        Velocity := velocityCmd,
        Acceleration := accelCmd,
        Deceleration := decelCmd);
readStatus(Axis := axis, Enable := TRUE);
readVel(Axis := axis, Enable := TRUE);

powerStatus := power.Status;
powerValid  := power.Valid;
inVelocity  := moveVel.InVelocity;
moveBusy    := moveVel.Busy;
moveError   := moveVel.Error;
standStill  := readStatus.StandStill;
contMotion  := readStatus.ContinuousMotion;
errorStop   := readStatus.ErrorStop;
moveErrorId := moveVel.ErrorID;
actualVel   := readVel.Velocity;
END_PROGRAM
