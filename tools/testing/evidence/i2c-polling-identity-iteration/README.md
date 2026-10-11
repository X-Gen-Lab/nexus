# Polling I2C pure controller identity development evidence

STM32 and GD32 each gained three genuine cold-assembly Google regressions.
They first failed against the actual polling providers with no `on_port`
method: STM32 17 cases/three failures and GD32 16 cases/three failures. After
adding the existing optional method slot, fresh full suites passed all 33
cases with no skip and the four owned source identities unchanged.

The predicates compare the exact controller method table/context and the
child's static controller/address association, accepting addresses 8 through
119. They do not inspect opened/initialized flags, read MMIO, lock, allocate,
modify state, or require startup. Cold tests have null hardware pointers,
verify exact/peer/foreign identities and byte-for-byte retained state, and
cover reserved-address and direct null argument rejection. The existing
private layouts and readonly table sizes do not change.

`manifest.json` binds original/stored raw bytes and final source capsules.
The Native generated factory regression and subsequent full Native execution
are separately owned and recorded in the I2C-pair iteration evidence. This
archive establishes software development behavior; root's final clean whole
candidate gate and physical I2C station execution remain separate.
