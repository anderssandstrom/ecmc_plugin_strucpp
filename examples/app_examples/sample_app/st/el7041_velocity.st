PROGRAM EL7041_VELOCITY
VAR
  actual_position    AT %IW0 : INT;   // @ecmc ec.s14.positionActual01
  drive_control      AT %QW0 : WORD;  // @ecmc ec.s14.driveControl01
  velocity_setpoint  AT %QW2 : INT;   // @ecmc ec.s14.velocitySetpoint01
END_VAR

drive_control := 16#0001;
velocity_setpoint := 1000;
END_PROGRAM
