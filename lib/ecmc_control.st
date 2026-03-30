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
  DFilterTau : LREAL := 0.0;
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
  PrevError    : LREAL;
  DState       : LREAL;
  WasEnabled   : BOOL;
  CandidateI   : LREAL;
  DRaw         : LREAL;
  DAlpha       : LREAL;
  UnsatOutput  : LREAL;
END_VAR

Error := Setpoint - Actual;

IF Reset THEN
  Output := 0.0;
  PPart := 0.0;
  IPart := 0.0;
  DPart := 0.0;
  FFPart := 0.0;
  Limited := FALSE;
  PrevError := Error;
  DState := 0.0;
  WasEnabled := FALSE;
ELSIF NOT Enable THEN
  Error := Setpoint - Actual;
  Output := 0.0;
  PPart := 0.0;
  DPart := 0.0;
  FFPart := FF * Kff;
  Limited := FALSE;
  PrevError := Error;
  DState := 0.0;
  WasEnabled := FALSE;
ELSIF NOT WasEnabled THEN
  PPart := Error * Kp;
  DPart := 0.0;
  FFPart := FF * Kff;
  UnsatOutput := PPart + IPart + FFPart;
  Output := UnsatOutput;
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
  DState := 0.0;
  WasEnabled := TRUE;
ELSE
  PPart := Error * Kp;
  CandidateI := IPart + Error * Ki * DT;

  IF IMax > IMin THEN
    IF CandidateI > IMax THEN
      CandidateI := IMax;
    END_IF;
    IF CandidateI < IMin THEN
      CandidateI := IMin;
    END_IF;
  END_IF;

  IF DT > 0.0 THEN
    DRaw := (Error - PrevError) / DT;
    IF DFilterTau > 0.0 THEN
      DAlpha := DT / (DFilterTau + DT);
      DState := DState + DAlpha * (DRaw - DState);
    ELSE
      DState := DRaw;
    END_IF;
    DPart := Kd * DState;
  ELSE
    DState := 0.0;
    DPart := 0.0;
  END_IF;

  FFPart := FF * Kff;
  UnsatOutput := PPart + CandidateI + DPart + FFPart;
  Output := UnsatOutput;
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

  IF (NOT Limited) OR
     ((UnsatOutput > OutMax) AND (Error < 0.0)) OR
     ((UnsatOutput < OutMin) AND (Error > 0.0)) THEN
    IPart := CandidateI;
  END_IF;

  PrevError := Error;
  WasEnabled := TRUE;
END_IF;
END_FUNCTION_BLOCK
