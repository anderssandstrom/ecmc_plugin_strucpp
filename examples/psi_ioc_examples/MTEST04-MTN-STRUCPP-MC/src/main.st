PROGRAM MAIN
VAR
  cycle_counter    AT %MW0 : INT;   // @epics rec=Main-CycleCounterAct
  target_high      AT %MX2.0 : BOOL;
  move_execute     AT %MX2.1 : BOOL;
  move_request     AT %MX2.2 : BOOL;

  axis             : ECMC_AXIS_REF;
  power            : MC_Power;
  moveAbs          : MC_MoveAbsolute;
  readPos          : MC_ReadActualPosition;
  readStatus       : MC_ReadStatus;
  readyTrig        : R_TRIG;
  moveDoneTrig     : R_TRIG;

  actual_pos_export: LREAL;  // @epics prefix=$(IOC): rec=Main-PosActMc
  target_pos_export: LREAL;  // @epics rec=Main-TargetPosAct
  power_ok_export  : BOOL;   // @epics rec=Main-Stat bit=0
  move_busy_export : BOOL;   // @epics rec=Main-Stat bit=1
  move_done_export : BOOL;   // @epics rec=Main-Stat bit=2
  valid_export     : BOOL;   // @epics rec=Main-Stat bit=3

  dbg_sample_trigger : BOOL;
  startup_printed    : BOOL;
  dbg_startup        : ECMC_DebugPrint;
  dbg_position       : ECMC_DebugPrint;
  dbg_target_low     : ECMC_DebugPrint;
  dbg_target_high    : ECMC_DebugPrint;
END_VAR

// Optional for demo visibility. Remove if you want quiet operation.
ECMC_DebugPrintEnable(Enable := TRUE);

cycle_counter := cycle_counter + 1;
axis.AxisIndex := 1;

readPos(Axis := axis, Enable := TRUE);
readStatus(Axis := axis, Enable := TRUE);
power(Axis := axis, Enable := TRUE);

readyTrig(CLK := power.Status AND power.Valid AND readPos.Valid AND
                 readStatus.Valid AND readStatus.StandStill AND
                 NOT readStatus.Disabled AND NOT readStatus.ErrorStop);

IF readyTrig.Q THEN
  target_high := TRUE;
  move_request := TRUE;
END_IF;

IF target_high THEN
  target_pos_export := 3600.0;
ELSE
  target_pos_export := 0.0;
END_IF;

moveAbs(Axis := axis,
        Execute := move_execute,
        Position := target_pos_export,
        Velocity := 360.0,
        Acceleration := 360.0,
        Deceleration := 360.0);

actual_pos_export := readPos.Position;
moveDoneTrig(CLK := moveAbs.Done);

IF moveDoneTrig.Q THEN
  target_high := NOT target_high;
  move_request := TRUE;
END_IF;

IF move_execute AND (moveAbs.Done OR moveAbs.Error OR moveAbs.CommandAborted) THEN
  move_execute := FALSE;
END_IF;

IF move_request AND NOT move_execute AND NOT moveAbs.Done THEN
  move_execute := TRUE;
  move_request := FALSE;
END_IF;

power_ok_export := power.Status;
move_busy_export := moveAbs.Busy;
move_done_export := moveAbs.Done;
valid_export := readStatus.Valid;

dbg_startup(Execute := NOT startup_printed,
            Message := 'MC motion program started');
startup_printed := TRUE;

dbg_sample_trigger := (cycle_counter MOD 1000) = 0;
dbg_position(Execute := dbg_sample_trigger,
             Message := CONCAT('actual_position=',
                               TO_STRING(actual_pos_export)));

dbg_target_high(Execute := readyTrig.Q OR (moveDoneTrig.Q AND target_high),
                Message := 'MC_MoveAbsolute -> 3600');
dbg_target_low(Execute := moveDoneTrig.Q AND NOT target_high,
               Message := 'MC_MoveAbsolute -> 0');
END_PROGRAM
