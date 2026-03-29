PROGRAM MAIN
VAR
  actual_position    AT %IW0 : INT;   // @ecmc ec.s${SLAVE_ID=14}.positionActual${CH_ID}
  drive_control      AT %QW0 : WORD;  // @ecmc ec.s${SLAVE_ID=14}.driveControl${CH_ID}
  velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s${SLAVE_ID=14}.velocitySetpoint${CH_ID}
  cycle_counter      AT %MW0 : INT;   // @epics rec_suffix=Main-CycleCounterAct
  actual_pos_export          : INT;   // @epics rec_full=$(IOC):Main-PosActMini
END_VAR

actual_pos_export := actual_position;
cycle_counter := cycle_counter + 1;
drive_control := 16#0001;

IF actual_position < 0 THEN
  velocity_setpoint := 1000;
ELSIF actual_position > 1000 THEN
  velocity_setpoint := -1000;
END_IF;
END_PROGRAM
