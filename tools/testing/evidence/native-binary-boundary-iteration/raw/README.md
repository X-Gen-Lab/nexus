# Native CI binary admission delta

The two maintained files reuse the existing checked_binary helper. They do not
add an execution runner, source graph, or relaxed report conversion.

The first red.log attempt had a missing CMake PATH and imported TestCase discovery;
it is retained as fixture/setup failure, not behavior qualification. With the
prepared local tool PATH and fixture composition, red2.log ran 11 actual cases,
with four genuine expected failures: two filesystem alias admissions and two
actual Native main runs that erroneously executed/published success through aliases.
The original production source was unchanged throughout that run.

green.log executes 84 complete tests in seven maintained suites, including the
existing real CMake/CTest orchestration tests. All passed, none skipped, and the
two owned files plus unchanged shared helper stayed byte-identical throughout.
The root clean-source matrix and final normal hook remain separate executions.

No source/index/HEAD mutation was performed beyond the two authorized files;
no MCU/HIL equipment or safety outputs were exercised. Alias checking is a
trusted local-build boundary, not a filesystem race or formal verification claim.
