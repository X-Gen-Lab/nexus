# Multiple-controller software fixtures

These assemblies select reviewed silicon pads for STM32F407ZGT6,
STM32F407VET6 and GD32F470ZGT6. Their Board packages are explicit software
contracts. PCB revisions, external device wiring, pulls, voltages and physical
qualification remain unknown.

Each part has six authored TOMLs. No test runner edits a backend or invents an
assembly:

| Suffix | Selected controller graph | Backend |
| --- | --- | --- |
| `.toml` | Two UART TX-DMA, two SPI DMA, one secondary I2C DMA | Bare metal |
| `-freertos.toml` | Same five independent controllers | FreeRTOS |
| `-i2c-pair.toml` | Two I2C controllers with finite message DMA | Bare metal |
| `-i2c-pair-freertos.toml` | Same I2C DMA pair | FreeRTOS |
| `-i2c-pair-irq.toml` | Two I2C controllers with finite message IRQ | Bare metal |
| `-i2c-pair-irq-freertos.toml` | Same I2C IRQ pair | FreeRTOS |

The secondary UART TX engine and the primary I2C TX engine both use physical
stream 6 in DMA1 on STM32 and DMA0 on GD32. The resolver rejects selecting those
owners together, even though their request selectors differ. The five-controller
graph therefore selects the secondary I2C controller. Same-address I2C endpoints
on two distinct buses retain separate identities and contexts.

The CMake fixture consumes `NEXUS_SOURCE_ROOT` and one explicit
`NEXUS_ASSEMBLY_FILE`, then calls the maintained `nexus_add_firmware` function.
Its main retains every selected public controller and endpoint face through the
typed factory. It initiates no transfer and does not establish physical device
behavior. The post-link observer checks actual ELF contexts and readonly
operations tables; the maintained provider-assembly runner also verifies budgets
and source/configuration identities.

Generated cold assembly owns the selected clock, pin and NVIC leases. A failed
constructor participates in reverse cleanup. Successful source drain and NVIC
quiescence precede pin restoration; every active-low CS output observes an
inactive level before any pin in its controller group is restored. A refused
withdrawal keeps the lease and blocks restart until explicit cleanup succeeds.
Ordinary I/O calls still invoke the public face's shared provider table directly.
