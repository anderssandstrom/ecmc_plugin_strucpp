PROGRAM MAIN
VAR
  actual_position    AT %IW0 : INT;   // @ecmc ec.s${SLAVE_ID=14}.positionActual${CH_ID}
  drive_control      AT %QW0 : WORD;  // @ecmc ec.s${SLAVE_ID=14}.driveControl${CH_ID}
  velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s${SLAVE_ID=14}.velocitySetpoint${CH_ID}
  cycle_counter      AT %MW0 : INT;   // @epics rec=Main-CycleCounterAct
  target_high        AT %MX2.0 : BOOL;

  pid                : ECMC_PID;
  target_pos         : LREAL;
  velocity_cmd       : LREAL;

  actual_pos_export  : LREAL;  // @epics prefix=$(IOC): rec=Main-PosActPid
  target_pos_export  : LREAL;  // @epics rec=Main-TargetPosPid
  velocity_cmd_export: LREAL;  // @epics rec=Main-VelCmdPid
  pid_error_export   : LREAL;  // @epics rec=Main-ErrPid
  pid_limited_export : BOOL;   // @epics rec=Main-Stat bit=0

  dbg_sample_trigger : BOOL;
  startup_printed    : BOOL;
  dbg_startup        : ECMC_DebugPrint;
  dbg_position       : ECMC_DebugPrint;
  dbg_target_low     : ECMC_DebugPrint;
  dbg_target_high    : ECMC_DebugPrint;
END_VAR

cycle_counter := cycle_counter + 1;
drive_control := 16#0001;

IF cycle_counter = 1 THEN
  target_high := TRUE;
END_IF;

IF target_high THEN
  target_pos := 12800.0;
ELSE
  target_pos := 0.0;
END_IF;

IF actual_position >= 12750 THEN
  target_high := FALSE;
ELSIF actual_position <= 50 THEN
  target_high := TRUE;
END_IF;

IF target_high THEN
  target_pos := 12800.0;
ELSE
  target_pos := 0.0;
END_IF;

pid(Setpoint := target_pos,
    Actual := INT_TO_LREAL(actual_position),
    Kp := 0.2,
    Ki := 0.002,
    DT := 1.0,
    OutMin := -1000.0,
    OutMax := 1000.0,
    IMin := -500.0,
    IMax := 500.0);

velocity_cmd := pid.Output;
velocity_setpoint := LREAL_TO_INT(velocity_cmd);

actual_pos_export := INT_TO_LREAL(actual_position);
target_pos_export := target_pos;
velocity_cmd_export := velocity_cmd;
pid_error_export := pid.Error;
pid_limited_export := pid.Limited;

dbg_startup(Execute := NOT startup_printed,
            Message := 'ST PID program started');
startup_printed := TRUE;

dbg_sample_trigger := (cycle_counter MOD 1000) = 0;
dbg_position(Execute := dbg_sample_trigger,
             Message := CONCAT('actual_position=',
                               TO_STRING(actual_position)));

dbg_target_high(Execute := actual_position <= 50,
                Message := 'PID target -> 12800');
dbg_target_low(Execute := actual_position >= 12750,
               Message := 'PID target -> 0');
END_PROGRAM
