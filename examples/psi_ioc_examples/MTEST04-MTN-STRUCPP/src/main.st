PROGRAM MAIN
VAR
  actual_position    AT %IW0 : INT;   // @ecmc ec.s${SLAVE_ID=14}.positionActual${CH_ID}
  drive_control      AT %QW0 : WORD;  // @ecmc ec.s${SLAVE_ID=14}.driveControl${CH_ID}
  velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s${SLAVE_ID=14}.velocitySetpoint${CH_ID}
  cycle_counter      AT %MW0 : INT;   // @epics rec=Main-CycleCounterAct
  actual_pos_export          : INT;   // @epics prefix=$(IOC): rec=Main-PosActMini
  dbg_sample_trigger         : BOOL;
  startup_printed            : BOOL;
  dbg_startup                : ECMC_DebugPrint;
  dbg_position               : ECMC_DebugPrint;
  dbg_reverse_pos            : ECMC_DebugPrint;
  dbg_reverse_neg            : ECMC_DebugPrint;
END_VAR

actual_pos_export := actual_position;
cycle_counter := cycle_counter + 1;
drive_control := 16#0001;

dbg_startup(Execute := NOT startup_printed,
            Message := 'ST program started');
startup_printed := TRUE;

dbg_sample_trigger := (cycle_counter MOD 1000) = 0;
dbg_position(Execute := dbg_sample_trigger,
             Message := CONCAT('actual_position=',
                               TO_STRING(actual_position)));

IF velocity_setpoint = 0 THEN
  velocity_setpoint := 1000;
END_IF;

IF actual_position <= 0 THEN
  velocity_setpoint := 1000;
ELSIF actual_position >= 12800 THEN
  velocity_setpoint := -1000;
END_IF;

dbg_reverse_pos(Execute := actual_position <= 0,
                Message := CONCAT('reverse -> +1000 at pos=',
                                  TO_STRING(actual_position)));
dbg_reverse_neg(Execute := actual_position >= 12800,
                Message := CONCAT('reverse -> -1000 at pos=',
                                  TO_STRING(actual_position)));
END_PROGRAM
