PROGRAM MAIN
VAR
  cycle_counter    AT %MW0 : INT;   // @epics rec=Main-CycleCounterAct
  target_high      AT %MX2.0 : BOOL;
  start_armed      AT %MX2.1 : BOOL;

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

cycle_counter := cycle_counter + 1;
axis.AxisIndex := 1;

readPos(Axis := axis, Enable := TRUE);
readStatus(Axis := axis, Enable := TRUE);
power(Axis := axis, Enable := TRUE);

readyTrig(CLK := power.Status AND readPos.Valid);
moveDoneTrig(CLK := moveAbs.Done);

IF readyTrig.Q THEN
  target_high := TRUE;
  start_armed := TRUE;
END_IF;

IF moveDoneTrig.Q THEN
  target_high := NOT target_high;
  start_armed := TRUE;
END_IF;

IF target_high THEN
  target_pos_export := 12800.0;
ELSE
  target_pos_export := 0.0;
END_IF;

moveAbs(Axis := axis,
        Execute := start_armed,
        Position := target_pos_export,
        Velocity := 1000.0,
        Acceleration := 2000.0,
        Deceleration := 2000.0);
start_armed := FALSE;

actual_pos_export := readPos.Position;
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
                Message := 'MC_MoveAbsolute -> 12800');
dbg_target_low(Execute := moveDoneTrig.Q AND NOT target_high,
               Message := 'MC_MoveAbsolute -> 0');
END_PROGRAM
