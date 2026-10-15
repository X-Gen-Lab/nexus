# Physical platform entry model

The empty and GPIO-only authored assemblies exercise the actual generated
STM32F407ZG and GD32F470 lifecycle entry points. Existing reviewed silicon GPIO
routes are software model inputs. These fixtures add no PCB wiring or hardware
qualification claim.

Google targets compile actual generated bindings and GPIO providers against the
pinned SDK headers. Only the external system clock/identity boundary and CPU
privilege fact are modeled. Native Arch save/restore and mapped GPIO registers
remain real implementations. Rejected entry must precede any system call or
register effect, including retries after outer cleanup fails.
