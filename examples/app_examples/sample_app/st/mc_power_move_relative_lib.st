PROGRAM MC_POWER_MOVE_RELATIVE_LIB
VAR
  axis         : ECMC_AXIS_REF;
  power        : MC_Power;
  moveRel      : MC_MoveRelative;
  readStatus   : MC_ReadStatus;
  readPos      : MC_ReadActualPosition;

  enableCmd    AT %IX0.0 : BOOL;
  executeCmd   AT %IX0.1 : BOOL;

  distanceCmd  AT %IL8  : LREAL;
  velocityCmd  AT %IL16 : LREAL;
  accelCmd     AT %IL24 : LREAL;
  decelCmd     AT %IL32 : LREAL;

  powerStatus  AT %QX0.0 : BOOL;
  powerValid   AT %QX0.1 : BOOL;
  moveBusy     AT %QX0.2 : BOOL;
  moveDone     AT %QX0.3 : BOOL;
  moveError    AT %QX0.4 : BOOL;
  standStill   AT %QX0.5 : BOOL;
  discreteMove AT %QX0.6 : BOOL;
  errorStop    AT %QX0.7 : BOOL;
  moveErrorId  AT %QD4   : UDINT;
  actualPos    AT %QL8   : LREAL;
END_VAR

axis.AxisIndex := 0;

power(Axis := axis, Enable := enableCmd);
moveRel(Axis := axis,
        Execute := executeCmd,
        Distance := distanceCmd,
        Velocity := velocityCmd,
        Acceleration := accelCmd,
        Deceleration := decelCmd);
readStatus(Axis := axis, Enable := TRUE);
readPos(Axis := axis, Enable := TRUE);

powerStatus  := power.Status;
powerValid   := power.Valid;
moveBusy     := moveRel.Busy;
moveDone     := moveRel.Done;
moveError    := moveRel.Error;
standStill   := readStatus.StandStill;
discreteMove := readStatus.DiscreteMotion;
errorStop    := readStatus.ErrorStop;
moveErrorId  := moveRel.ErrorID;
actualPos    := readPos.Position;
END_PROGRAM
