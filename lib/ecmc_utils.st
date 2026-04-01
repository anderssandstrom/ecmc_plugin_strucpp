FUNCTION_BLOCK ECMC_DebounceBool
VAR_INPUT
  In       : BOOL;
  OnDelay  : TIME := T#0ms;
  OffDelay : TIME := T#0ms;
END_VAR
VAR_OUTPUT
  Out     : BOOL;
  Rising  : BOOL;
  Falling : BOOL;
END_VAR
VAR
  OnTimer  : TON;
  OffTimer : TON;
END_VAR

Rising := FALSE;
Falling := FALSE;

OnTimer(IN := In, PT := OnDelay);
OffTimer(IN := NOT In, PT := OffDelay);

IF In THEN
  IF (OnDelay <= T#0ms) OR OnTimer.Q THEN
    IF NOT Out THEN
      Rising := TRUE;
    END_IF;
    Out := TRUE;
  END_IF;
ELSE
  IF (OffDelay <= T#0ms) OR OffTimer.Q THEN
    IF Out THEN
      Falling := TRUE;
    END_IF;
    Out := FALSE;
  END_IF;
END_IF;
END_FUNCTION_BLOCK

FUNCTION ECMC_ApplyDeadband : LREAL
VAR_INPUT
  Value  : LREAL;
  Width  : LREAL := 0.0;
  Center : LREAL := 0.0;
END_VAR
VAR
  Delta : LREAL;
END_VAR

Delta := Value - Center;
IF Width <= 0.0 THEN
  ECMC_ApplyDeadband := Value;
ELSIF ABS(Delta) <= Width THEN
  ECMC_ApplyDeadband := Center;
ELSIF Delta > 0.0 THEN
  ECMC_ApplyDeadband := Center + (Delta - Width);
ELSE
  ECMC_ApplyDeadband := Center + (Delta + Width);
END_IF;
END_FUNCTION

FUNCTION ECMC_Clamp : LREAL
VAR_INPUT
  Value : LREAL;
  Min   : LREAL := 0.0;
  Max   : LREAL := 0.0;
END_VAR
VAR
  Lo : LREAL;
  Hi : LREAL;
END_VAR

Lo := Min;
Hi := Max;
IF Hi < Lo THEN
  Lo := Max;
  Hi := Min;
END_IF;

IF Hi > Lo THEN
  IF Value < Lo THEN
    ECMC_Clamp := Lo;
  ELSIF Value > Hi THEN
    ECMC_Clamp := Hi;
  ELSE
    ECMC_Clamp := Value;
  END_IF;
ELSE
  ECMC_Clamp := Value;
END_IF;
END_FUNCTION

FUNCTION ECMC_InWindow : BOOL
VAR_INPUT
  Value     : LREAL;
  Low       : LREAL;
  High      : LREAL;
  Inclusive : BOOL := TRUE;
END_VAR
VAR
  Lo : LREAL;
  Hi : LREAL;
END_VAR

Lo := Low;
Hi := High;
IF Hi < Lo THEN
  Lo := High;
  Hi := Low;
END_IF;

IF Inclusive THEN
  ECMC_InWindow := (Value >= Lo) AND (Value <= Hi);
ELSE
  ECMC_InWindow := (Value > Lo) AND (Value < Hi);
END_IF;
END_FUNCTION

FUNCTION ECMC_GetCycleTimeS : LREAL
{external
  extern double ecmcStrucppGetCycleTimeS();
  return ecmcStrucppGetCycleTimeS();
}
END_FUNCTION

FUNCTION ECMC_EpicsStarted : BOOL
{external
  extern int32_t ecmcStrucppGetEpicsStarted();
  return ecmcStrucppGetEpicsStarted() != 0;
}
END_FUNCTION

FUNCTION_BLOCK ECMC_StartupDelay
VAR_INPUT
  Enable : BOOL := TRUE;
  Delay  : TIME := T#500ms;
END_VAR
VAR_OUTPUT
  Ready  : BOOL;
  Rising : BOOL;
END_VAR
VAR
  DelayTimer : TON;
  PrevReady  : BOOL;
END_VAR

DelayTimer(IN := Enable, PT := Delay);
Ready := DelayTimer.Q;
Rising := Ready AND NOT PrevReady;
PrevReady := Ready;
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_RateLimiter
VAR_INPUT
  Enable      : BOOL := TRUE;
  Reset       : BOOL := FALSE;
  Input       : LREAL;
  RisingRate  : LREAL := 0.0;
  FallingRate : LREAL := 0.0;
  DT          : LREAL := 1.0;
  InitToInput : BOOL := TRUE;
END_VAR
VAR_OUTPUT
  Output  : LREAL;
  Limited : BOOL;
END_VAR
VAR
  Delta   : LREAL;
  MaxStep : LREAL;
END_VAR

IF Reset OR (NOT Enable) THEN
  IF InitToInput THEN
    Output := Input;
  END_IF;
  Limited := FALSE;
ELSE
  Delta := Input - Output;
  Limited := FALSE;

  IF (DT <= 0.0) THEN
    Output := Input;
  ELSIF Delta > 0.0 THEN
    IF RisingRate <= 0.0 THEN
      Output := Input;
    ELSE
      MaxStep := RisingRate * DT;
      IF Delta > MaxStep THEN
        Output := Output + MaxStep;
        Limited := TRUE;
      ELSE
        Output := Input;
      END_IF;
    END_IF;
  ELSIF Delta < 0.0 THEN
    IF FallingRate <= 0.0 THEN
      Output := Input;
    ELSE
      MaxStep := FallingRate * DT;
      IF (-Delta) > MaxStep THEN
        Output := Output - MaxStep;
        Limited := TRUE;
      ELSE
        Output := Input;
      END_IF;
    END_IF;
  END_IF;
END_IF;
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_FirstOrderFilter
VAR_INPUT
  Enable      : BOOL := TRUE;
  Reset       : BOOL := FALSE;
  Input       : LREAL;
  Tau         : LREAL := 0.0;
  DT          : LREAL := 1.0;
  InitToInput : BOOL := TRUE;
END_VAR
VAR_OUTPUT
  Output : LREAL;
END_VAR
VAR
  Alpha : LREAL;
END_VAR

IF Reset OR (NOT Enable) THEN
  IF InitToInput THEN
    Output := Input;
  END_IF;
ELSIF (Tau <= 0.0) OR (DT <= 0.0) THEN
  Output := Input;
ELSE
  Alpha := DT / (Tau + DT);
  Output := Output + Alpha * (Input - Output);
END_IF;
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_HysteresisBool
VAR_INPUT
  In        : LREAL;
  Low       : LREAL := 0.0;
  High      : LREAL := 0.0;
  Reset     : BOOL := FALSE;
  InitState : BOOL := FALSE;
END_VAR
VAR_OUTPUT
  Out     : BOOL;
  Rising  : BOOL;
  Falling : BOOL;
END_VAR
VAR
  Lo : LREAL;
  Hi : LREAL;
END_VAR

Lo := Low;
Hi := High;
IF Hi < Lo THEN
  Lo := High;
  Hi := Low;
END_IF;

Rising := FALSE;
Falling := FALSE;

IF Reset THEN
  IF Out <> InitState THEN
    Rising := InitState;
    Falling := NOT InitState;
  END_IF;
  Out := InitState;
ELSE
  IF NOT Out THEN
    IF In >= Hi THEN
      Out := TRUE;
      Rising := TRUE;
    END_IF;
  ELSE
    IF In <= Lo THEN
      Out := FALSE;
      Falling := TRUE;
    END_IF;
  END_IF;
END_IF;
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_Integrator
VAR_INPUT
  Enable : BOOL := TRUE;
  Reset  : BOOL := FALSE;
  Hold   : BOOL := FALSE;
  In     : LREAL;
  K      : LREAL := 1.0;
  DT     : LREAL := 1.0;
  Min    : LREAL := 0.0;
  Max    : LREAL := 0.0;
  Init   : LREAL := 0.0;
END_VAR
VAR_OUTPUT
  Out     : LREAL;
  Limited : BOOL;
END_VAR
VAR
  Candidate : LREAL;
  Lo        : LREAL;
  Hi        : LREAL;
END_VAR

Lo := Min;
Hi := Max;
IF Hi < Lo THEN
  Lo := Max;
  Hi := Min;
END_IF;

IF Reset THEN
  Out := Init;
  Limited := FALSE;
ELSIF Enable AND (NOT Hold) AND (DT > 0.0) THEN
  Candidate := Out + In * K * DT;
  Limited := FALSE;

  IF Hi > Lo THEN
    IF Candidate < Lo THEN
      Candidate := Lo;
      Limited := TRUE;
    ELSIF Candidate > Hi THEN
      Candidate := Hi;
      Limited := TRUE;
    END_IF;
  END_IF;

  Out := Candidate;
ELSE
  Limited := FALSE;
END_IF;
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_EcSlaveStatus
VAR_INPUT
  SlaveId  : INT;
  MasterId : INT := -1;
END_VAR
VAR_OUTPUT
  Valid       : BOOL;
  Online      : BOOL;
  Operational : BOOL;
  Init        : BOOL;
  PreOp       : BOOL;
  SafeOp      : BOOL;
  Op          : BOOL;
  StateCode   : BYTE;
  StateWord   : DWORD;
END_VAR
{external
  extern uint32_t ecmcStrucppGetEcSlaveStateWord(int32_t master_index, int32_t slave_index);
  const uint32_t word = ecmcStrucppGetEcSlaveStateWord(MASTERID.get(), SLAVEID.get());
  const uint32_t state_code = (word >> 3u) & 0xFu;

  STATEWORD.set(word);
  STATECODE.set(static_cast<uint8_t>(state_code));
  VALID.set((word & (1u << 0u)) != 0u);
  ONLINE.set((word & (1u << 1u)) != 0u);
  OPERATIONAL.set((word & (1u << 2u)) != 0u);
  INIT.set(state_code == 1u);
  PREOP.set(state_code == 2u);
  SAFEOP.set(state_code == 4u);
  OP.set(state_code == 8u);
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK ECMC_EcMasterStatus
VAR_INPUT
  MasterId : INT := -1;
END_VAR
VAR_OUTPUT
  Valid            : BOOL;
  LinkUp           : BOOL;
  Init             : BOOL;
  PreOp            : BOOL;
  SafeOp           : BOOL;
  Op               : BOOL;
  StateCode        : BYTE;
  SlavesResponding : UINT;
  StateWord        : DWORD;
END_VAR
{external
  extern uint32_t ecmcStrucppGetEcMasterStateWord(int32_t master_index);
  const uint32_t word = ecmcStrucppGetEcMasterStateWord(MASTERID.get());
  const uint32_t state_code = (word >> 2u) & 0xFu;

  STATEWORD.set(word);
  STATECODE.set(static_cast<uint8_t>(state_code));
  VALID.set((word & (1u << 0u)) != 0u);
  LINKUP.set((word & (1u << 1u)) != 0u);
  INIT.set(state_code == 1u);
  PREOP.set(state_code == 2u);
  SAFEOP.set(state_code == 4u);
  OP.set(state_code == 8u);
  SLAVESRESPONDING.set(static_cast<uint16_t>((word >> 16u) & 0xFFFFu));
}
END_FUNCTION_BLOCK

FUNCTION ECMC_AxisGetTrajSource : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisTrajSource(int32_t axis_index);
  return ecmcStrucppGetAxisTrajSource(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisGetEncSource : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisEncSource(int32_t axis_index);
  return ecmcStrucppGetAxisEncSource(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisGetActualPos : LREAL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern double ecmcStrucppGetAxisActualPos(int32_t axis_index);
  return ecmcStrucppGetAxisActualPos(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisGetSetpointPos : LREAL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern double ecmcStrucppGetAxisSetpointPos(int32_t axis_index);
  return ecmcStrucppGetAxisSetpointPos(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisGetActualVel : LREAL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern double ecmcStrucppGetAxisActualVel(int32_t axis_index);
  return ecmcStrucppGetAxisActualVel(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisGetSetpointVel : LREAL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern double ecmcStrucppGetAxisSetpointVel(int32_t axis_index);
  return ecmcStrucppGetAxisSetpointVel(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisIsEnabled : BOOL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisEnabled(int32_t axis_index);
  return ecmcStrucppGetAxisEnabled(AXISID.get()) != 0;
}
END_FUNCTION

FUNCTION ECMC_AxisIsBusy : BOOL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisBusy(int32_t axis_index);
  return ecmcStrucppGetAxisBusy(AXISID.get()) != 0;
}
END_FUNCTION

FUNCTION ECMC_AxisHasError : BOOL
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisError(int32_t axis_index);
  return ecmcStrucppGetAxisError(AXISID.get()) != 0;
}
END_FUNCTION

FUNCTION ECMC_AxisGetErrorId : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppGetAxisErrorId(int32_t axis_index);
  return ecmcStrucppGetAxisErrorId(AXISID.get());
}
END_FUNCTION

FUNCTION ECMC_AxisSetTrajSource : DINT
VAR_INPUT
  AxisId : DINT;
  Source : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppSetAxisTrajSource(int32_t axis_index, int32_t source);
  return ecmcStrucppSetAxisTrajSource(AXISID.get(), SOURCE.get());
}
END_FUNCTION

FUNCTION ECMC_AxisSetEncSource : DINT
VAR_INPUT
  AxisId : DINT;
  Source : DINT;
END_VAR
{external
  extern int32_t ecmcStrucppSetAxisEncSource(int32_t axis_index, int32_t source);
  return ecmcStrucppSetAxisEncSource(AXISID.get(), SOURCE.get());
}
END_FUNCTION

FUNCTION ECMC_AxisUseInternalTraj : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
  ECMC_AxisUseInternalTraj := ECMC_AxisSetTrajSource(AxisId := AxisId, Source := 0);
END_FUNCTION

FUNCTION ECMC_AxisUseExternalTraj : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
  ECMC_AxisUseExternalTraj := ECMC_AxisSetTrajSource(AxisId := AxisId, Source := 1);
END_FUNCTION

FUNCTION ECMC_AxisUseInternalEnc : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
  ECMC_AxisUseInternalEnc := ECMC_AxisSetEncSource(AxisId := AxisId, Source := 0);
END_FUNCTION

FUNCTION ECMC_AxisUseExternalEnc : DINT
VAR_INPUT
  AxisId : DINT;
END_VAR
  ECMC_AxisUseExternalEnc := ECMC_AxisSetEncSource(AxisId := AxisId, Source := 1);
END_FUNCTION

FUNCTION ECMC_AxisSetExternalSetpointPos : DINT
VAR_INPUT
  AxisId   : DINT;
  Position : LREAL;
END_VAR
{external
  extern int32_t ecmcStrucppSetAxisExtSetPos(int32_t axis_index, double value);
  return ecmcStrucppSetAxisExtSetPos(AXISID.get(), POSITION.get());
}
END_FUNCTION

FUNCTION ECMC_AxisSetExternalEncoderPos : DINT
VAR_INPUT
  AxisId   : DINT;
  Position : LREAL;
END_VAR
{external
  extern int32_t ecmcStrucppSetAxisExtActPos(int32_t axis_index, double value);
  return ecmcStrucppSetAxisExtActPos(AXISID.get(), POSITION.get());
}
END_FUNCTION
