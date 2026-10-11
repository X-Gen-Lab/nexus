# Physical platform entry iteration

These development Google executions compile actual generated STM32F407/GD32F470
empty and GPIO-only bindings with actual GPIO provider C and pinned SDK headers.
The external clock/identity boundary and CPU privilege fact are modeled. Native
Arch mask save/restore remains real. No PCB, SVC/MPU, HIL, cycles or release
qualification is implied.

The old branch ran 12 unfiltered cases per family: four passed and eight failed.
Uniform physical-family privilege admission then passed all 12 per family in
normal and ASan/UBSan executions. A final test-fixture-only change uses exact
non-replacing owned mappings; the final same 24 normal and 24 sanitizer cases
passed. Earlier source bytes and execution logs remain unchanged.

Generated binding strict C11 compiles are separately recorded: four units passed
Wall/Wextra/Werror/Wconversion/Wshadow. They do not constitute firmware link or
physical execution. Final source dependencies, declared resolved inputs, actual
CMake compile contexts and compressed binary identities are retained. Source
capsules belong to the recorded execution; later source changes need fresh gates.
