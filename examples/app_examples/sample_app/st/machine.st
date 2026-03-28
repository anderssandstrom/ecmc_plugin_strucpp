PROGRAM MACHINE
VAR
  start_button   AT %IX0.0 : BOOL;  // @ecmc ec0.s2.mm.inputDataArray01
  stop_button    AT %IX0.1 : BOOL;  // @ecmc ec0.s2.mm.inputDataArray01
  enable_output  AT %QX0.0 : BOOL;  // @ecmc ec0.s2.mm.outputDataArray01
  target_word    AT %QW2   : INT;   // @ecmc ec0.s2.mm.outputDataArray01
  counter        AT %MW0   : INT;   // @epics
  manual_target            : INT;   // @epics rw
END_VAR

IF stop_button THEN
  enable_output := FALSE;
ELSIF start_button THEN
  enable_output := TRUE;
END_IF;

IF enable_output THEN
  counter := counter + 1;
  IF manual_target <> 0 THEN
    target_word := manual_target;
  ELSE
    target_word := counter;
  END_IF;
ELSE
  target_word := 0;
END_IF;
END_PROGRAM
