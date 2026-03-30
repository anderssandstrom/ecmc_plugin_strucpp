FUNCTION_BLOCK ECMC_PID
VAR_INPUT
  Enable   : BOOL := TRUE;
  Reset    : BOOL := FALSE;
  Setpoint : LREAL;
  Actual   : LREAL;
  FF       : LREAL := 0.0;
  Kp       : LREAL := 1.0;
  Ki       : LREAL := 0.0;
  Kd       : LREAL := 0.0;
  Kff      : LREAL := 1.0;
  DT       : LREAL := 1.0;
  OutMin   : LREAL := 0.0;
  OutMax   : LREAL := 0.0;
  IMin     : LREAL := 0.0;
  IMax     : LREAL := 0.0;
END_VAR
VAR_OUTPUT
  Error   : LREAL;
  Output  : LREAL;
  PPart   : LREAL;
  IPart   : LREAL;
  DPart   : LREAL;
  FFPart  : LREAL;
  Limited : BOOL;
END_VAR
VAR
  PrevError : LREAL;
END_VAR

IF (NOT Enable) OR Reset THEN
  Error := Setpoint - Actual;
  Output := 0.0;
  PPart := 0.0;
  IPart := 0.0;
  DPart := 0.0;
  FFPart := 0.0;
  Limited := FALSE;
  PrevError := Error;
ELSE
  Error := Setpoint - Actual;

  PPart := Error * Kp;
  IPart := IPart + Error * Ki * DT;

  IF IMax > IMin THEN
    IF IPart > IMax THEN
      IPart := IMax;
    END_IF;
    IF IPart < IMin THEN
      IPart := IMin;
    END_IF;
  END_IF;

  IF DT > 0.0 THEN
    DPart := (Error - PrevError) * Kd / DT;
  ELSE
    DPart := 0.0;
  END_IF;

  FFPart := FF * Kff;
  Output := PPart + IPart + DPart + FFPart;
  Limited := FALSE;

  IF OutMax > OutMin THEN
    IF Output > OutMax THEN
      Output := OutMax;
      Limited := TRUE;
    END_IF;
    IF Output < OutMin THEN
      Output := OutMin;
      Limited := TRUE;
    END_IF;
  END_IF;

  PrevError := Error;
END_IF;
END_FUNCTION_BLOCK
