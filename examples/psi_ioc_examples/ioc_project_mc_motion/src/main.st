PROGRAM MAIN
VAR
  axis            : ECMC_AXIS_REF;
  power           : MC_Power;
  moveAbs         : MC_MoveAbsolute;
  readPos         : MC_ReadActualPosition;
  readStatus      : MC_ReadStatus;

  enable_cmd      : BOOL;   // @epics rw rec=Main-Cmd bit=0
  execute_cmd     : BOOL;   // @epics rw rec=Main-Cmd bit=1
  target_pos      : LREAL;  // @epics rw rec=Main-TargetPos-RB
  velocity        : LREAL;  // @epics rw rec=Main-Velocity-RB
  acceleration    : LREAL;  // @epics rw rec=Main-Acceleration-RB
  deceleration    : LREAL;  // @epics rw rec=Main-Deceleration-RB

  power_status    : BOOL;   // @epics rec=Main-Stat bit=0
  move_busy       : BOOL;   // @epics rec=Main-Stat bit=1
  move_done       : BOOL;   // @epics rec=Main-Stat bit=2
  move_error      : BOOL;   // @epics rec=Main-Stat bit=3
  move_error_id   : UDINT;  // @epics rec=Main-MoveErrorId
  actual_pos      : LREAL;  // @epics rec=Main-ActualPos
  status_valid    : BOOL;   // @epics rec=Main-Stat bit=4
  status_homed    : BOOL;   // @epics rec=Main-Stat bit=5
  status_errorstop: BOOL;   // @epics rec=Main-Stat bit=6
END_VAR

axis.AxisIndex := 1;

power(Axis := axis, Enable := enable_cmd);
moveAbs(Axis := axis,
        Execute := execute_cmd,
        Position := target_pos,
        Velocity := velocity,
        Acceleration := acceleration,
        Deceleration := deceleration);
readPos(Axis := axis, Enable := TRUE);
readStatus(Axis := axis, Enable := TRUE);

power_status     := power.Status;
move_busy        := moveAbs.Busy;
move_done        := moveAbs.Done;
move_error       := moveAbs.Error;
move_error_id    := moveAbs.ErrorID;
actual_pos       := readPos.Position;
status_valid     := readStatus.Valid;
status_homed     := readStatus.IsHomed;
status_errorstop := readStatus.Errorstop;
END_PROGRAM
