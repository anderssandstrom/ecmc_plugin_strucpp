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
