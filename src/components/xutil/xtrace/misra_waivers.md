# xTrace MISRA C:2012 Waivers

All deviations from MISRA C:2012 required rules are documented here.
Corresponding rule numbers are suppressed in the SDK-root `misra.json`.

---

## Rule 15.5 - Single exit point per function

**Location:** `xtrace.c` - all public API entry functions and helpers use early return on guard checks (`NULL` pointer, `is_initialized` check, configuration limits like `capacity_bytes < 16U`).

**Justification:** Guard-clause style reduces nesting depth, makes error paths explicit, and simplifies code readability. Each early return is preceded by an explicit check; no resources are acquired before the guard block.

**Call sites:** `xTRACE_Init`, `xTRACE_Deinit`, `xTRACE_Flush`, `xTRACE_Get_Status`, `emit_start`.

---

## Rule 14.4 - Use of `while (0)` in `xTRACE_E*` no-op macros

**Location:** `xtrace.h` - disabled-trace compile-time no-op macros.

**Justification:** Standard `do { ... } while (0)` pattern ensures the macros expand as a single statement and behave correctly in conditional/control blocks without dangling `else` issues. Unused variables are safely suppressed via `(void)` casts. No behavioral concern.

**Call sites:** `xTRACE_E0`, `xTRACE_E1`, `xTRACE_E2`, `xTRACE_E3` when `xTRACE_ENABLE` is defined as `0`.
