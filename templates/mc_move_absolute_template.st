PROGRAM MY_MOTION_APP
VAR
  axis        : ECMC_AXIS_REF;
  power       : MC_Power;
  moveAbs     : MC_MoveAbsolute;
  readPos     : MC_ReadActualPosition;

  enableCmd   AT %IX0.0 : BOOL;
  executeCmd  AT %IX0.1 : BOOL;

  targetPos   AT %IL8  : LREAL;
  velocity    AT %IL16 : LREAL;
  accel       AT %IL24 : LREAL;
  decel       AT %IL32 : LREAL;

  powerStatus AT %QX0.0 : BOOL;
  powerValid  AT %QX0.1 : BOOL;
  moveBusy    AT %QX0.2 : BOOL;
  moveDone    AT %QX0.3 : BOOL;
  moveError   AT %QX0.4 : BOOL;
  moveErrorId AT %QD4   : UDINT;
  actualPos   AT %QL8   : LREAL;
END_VAR

axis.AxisIndex := 0;

power(Axis := axis, Enable := enableCmd);
moveAbs(Axis := axis,
        Execute := executeCmd,
        Position := targetPos,
        Velocity := velocity,
        Acceleration := accel,
        Deceleration := decel);
readPos(Axis := axis, Enable := TRUE);

powerStatus := power.Status;
powerValid  := power.Valid;
moveBusy    := moveAbs.Busy;
moveDone    := moveAbs.Done;
moveError   := moveAbs.Error;
moveErrorId := moveAbs.ErrorID;
actualPos   := readPos.Position;
END_PROGRAM
