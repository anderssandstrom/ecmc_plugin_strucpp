FUNCTION_BLOCK ECMC_DebugPrint
VAR_INPUT
  Execute : BOOL;
  Message : STRING;
END_VAR
VAR_OUTPUT
  Fired : BOOL;
END_VAR
VAR
  PrevExecute : BOOL;
END_VAR
{external
  const bool execute = EXECUTE.get();
  FIRED.set(false);

  if (execute && !PREVEXECUTE.get()) {
    const auto message = MESSAGE.get();
    extern void ecmcStrucppDebugPrint(const char* message);
    ecmcStrucppDebugPrint(message.c_str());
    FIRED.set(true);
  }

  PREVEXECUTE.set(execute);
}
END_FUNCTION_BLOCK

FUNCTION ECMC_DebugPrintNow : BOOL
VAR_INPUT
  Message : STRING;
END_VAR
{external
  const auto message = MESSAGE.get();
  extern void ecmcStrucppDebugPrint(const char* message);
  ecmcStrucppDebugPrint(message.c_str());
  return true;
}
END_FUNCTION

FUNCTION ECMC_DebugPrintEnable : BOOL
VAR_INPUT
  Enable : BOOL;
END_VAR
{external
  extern void ecmcStrucppSetDebugPrintEnabled(int enable);
  ecmcStrucppSetDebugPrintEnabled(ENABLE.get() ? 1 : 0);
  return true;
}
END_FUNCTION
