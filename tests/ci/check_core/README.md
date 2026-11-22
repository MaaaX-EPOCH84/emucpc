# Fixtures of ci/check_core.sh

Each directory is a source tree laid out like the real one (`libs/<lib>/`),
used by `tests/ci/check_core_test.sh` to prove that each check of
`ci/check_core.sh` refuses what it must refuse and accepts what it must
accept. They are never part of the emulator.

- `good`: allowed includes only; built into `libemucpc_z80.a`, whose
  undefined symbols (operator new, the throw helpers of `std::vector`) must
  pass.
- `bad_include`: `<thread>` and `<cstdio>` in the core.
- `bad_quoted`: a quoted include that leaves the core (`libs/session`).
- `symbols`: no include at all, yet `fopen`, `__cxa_throw` and `time` are
  referenced: what an indirect include would let in.
